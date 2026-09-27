#!/usr/bin/env python3
"""Check that GCC still inlines the helpers the hot paths depend on.

The C extension is one translation unit that sits at GCC's inlining
budget, so a change anywhere in it can push a helper out of line
somewhere else.  Each time that happened the benchmarks regressed on
code nobody touched: ``md_calc_identity()`` fell out of ``__getitem__``
and ``__delitem__`` when the shell pools landed, ``md_contains()`` fell
out of ``keys().isdisjoint()`` when the ASCII identity path landed, and
``md_next()`` fell out of the items iterator when a cold leg of
``md_get_one()`` was outlined.

This script compiles ``_multidict.c`` the way ``pip install`` does, with
the interpreter's own compiler flags plus the release flags from
``setup.py``, disassembles the object, and checks every rule in
``RULES`` below.  A rule names a helper and the functions that must not
contain a call or tail call to it, under any clone GCC makes of either
(``.isra.0``, ``.part.0``, ``.constprop.0``).  A call made from a
``.cold`` partition does not count, since that code is off the hot path
by definition.  A caller that GCC inlined everywhere passes, since it
has no copy of its own left to make the call from.  ``"*"`` in place of
the callers means no out-of-line copy of the helper may exist at all.

Typical use, against both builds::

    tools/check_inlining.py \\
        --python ~/.pyenv/versions/3.14.7/bin/python3 \\
        --python ~/.pyenv/versions/3.14.7t/bin/python3.14t

To see what a function calls out of line today, when writing a rule or
reading a failure, pass ``--show``::

    tools/check_inlining.py --show multidict_get --show md_get_one

Rules are about direct calls only.  If ``multidict_get()`` calls
``md_get_one()`` out of line and ``md_get_one()`` has
``md_calc_identity()`` inlined, a rule forbidding ``md_calc_identity``
in ``multidict_get`` passes; name ``md_get_one`` as a caller too where
that matters.  Rules describe what the current code achieves on the
compilers CI uses.  A fix for a new inlining regression, or a helper
pinned with ``ALWAYS_INLINE`` or ``NOINLINE`` for speed, adds a rule in
the same change; a regression that is accepted on purpose edits its rule
there too, with the measurement that justified it.
"""

import argparse
import re
import shlex
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

EXTENSION_SOURCE = "multidict/_multidict.c"

# Release flags from setup.py, minus the warning options: this build is
# only ever disassembled, and warnings do not affect code generation.
SETUP_CFLAGS = ["-O3", "-DNDEBUG", "-std=c11", "-fno-strict-aliasing"]

GIL = "gil"
FT = "ft"
BOTH = (GIL, FT)

GETITEM_ENTRIES = (
    "multidict_get",
    "multidict_getone",
    "multidict_mp_subscript",
    "multidict_proxy_get",
    "multidict_proxy_getone",
    "multidict_proxy_mp_subscript",
)

ITERNEXT_ENTRIES = (
    "multidict_items_iter_tp_iternext",
    "multidict_keys_iter_tp_iternext",
    "multidict_values_iter_tp_iternext",
)


@dataclass(frozen=True)
class Rule:
    helper: str
    callers: tuple[str, ...] | str
    builds: tuple[str, ...]
    why: str


RULES = (
    Rule(
        "md_calc_identity",
        (
            *GETITEM_ENTRIES,
            "multidict_getall",
            "multidict_add",
            "multidict_setdefault",
            "md_del",
            "md_replace",
            "md_pop_one",
            "md_update_from_dict",
            "md_update_from_seq",
            "multidict_keysview_isdisjoint",
        ),
        BOTH,
        "#1541/#1542: out of line in get and delitem cost 3.6% and 4%",
    ),
    Rule(
        "md_calc_identity",
        ("md_get_one", "md_contains"),
        (FT,),
        "#1541/#1542: md_get_one and md_contains stay out of line on FT",
    ),
    Rule(
        "md_get_one",
        GETITEM_ENTRIES,
        (GIL,),
        "__getitem__ and get() pay a call per lookup",
    ),
    Rule(
        "md_get_all",
        ("multidict_getall",),
        BOTH,
        "#1530: a stack buffer over 256 bytes cost getall 2%",
    ),
    Rule(
        "md_contains",
        ("multidict_keysview_isdisjoint",),
        (GIL,),
        "#1527: keys().isdisjoint() +31% on a case-sensitive MultiDict",
    ),
    Rule(
        "md_next",
        (*ITERNEXT_ENTRIES, "multidict_keysview_isdisjoint"),
        BOTH,
        "#1601 prototype: items iteration +19% on FT",
    ),
    Rule(
        "md_clear",
        ("multidict_clear",),
        (FT,),
        "#1601 prototype: clear() +13% on FT",
    ),
    Rule(
        "htkeysiter_init",
        "*",
        BOTH,
        "#1554: a call on every probe, get_miss -23% once inlined",
    ),
    Rule(
        "_md_del_at",
        "*",
        BOTH,
        "#1604: pinned after budget respend, popitem +1% on FT",
    ),
    Rule(
        "htkeys_set_index",
        "*",
        BOTH,
        "#1604/#1605: pinned after budget respend, ctor +60 Ir on FT",
    ),
    Rule(
        "_multidict_tp_init",
        "*",
        BOTH,
        "#1591: one shared copy made subclass init 7-8% slower",
    ),
)

FUNC_RE = re.compile(r"^[0-9a-f]+ <(.+)>:$")
TARGET_RE = re.compile(r"<([A-Za-z_][A-Za-z_0-9.]*?)(?:@plt)?(?:[+-]0x[0-9a-f]+)?>")
CLONE_SUFFIX_RE = re.compile(r"(\.(isra|part|constprop|lto_priv|cold)(\.\d+)?)+$")


def base_name(symbol):
    return CLONE_SUFFIX_RE.sub("", symbol)


def is_cold(symbol):
    return ".cold" in symbol


def run(cmd, **kwargs):
    return subprocess.run(
        [str(part) for part in cmd],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        **kwargs,
    ).stdout


def interpreter_config(python):
    out = run(
        [
            python,
            "-c",
            "; ".join(
                (
                    "import sysconfig as s",
                    "print(s.get_config_var('CC'))",
                    "print(s.get_config_var('CFLAGS') or '')",
                    "print(s.get_config_var('CCSHARED') or '')",
                    "print(s.get_path('include'))",
                    "print(int(bool(s.get_config_var('Py_GIL_DISABLED'))))",
                )
            ),
        ]
    ).splitlines()
    cc, cflags, ccshared, include, ft = out
    return (
        shlex.split(cc),
        shlex.split(cflags) + shlex.split(ccshared),
        include,
        ft == "1",
    )


def build_object(root, python, cc, out):
    default_cc, cflags, include, ft = interpreter_config(python)
    cmd = [
        *(shlex.split(cc) if cc else default_cc),
        "-c",
        *cflags,
        *SETUP_CFLAGS,
        "-w",
        "-I",
        include,
        "-I",
        root / "multidict",
        root / EXTENSION_SOURCE,
        "-o",
        out,
    ]
    result = subprocess.run(
        [str(part) for part in cmd],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    if result.returncode:
        print(result.stdout, end="", file=sys.stderr)
        sys.exit(f"build failed with {python}")
    return ft


def call_graph(obj):
    """Map each function symbol to the set of symbols it branches to."""
    graph = {}
    current = None
    for line in run(["objdump", "-d", "--no-show-raw-insn", obj]).splitlines():
        match = FUNC_RE.match(line)
        if match:
            current = graph.setdefault(match.group(1), set())
        elif current is not None:
            current.update(TARGET_RE.findall(line))
    return graph


def out_of_line_callees(graph, caller):
    """Callees of every non-cold clone of ``caller``, by base name."""
    clones = [s for s in graph if base_name(s) == caller and not is_cold(s)]
    if not clones:
        return None
    callees = {}
    for clone in clones:
        for target in graph[clone]:
            if base_name(target) != caller:
                callees.setdefault(base_name(target), set()).add(target)
    return callees


def defined_in_source(root, name):
    pattern = re.compile(rf"^{re.escape(name)}\(", re.MULTILINE)
    sources = [root / EXTENSION_SOURCE, *(root / "multidict/_multilib").glob("*.h")]
    return any(pattern.search(path.read_text()) for path in sources)


def check(root, graph, build):
    failures = []
    for rule in RULES:
        if build not in rule.builds:
            continue
        # A rule naming a helper that no longer exists passes forever,
        # so a rename has to fail here until the rule follows it.
        if not defined_in_source(root, rule.helper):
            failures.append(f"{rule.helper}: not defined in the sources; stale rule?")
            continue
        if rule.callers == "*":
            copies = sorted(
                s for s in graph if base_name(s) == rule.helper and not is_cold(s)
            )
            if copies:
                failures.append(
                    f"{rule.helper}: out-of-line copy {', '.join(copies)}"
                    f"\n      ({rule.why})"
                )
            continue
        for caller in rule.callers:
            callees = out_of_line_callees(graph, caller)
            if callees is None:
                # Inlined into every caller of its own, which leaves no
                # copy to call the helper from.
                if not defined_in_source(root, caller):
                    failures.append(
                        f"{caller}: not defined in the sources; stale rule?"
                    )
            elif rule.helper in callees:
                failures.append(
                    f"{caller} calls {', '.join(sorted(callees[rule.helper]))}"
                    f"\n      ({rule.why})"
                )
    return failures


def show(graph, name):
    callees = out_of_line_callees(graph, name)
    if callees is None:
        print(f"  {name}: not found (inlined everywhere?)")
        return
    local = sorted(c for c in callees if any(base_name(s) == c for s in graph))
    print(f"  {name} -> {', '.join(local) or '(no local calls)'}")


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--python",
        action="append",
        metavar="PATH",
        help="interpreter whose headers and compiler flags to build with; "
        "repeat it to cover a GIL build and a free-threaded one "
        "(default: this one)",
    )
    parser.add_argument(
        "--cc",
        metavar="CMD",
        help="compiler to use instead of the interpreter's own, for example gcc-14",
    )
    parser.add_argument(
        "--show",
        action="append",
        metavar="FUNCTION",
        help="print the local functions FUNCTION calls out of line, "
        "instead of checking the rules",
    )
    args = parser.parse_args()

    root = Path(__file__).resolve().parent.parent
    failed = False
    with tempfile.TemporaryDirectory(prefix="check-inlining-") as tmp:
        for python in args.python or [sys.executable]:
            obj = Path(tmp) / "_multidict.o"
            build = FT if build_object(root, python, args.cc, obj) else GIL
            graph = call_graph(obj)
            print(f"{python} ({build.upper()}):")
            if args.show:
                for name in args.show:
                    show(graph, name)
                continue
            failures = check(root, graph, build)
            for failure in failures:
                print(f"  {failure}")
            if not failures:
                applicable = sum(build in rule.builds for rule in RULES)
                print(f"  ok, {applicable} rules")
            failed = failed or bool(failures)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
