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

Only fields that move when code is relocated are normalized: the
address column, RIP-relative displacements, and branch or call offsets
inside a symbol.  Immediate operands and memory displacements are left
alone, because those carry constants and structure offsets, and folding
them together would report "no difference" for a store that moved to
another field.  ``--self-test`` checks that.

Expect several functions to be reported even for a change that is
semantically neutral.  A helper that gets smaller frees inlining budget,
and a stack slot that moves changes a displacement, neither of which is
a behavior change.  The report carries the two numbers that separate
those from a real one: an unchanged instruction count together with an
unchanged set of called symbols is register allocation and block layout,
while a changed set of call targets is worth reading.

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
RELOC_RE = re.compile(r"^R_[A-Z0-9_]+\s+(\S+?)(?:[+-]0x[0-9a-f]+)?$")

# Only fields that move when code is relocated are normalised.  Immediate
# operands and memory displacements are left alone on purpose: they carry
# constants and structure offsets, so folding them together would report
# "no difference" for a store that moved to another field.
SUBSTITUTIONS = (
    # Address column at the start of an instruction, and the offset
    # column of a relocation line.
    (re.compile(r"^\s*[0-9a-f]+:\s*"), ""),
    # RIP-relative displacements move with the function.
    (re.compile(r"0x[0-9a-f]+\(%rip\)"), "RIP"),
    # Branch and call targets: the offset inside the symbol shifts as
    # soon as any earlier instruction changes length.
    (re.compile(r"<([A-Za-z_.][A-Za-z_.0-9]*)\+0x[0-9a-f]+>"), r"<\1+OFF>"),
    (re.compile(r"\b[0-9a-f]+ <([A-Za-z_.][A-Za-z_.0-9]*(?:\+OFF)?)>"), r"TGT <\1>"),
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
        ["objdump", "-dr", "--no-show-raw-insn", obj], stdout=subprocess.PIPE
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
        reloc = RELOC_RE.match(line)
        if reloc:
            targets.add(reloc.group(1))
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


# Pairs of translation units that differ in exactly one way, each of
# which an earlier version of the normalization silently folded away.
SELF_TEST_CASES = {
    "immediate operand": (
        "int f(int v) { return v == 1; }",
        "int f(int v) { return v == 2; }",
    ),
    "structure offset": (
        "struct S { long a; long b; };\nvoid f(struct S* s, long v) { s->a = v; }",
        "struct S { long a; long b; };\nvoid f(struct S* s, long v) { s->b = v; }",
    ),
    "external call target": (
        "extern int alpha(int);\nint f(int v) { return alpha(v); }",
        "extern int beta(int);\nint f(int v) { return beta(v); }",
    ),
    "unchanged": (
        "int f(int v) { return v == 1; }",
        "int f(int v) { return v == 1; }",
    ),
}


def self_test():
    """Check that the comparison still sees differences it once missed."""
    failures = 0
    with tempfile.TemporaryDirectory(prefix="codegen-selftest-") as tmp:
        tmp = Path(tmp)
        for name, (before_src, after_src) in SELF_TEST_CASES.items():
            objects = []
            for label, source in (("before", before_src), ("after", after_src)):
                path = tmp / f"{label}.c"
                path.write_text(source + "\n")
                obj = tmp / f"{label}.o"
                run(["gcc", "-c", "-fPIC", "-O3", "-w", path, "-o", obj])
                objects.append(obj)
            _, report = compare(*objects)
            expected = name != "unchanged"
            ok = bool(report) is expected
            failures += not ok
            print(f"{'ok  ' if ok else 'FAIL'}  {name}")
    return 1 if failures else 0


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
        "--self-test",
        action="store_true",
        help="compile pairs of tiny sources that differ in one known way "
        "and check the comparison reports each of them",
    )
    parser.add_argument(
        "--fail-on-diff",
        action="store_true",
        help="exit non-zero when any function differs",
    )
    args = parser.parse_args()

    if shutil.which("objdump") is None:
        sys.exit("objdump not found; install binutils")

    if args.self_test:
        return self_test()

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
                if probe is None:
                    source = tree / EXTENSION_SOURCE
                else:
                    # GCC searches the directory of the including file
                    # before any -I, so compiling the probe where it lies
                    # would resolve its quoted includes against the
                    # working tree for both builds and hide the very
                    # difference being looked for.  A scratch directory
                    # has nothing to shadow with.
                    scratch = tmp / f"{label}-probe"
                    scratch.mkdir(exist_ok=True)
                    source = scratch / probe.name
                    shutil.copyfile(probe, source)
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
