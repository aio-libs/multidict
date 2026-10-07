#!/usr/bin/env python3
"""Check that GCC still inlines the helpers the hot paths depend on.

The C extension is one translation unit that sits at GCC's inlining
budget, so a change anywhere in it can push a helper out of line
somewhere else.  Each time that happened the benchmarks regressed on
code nobody touched: ``md_calc_identity()`` fell out of ``__getitem__``
and ``__delitem__`` when the shell pools landed, ``md_contains()`` fell
out of ``keys().isdisjoint()`` when the ASCII identity path landed, and
the iterator step fell out of the items iterator when a cold leg of
``md_get_one()`` was outlined.

This script compiles ``_multidict.c`` the way ``pip install`` does, with
the interpreter's own compiler flags plus the release flags from
``setup.py``, disassembles the object, and checks every rule in
``RULES`` below.  A rule names a helper and the entry points that must
not reach it through an out-of-line call or tail call, under any clone
GCC makes (``.isra.0``, ``.part.0``, ``.constprop.0``).  The check is
transitive: if ``multidict_get()`` calls ``md_get_one()`` out of line and
``md_get_one()`` calls ``md_calc_identity()`` out of line, a rule
forbidding ``md_calc_identity`` from ``multidict_get`` fails, and the
report shows the chain.  Calls from a ``.cold`` partition are not
followed, since that code is off the hot path by definition.  ``"*"`` in
place of the entry points means no out-of-line copy of the helper may
exist at all.

Entry points are the functions Python or a C API client calls through a
pointer: type slots, methods and ``MultiDict_*`` C API functions.  GCC
always keeps a copy of those, so a rule naming anything else fails the
moment GCC inlines it into its callers, rather than going quiet.

Typical use, against both builds::

    tools/check_inlining.py \\
        --python ~/.pyenv/versions/3.14.7/bin/python3 \\
        --python ~/.pyenv/versions/3.14.7t/bin/python3.14t

To see what a function calls out of line today, when writing a rule or
reading a failure, pass ``--show``::

    tools/check_inlining.py --show multidict_get --show MultiDict_GetItem

Rules describe what the current code achieves on the compilers CI uses.
A fix for a new inlining regression, or a helper pinned with
``ALWAYS_INLINE`` or ``NOINLINE`` for speed, adds a rule in the same
change; a regression that is accepted on purpose edits its rule there
too, with the measurement that justified it.
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
    "MultiDict_GetItem",
)

GETALL_ENTRIES = (
    "multidict_getall",
    "multidict_proxy_getall",
)

# Every entry point that takes a key.
KEY_ENTRIES = (
    *GETITEM_ENTRIES,
    *GETALL_ENTRIES,
    "multidict_sq_contains",
    "multidict_proxy_sq_contains",
    "multidict_mp_ass_subscript",
    "multidict_add",
    "multidict_setdefault",
    "multidict_pop",
    "multidict_popone",
    "multidict_popall",
    "multidict_update",
    "multidict_extend",
    "multidict_tp_init",
    "MultiDict_Contains",
    "MultiDict_Add",
    "MultiDict_SetItem",
    "MultiDict_SetDefault",
    "MultiDict_DelItem",
    "MultiDict_Pop",
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
        KEY_ENTRIES,
        BOTH,
        "#1541/#1542: out of line in get and delitem cost 3.6% and 4%; "
        "#1614: out of line in getall on FT with GCC 13",
    ),
    Rule(
        "_iter_next_entry",
        ITERNEXT_ENTRIES,
        BOTH,
        "#1601 prototype: items iteration +19% on FT; ALWAYS_INLINE in "
        "#1627/#1628, where GCC 15 on FT dropped it again (+19%); plain "
        "inline since #1732",
    ),
    Rule(
        "_md_last_live",
        "*",
        BOTH,
        "#1674: an out-of-line copy loses the constant kind, which cost "
        "3-5 Ir per popitem()",
    ),
    Rule(
        "entry_next",
        "*",
        BOTH,
        "#1676: the step of every per-kind pointer walk; out of line it "
        "loses the constant entry size",
    ),
    Rule(
        "_md_copy_live",
        "*",
        BOTH,
        "#1676: the constant kind made adding to a table with holes 5-8% cheaper",
    ),
    Rule(
        "entry_is_hole",
        "*",
        BOTH,
        "#1674: a single load tested once per entry in every walk",
    ),
    Rule(
        "_md_parse_item",
        "*",
        BOTH,
        "#1644: six copies of the bulk update took it out of line, "
        "37 Ir per constructor item",
    ),
    Rule(
        "_multidict_vectorcall_impl",
        "*",
        BOTH,
        "out of line once the constructor was pinned, cls() +5.5% on FT",
    ),
    Rule(
        "md_calc_key",
        "*",
        BOTH,
        "an out-of-line copy adds a call per key that popitem() and "
        "update() from another class materialize",
    ),
    Rule(
        "md_borrow_identity",
        "*",
        BOTH,
        "borrowing the lookup identity saved 10 Ir per istr d[key] on "
        "CIMultiDict and 15 per MultiDict d[key] on GIL",
    ),
    Rule(
        "unpack_pair",
        "*",
        BOTH,
        "per-class bulk updates took it out of line, 18 Ir per "
        "MultiDict(items) item on FT",
    ),
    Rule(
        "md_add_with_hash_steal_refs",
        "*",
        BOTH,
        "#1644: per-kind layouts pushed it out of the constructor's loop, "
        "CIMultiDict(items) +10%",
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
        "htkeys_get_index",
        "*",
        BOTH,
        "#1651: per-class getall took it out of htkeysiter_init on FT, 4 Ir per d[key]",
    ),
    Rule(
        "htkeys_set_index",
        "*",
        BOTH,
        "#1604/#1605: pinned after budget respend, ctor +60 Ir on FT",
    ),
    Rule(
        "_compact_entry_matches",
        "*",
        (FT,),
        "#1653: out of line, MultiDict d[key] +6.9% and CIMultiDict "
        "key in d +6.5% on FT",
    ),
    Rule(
        "_full_entry_matches",
        "*",
        (FT,),
        "#1653: pinned with _compact_entry_matches, its twin for the "
        "full layout in the same lock-free probe loops",
    ),
    Rule(
        "_key_to_identity_cs",
        "*",
        (FT,),
        "#1675: out of line in d[key] = v once update() and merge() lost "
        "their per-class copies, and with it del d[key] +3% on CIMultiDict",
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


def reachable(graph, caller):
    """Map each function ``caller`` reaches through out-of-line calls to
    the shortest chain of calls that gets there, by base name.

    Cold partitions are not followed, since the code in them is off the
    hot path by definition.  Returns None when ``caller`` has no copy of
    its own in the object.
    """
    frontier = [s for s in graph if base_name(s) == caller and not is_cold(s)]
    if not frontier:
        return None
    paths = {caller: [caller]}
    while frontier:
        next_frontier = []
        for symbol in frontier:
            for target in sorted(graph[symbol]):
                name = base_name(target)
                if target not in graph or is_cold(target) or name in paths:
                    continue
                paths[name] = [*paths[base_name(symbol)], target]
                next_frontier.extend(
                    s for s in graph if base_name(s) == name and not is_cold(s)
                )
        frontier = next_frontier
    del paths[caller]
    return paths


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
            paths = reachable(graph, caller)
            if paths is None:
                # A slot, method or C API function is referenced by address
                # and always keeps a copy, so a caller that has none is not
                # an entry point: its calls now live in functions the rule
                # does not name.
                if defined_in_source(root, caller):
                    failures.append(
                        f"{caller}: inlined into its callers; name the entry "
                        f"points that reach it instead"
                    )
                else:
                    failures.append(
                        f"{caller}: not defined in the sources; stale rule?"
                    )
            elif rule.helper in paths:
                failures.append(
                    f"{' -> '.join(paths[rule.helper])}\n      ({rule.why})"
                )
    return failures


def show(graph, name):
    paths = reachable(graph, name)
    if paths is None:
        print(f"  {name}: no copy of its own (inlined everywhere?)")
        return
    print(f"  {name} reaches:")
    for callee in sorted(paths):
        print(f"      {' -> '.join(paths[callee][1:])}")
    if not paths:
        print("      (no local calls)")


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
        help="print every local function FUNCTION reaches through "
        "out-of-line calls, instead of checking the rules",
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
