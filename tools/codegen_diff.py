#!/usr/bin/env python3
"""Report which functions GCC emits differently before and after a change.

A refactor of the C extension that is meant to change no behavior is
easiest to trust when the emitted code is unchanged, but a raw diff of
``objdump`` output is useless: one instruction-length change shifts every
later address and label, so untouched functions look modified.  This
script normalizes that away and compares function by function.

Typical use, on a branch, against both builds::

    tools/codegen_diff.py --ref master \\
        --python ~/.pyenv/versions/3.13.2/bin/python3 \\
        --python ~/.pyenv/versions/3.14.7t/bin/python3.14t

Some difference is normal and does not mean the change is wrong.
Shrinking a ``static inline`` helper frees inlining budget, so GCC may
inline something else nearby and perturb a function the change never
touched.  The per-function report prints the instruction count and
whether the set of called symbols moved, which separates the two cases:
an unchanged count with unchanged call targets is register allocation and
block layout, while a changed set of call targets is worth reading.

A helper that GCC always inlines has no standalone copy to compare.  To
look at one on its own, write a small translation unit that includes the
headers and wraps it, and pass that with ``--probe``::

    #include <Python.h>
    #include "_multilib/hashtable.h"

    __attribute__((noinline, used)) int
    probe(MultiDictObject* md, Py_hash_t h, PyObject* i, PyObject* k,
          PyObject* v)
    {
        return _md_add_with_hash_steal_refs(md, h, i, k, v);
    }

That isolates the helper, but by construction it also hides the inlining
interaction above, so use it alongside the whole-module report and not
instead of it.
"""

import argparse
import difflib
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

EXTENSION_SOURCE = "multidict/_multidict.c"

# Release flags from setup.py, minus the warning options: this build is
# only ever disassembled, and -Werror would fail it for a warning that
# the real build has already accepted.
CFLAGS = [
    "-c",
    "-fPIC",
    "-O3",
    "-DNDEBUG",
    "-std=c11",
    "-fno-strict-aliasing",
    "-w",
]

FUNC_RE = re.compile(r"^[0-9a-f]+ <(.+)>:$")
CALL_RE = re.compile(r"<([A-Za-z_.][A-Za-z_.0-9]*)(?:\+OFF)?>")

SUBSTITUTIONS = (
    # Address column at the start of every instruction line.
    (re.compile(r"^\s*[0-9a-f]+:\s*"), ""),
    # RIP-relative displacements move with the function.
    (re.compile(r"0x[0-9a-f]+\(%rip\)"), "RIP"),
    # Branch and call targets: the offset inside the symbol shifts as
    # soon as any earlier instruction changes length.
    (re.compile(r"<([A-Za-z_.][A-Za-z_.0-9]*)\+0x[0-9a-f]+>"), r"<\1+OFF>"),
    (re.compile(r"\b[0-9a-f]+ <([A-Za-z_.][A-Za-z_.0-9]*(?:\+OFF)?)>"), r"TGT <\1>"),
    (re.compile(r"\$0x[0-9a-f]+"), "IMM"),
    (re.compile(r"0x[0-9a-f]+"), "HEX"),
    (re.compile(r"\s+$"), ""),
)


def run(cmd, text=True, **kwargs):
    return subprocess.run([str(part) for part in cmd], check=True, text=text, **kwargs)


def export_tree(ref, dest):
    """Write the tree of ``ref`` into ``dest``."""
    dest.mkdir(parents=True, exist_ok=True)
    archive = run(
        ["git", "archive", "--format=tar", ref],
        text=False,
        stdout=subprocess.PIPE,
    ).stdout
    run(["tar", "-x", "-C", dest], text=False, input=archive)


def include_dir(python):
    return run(
        [python, "-c", "import sysconfig; print(sysconfig.get_path('include'))"],
        stdout=subprocess.PIPE,
    ).stdout.strip()


def build_object(tree, python, source, out, keep_static):
    flags = list(CFLAGS)
    if keep_static:
        flags.append("-fkeep-static-functions")
    # The package directory is on the include path so that a --probe file
    # kept outside the tree can still say #include "_multilib/....h".
    run(
        [
            "gcc",
            *flags,
            "-I",
            include_dir(python),
            "-I",
            tree,
            "-I",
            Path(tree) / "multidict",
            source,
            "-o",
            out,
        ],
        cwd=tree,
    )


def disassemble(obj):
    return run(
        ["objdump", "-d", "--no-show-raw-insn", obj], stdout=subprocess.PIPE
    ).stdout.splitlines()


def split_functions(lines):
    """Group normalized instructions by the function they belong to."""
    functions = {}
    current = None
    for line in lines:
        match = FUNC_RE.match(line)
        if match:
            current = functions.setdefault(match.group(1), [])
            continue
        if current is None:
            continue
        for pattern, replacement in SUBSTITUTIONS:
            line = pattern.sub(replacement, line)
        if line:
            current.append(line)
    return functions


def call_targets(instructions):
    targets = set()
    for line in instructions:
        targets.update(CALL_RE.findall(line))
    return targets


def changed_lines(before, after):
    return sum(
        1
        for line in difflib.unified_diff(before, after, n=0)
        if line[:1] in "+-" and line[:3] not in ("+++", "---")
    )


def describe(name, before, after):
    note = "call targets unchanged"
    if call_targets(before) != call_targets(after):
        note = "CALL TARGETS DIFFER"
    return (
        f"  {name}\n"
        f"      {len(before)} -> {len(after)} instructions, "
        f"{changed_lines(before, after)} changed lines, {note}"
    )


def compare(before_obj, after_obj):
    before = split_functions(disassemble(before_obj))
    after = split_functions(disassemble(after_obj))

    report = []
    for name in sorted(set(before) - set(after)):
        report.append(f"  {name}\n      gone")
    for name in sorted(set(after) - set(before)):
        report.append(f"  {name}\n      new")
    for name in sorted(set(before) & set(after)):
        if before[name] != after[name]:
            report.append(describe(name, before[name], after[name]))
    return len(before), report


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--ref",
        default="HEAD^",
        help="baseline to compare the working tree against "
        "(default: %(default)s; a merge base with master is usually "
        "what you want on a branch)",
    )
    parser.add_argument(
        "--python",
        action="append",
        metavar="PATH",
        help="interpreter whose headers to build against; repeat it to "
        "cover a GIL build and a free-threaded one (default: this one)",
    )
    parser.add_argument(
        "--probe",
        metavar="FILE.c",
        help="compile this translation unit instead of the extension, to "
        "look at an always-inlined helper on its own (see the module "
        "docstring)",
    )
    parser.add_argument(
        "--keep-static",
        action="store_true",
        help="pass -fkeep-static-functions, which emits a standalone copy "
        "of some helpers that are otherwise only visible inlined",
    )
    parser.add_argument(
        "--fail-on-diff",
        action="store_true",
        help="exit non-zero when any function differs",
    )
    args = parser.parse_args()

    if shutil.which("objdump") is None:
        sys.exit("objdump not found; install binutils")

    root = Path(
        run(
            ["git", "rev-parse", "--show-toplevel"], stdout=subprocess.PIPE
        ).stdout.strip()
    )
    pythons = args.python or [sys.executable]

    probe = Path(args.probe).resolve() if args.probe else None
    if probe is not None and not probe.is_file():
        sys.exit(f"{args.probe}: no such file")

    differed = False
    with tempfile.TemporaryDirectory(prefix="codegen-diff-") as tmp:
        tmp = Path(tmp)
        baseline = tmp / "baseline"
        export_tree(args.ref, baseline)

        for python in pythons:
            objects = {}
            for label, tree in (("before", baseline), ("after", root)):
                source = str(probe) if probe else tree / EXTENSION_SOURCE
                objects[label] = tmp / f"{label}.o"
                build_object(tree, python, source, objects[label], args.keep_static)

            total, report = compare(objects["before"], objects["after"])
            print(f"{python}: {len(report)} of {total} functions differ")
            print("\n".join(report))
            differed = differed or bool(report)

    if differed and args.fail_on_diff:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
