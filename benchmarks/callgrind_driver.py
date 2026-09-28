"""Collect deterministic instruction counts for every benchmarked operation.

Run it *with* the interpreter you want to measure, from the repository root::

    .venv-gil/bin/python benchmarks/callgrind_driver.py -o gil.json

Each cell is measured with four Callgrind child processes, ``{run, noop}`` at
two round counts, and reduced with::

    ir_per_op = ((run[r2] - run[r1]) - (noop[r2] - noop[r1])) / ((r2 - r1) * inner)

Differencing two non-zero round counts cancels everything constant, and
subtracting the ``noop`` arm cancels the driving loop.  The formula is the same
whether or not the child could bracket the instrumented region, which is why
four children are used rather than two.

Without bracketing the untimed setup falls inside the counted region, and it
only cancels where both arms leave the mapping in the same state; a destructive
operation such as ``d.clear()`` then reads far too cheap, because the run arm
hands the round a mapping that is cheaper to deallocate.  The driver refuses to
collect such numbers without ``--whole-process``, and numbers published in
the documentation must come from a bracketed run.

Instruction counts do not depend on scheduling, so the children are safe to run
in parallel on a loaded machine.  They do depend on the heap layout, which the
paths and directory listings the interpreter allocates at startup shift, so the
children run from a staging directory of a fixed length instead of from the
tree; see ``Stage``.
"""

import argparse
import concurrent.futures
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import sysconfig
import tempfile
import time

import operations

import multidict

try:
    import multidict._multidict  # noqa: F401

    C_EXTENSION = True
except ImportError:  # pragma: no cover - depends on how multidict was built
    C_EXTENSION = False

I_REFS = re.compile(rb"^==\d+== I\s+refs:\s+([\d,]+)\s*$", re.M)

HERE = os.path.dirname(os.path.abspath(__file__))
CHILD_FILES = ("callgrind_child.py", "operations.py")

# Long enough for any usual temporary directory plus mkdtemp()'s random suffix.
STAGE_LENGTH = 64


class DriverError(RuntimeError):
    pass


class Stage:
    """Hide every path of the tree, the virtualenv and the caller from a child.

    CPython allocates each path it knows at startup, and the listing of each
    directory it imports from, so two checkouts of one commit at paths of
    different lengths, or with different build products lying around, hand the
    measured loop a different heap.  An allocating operation such as an
    ``istr`` lookup can then take a pymalloc slow path on every iteration in
    one and never in the other.  The child runs under ``-S``, which keeps out
    ``pyvenv.cfg`` and the ``.pth`` files whose editable-install finder carries
    the tree's path, from a directory of a fixed length holding a copy of its
    own scripts (``sys.path[0]`` is resolved through symbolic links) and one
    import root with a link to each module it imports.  Its bytecode cache and
    a reduced environment keep the tree's ``__pycache__`` and the caller's
    ``PWD`` out as well.
    """

    def __init__(self, root: str, impl_ids: list[str]) -> None:
        self.root = root
        self.python = os.path.realpath(
            getattr(sys, "_base_executable", None) or sys.executable
        )
        check_interpreter(self.python)
        os.symlink(self.python, os.path.join(root, "python"))
        for name in CHILD_FILES:
            shutil.copy(os.path.join(HERE, name), root)
        self.child = os.path.join(root, CHILD_FILES[0])
        self.env = {
            key: value
            for key, value in os.environ.items()
            if key.startswith(("PYTHON", "MULTIDICT_", "VALGRIND_"))
            or key == "LD_LIBRARY_PATH"
        }
        self.env.pop("PYTHONSTARTUP", None)
        self.env.update(
            PYTHONHASHSEED="0",
            PYTHONPYCACHEPREFIX=os.path.join(root, "pycache"),
            # mimalloc places a reservation of up to 1 GiB at a random address
            # and purges on a timer, and either moves free-threaded counts
            # between two runs of one tree.  A larger reservation is placed by
            # the kernel, which under Valgrind means always at one address.
            MIMALLOC_ARENA_RESERVE="2GiB",
            MIMALLOC_PURGE_DELAY="-1",
            LC_ALL="C",
            TZ="UTC",
        )

        # Find what the child imports through every import root the driver has,
        # then give it only those modules.
        scan = os.path.join(root, "scan")
        os.mkdir(scan)
        links = []
        for i, entry in enumerate(self._import_roots()):
            links.append(os.path.join(scan, f"p{i:02d}"))
            os.symlink(entry, links[-1])
        self.env["PYTHONPATH"] = os.pathsep.join(links)
        found = json.loads(self.run("--probe", *impl_ids))
        shutil.rmtree(scan)

        lib = os.path.join(root, "lib")
        os.mkdir(lib)
        self.modules = found["modules"]
        for path in self.modules.values():
            os.symlink(path, os.path.join(lib, os.path.basename(path)))
        # Build products for other interpreters collect in a checkout, so the
        # package under test holds only the files the child imports from it.
        package = os.path.join(lib, "multidict")
        os.unlink(package)
        os.mkdir(package)
        for path in found["multidict_files"]:
            os.symlink(path, os.path.join(package, os.path.basename(path)))
        self.env["PYTHONPATH"] = lib

        found = json.loads(self.run("--probe", *impl_ids, write_bytecode=True))
        if found["multidict_file"] != os.path.realpath(multidict.__file__):
            raise DriverError(
                f"the staged child imports {found['multidict_file']}, not "
                f"{multidict.__file__}"
            )
        self.bracketed: bool = found["bracketed"]

    def _import_roots(self) -> list[str]:
        no_site = subprocess.run(
            [self.python, "-S", "-c", "import json, sys; print(json.dumps(sys.path))"],
            capture_output=True,
            text=True,
            env={},
            check=True,
        ).stdout
        stdlib = {os.path.realpath(entry) for entry in json.loads(no_site)[1:]}
        roots = [os.path.dirname(os.path.dirname(os.path.realpath(multidict.__file__)))]
        # sys.path[0] is this directory; the child has its own copy instead.
        for entry in sys.path[1:]:
            entry = os.path.realpath(entry or os.curdir)
            if entry not in roots and entry not in stdlib and os.path.isdir(entry):
                roots.append(entry)
        return roots

    def command(self, *args: str) -> list[str]:
        return [os.path.join(self.root, "python"), "-S", self.child, *args]

    def run(self, *args: str, write_bytecode: bool = False) -> str:
        env = dict(self.env)
        if not write_bytecode:
            env["PYTHONDONTWRITEBYTECODE"] = "1"
        proc = subprocess.run(
            self.command(*args),
            capture_output=True,
            text=True,
            cwd=self.root,
            env=env,
            check=False,
        )
        if proc.returncode != 0:
            raise DriverError(
                f"the staged child exited {proc.returncode}:\n{proc.stderr[-2000:]}"
            )
        return proc.stdout


def make_stage_dir() -> str:
    tmp = tempfile.gettempdir()
    # mkdtemp() appends eight random characters; only the length matters.
    pad = STAGE_LENGTH - len(tmp) - len(os.sep) - 8 - len("mdcg-")
    return tempfile.mkdtemp(prefix="mdcg-" + "x" * max(pad, 0), dir=tmp)


def check_interpreter(python: str) -> None:
    """Refuse a wrapper script; Valgrind would detach at its ``exec``."""
    with open(os.path.realpath(python), "rb") as fp:
        if fp.read(2) == b"#!":
            raise DriverError(
                f"{python} is a script, not a binary. Valgrind does not follow "
                "children, so it would measure the wrapper and report a "
                "constant that never changes with the round count. Use a real "
                "interpreter, such as a virtualenv's bin/python or "
                "~/.pyenv/versions/<version>/bin/python, not a pyenv shim."
            )


def measure(
    valgrind: str,
    stage: Stage,
    impl_id: str,
    op_id: str,
    variant: str,
    rounds: int,
    is_bracketed: bool,
) -> int:
    proc = subprocess.run(
        [
            valgrind,
            "--tool=callgrind",
            # Only the child that can turn instrumentation on may start with it
            # off; without the client requests nothing would ever enable it and
            # every count would come back zero.
            f"--instr-atstart={'no' if is_bracketed else 'yes'}",
            "--callgrind-out-file=/dev/null",
            *stage.command(impl_id, op_id, variant, str(rounds)),
        ],
        capture_output=True,
        cwd=stage.root,
        env={**stage.env, "PYTHONDONTWRITEBYTECODE": "1"},
        check=False,
    )
    if proc.returncode != 0:
        raise DriverError(
            f"{impl_id}/{op_id}/{variant}/{rounds} exited {proc.returncode}:\n"
            + proc.stderr.decode(errors="replace")[-2000:]
        )
    found = I_REFS.findall(proc.stderr)
    if len(found) != 1:
        raise DriverError(
            f"{impl_id}/{op_id}/{variant}/{rounds}: expected one 'I refs' line, "
            f"got {len(found)}"
        )
    return int(found[0].replace(b",", b""))


def self_check(
    cells: list[tuple[operations.Operation, operations.Impl]], remedy: str
) -> None:
    """Run each cell once without Valgrind and assert the end state."""
    for op, impl in cells:
        try:
            case = operations.build(op, impl)
        except ImportError as exc:
            raise DriverError(
                f"{impl.id} cannot be imported ({exc}); {remedy}"
            ) from None
        for body in (case.run, case.noop):
            target = case.setup()
            body(target)
        target = case.setup()
        case.run(target)
        if op.id in {"delitem", "pop", "popitem", "clear"}:
            assert len(target) == 0, f"{op.id}/{impl.id} left {len(target)} items"
        elif op.id in {
            "setitem_insert",
            "setdefault_new",
            "add",
            "add_istr",
            "reinit_items",
            "reinit_items_small",
            "reinit_clone",
            "reinit_clone_small",
        }:
            assert len(target) == op.size, f"{op.id}/{impl.id} has {len(target)} items"
        elif op.id == "update":
            assert len(target) == op.size, f"{op.id}/{impl.id} has {len(target)} items"
    print(f"self-check passed: {len(cells)} cells")


def metadata(
    python: str, valgrind: str, is_bracketed: bool, stage: Stage
) -> dict[str, object]:
    def git(*args: str) -> str:
        try:
            return subprocess.run(
                ["git", *args], capture_output=True, text=True, check=True
            ).stdout.strip()
        except (OSError, subprocess.CalledProcessError):
            return "unknown"

    cpu_model = "unknown"
    try:
        with open("/proc/cpuinfo") as fp:
            for line in fp:
                if line.startswith("model name"):
                    cpu_model = line.split(":", 1)[1].strip()
                    break
    except OSError:  # a non-Linux box just reports an unknown CPU
        pass

    valgrind_version = subprocess.run(
        [valgrind, "--version"], capture_output=True, text=True, check=False
    ).stdout.strip()

    return {
        "created": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "git_sha": git("rev-parse", "--short", "HEAD"),
        "git_dirty": bool(git("status", "--porcelain")),
        "multidict_version": multidict.__version__,
        "multidict_file": multidict.__file__,
        "c_extension": C_EXTENSION,
        "python_version": platform.python_version(),
        "python_version_full": sys.version,
        "python_executable": python,
        "gil_disabled": bool(sysconfig.get_config_var("Py_GIL_DISABLED")),
        "gil_enabled_runtime": getattr(sys, "_is_gil_enabled", lambda: True)(),
        "valgrind_version": valgrind_version,
        "cpu_model": cpu_model,
        "platform": platform.platform(),
        "bracketed": is_bracketed,
        "stage": {
            "root_length": len(stage.root),
            "python": stage.python,
            "modules": stage.modules,
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("-o", "--output", help="write results as JSON to this path")
    parser.add_argument(
        "-j",
        "--jobs",
        type=int,
        default=max(1, (os.cpu_count() or 2) // 2),
        help="Callgrind child processes to run at once",
    )
    parser.add_argument("--valgrind", default=shutil.which("valgrind") or "valgrind")
    parser.add_argument(
        "--include-multidict-only",
        action="store_true",
        help="also measure operations a plain dict has no counterpart for",
    )
    parser.add_argument("--impl", choices=sorted(operations.IMPLEMENTATIONS_BY_ID))
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="run every cell once without Valgrind and exit",
    )
    parser.add_argument(
        "--whole-process",
        action="store_true",
        help="measure even without the Valgrind client requests; the numbers "
        "are not comparable with a bracketed run",
    )
    args = parser.parse_args()

    if args.self_check:
        self_check(
            operations.selected(),
            "--self-check covers every implementation, so build the C extension",
        )
        return 0

    python = sys.executable
    valgrind = shutil.which(args.valgrind)
    if valgrind is None:
        raise DriverError(f"valgrind not found at {args.valgrind}")
    # The children run from the stage, without the caller's PATH.
    args.valgrind = os.path.abspath(valgrind)

    cells = operations.selected(
        impl_id=args.impl, shared_only=not args.include_multidict_only
    )
    self_check(
        cells,
        "build the C extension or pick an importable implementation with --impl",
    )
    stage_dir = make_stage_dir()
    try:
        return collect(args, python, stage_dir, cells)
    finally:
        shutil.rmtree(stage_dir)


def collect(
    args: argparse.Namespace,
    python: str,
    stage_dir: str,
    cells: list[tuple[operations.Operation, operations.Impl]],
) -> int:
    stage = Stage(stage_dir, sorted({impl.id for _, impl in cells}))
    is_bracketed = stage.bracketed
    if not is_bracketed and not args.whole_process:
        raise DriverError(
            "the Valgrind client requests are unavailable, so the whole "
            "process would be counted: the untimed setup lands in every "
            "number and destructive operations read too cheap. Install "
            "requirements/pytest.txt, which brings pytest-codspeed, or pass "
            "--whole-process to measure anyway."
        )
    jobs = [
        (op, impl, variant, rounds)
        for op, impl in cells
        for variant in ("run", "noop")
        for rounds in op.rounds
    ]

    print(
        f"{len(cells)} cells, {len(jobs)} Callgrind runs, -j{args.jobs}, "
        f"{'bracketed' if is_bracketed else 'whole-process'} counting",
        flush=True,
    )
    if not is_bracketed:
        print(
            "warning: the Valgrind client requests are unavailable, so the "
            "untimed setup is counted too and destructive operations read too "
            "cheap. Do not compare these numbers with a bracketed run.",
            file=sys.stderr,
            flush=True,
        )

    points: dict[tuple[str, str, str, int], int] = {}
    started = time.monotonic()
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = {
            pool.submit(
                measure,
                args.valgrind,
                stage,
                impl.id,
                op.id,
                variant,
                rounds,
                is_bracketed,
            ): (op.id, impl.id, variant, rounds)
            for op, impl, variant, rounds in jobs
        }
        for done, future in enumerate(
            concurrent.futures.as_completed(futures), start=1
        ):
            key = futures[future]
            points[key] = future.result()
            if done % 20 == 0 or done == len(futures):
                print(f"  {done}/{len(futures)}", flush=True)
    elapsed = time.monotonic() - started

    canary = cells[0]
    op, impl = canary
    r1, r2 = op.rounds
    span = (r2 - r1) * op.inner
    scaling = points[(op.id, impl.id, "run", r2)] - points[(op.id, impl.id, "run", r1)]
    if scaling < span:
        raise DriverError(
            f"{r2 - r1} extra rounds of {op.id}/{impl.id} added only {scaling} "
            "instructions, less than one per operation; Valgrind is not counting "
            "the measured code. A wrapper script instead of a real interpreter is "
            "the usual cause."
        )

    repeat = measure(args.valgrind, stage, impl.id, op.id, "run", r1, is_bracketed)
    drift = repeat - points[(op.id, impl.id, "run", r1)]
    tolerance = 0 if is_bracketed else scaling // 100
    if abs(drift) > tolerance:
        raise DriverError(
            f"counts are not reproducible: {drift:+} instructions between two "
            "identical runs. Did the source change while the run was in "
            "progress?"
        )

    results: dict[str, dict[str, object]] = {}
    for op, impl in cells:
        r1, r2 = op.rounds
        span = (r2 - r1) * op.inner
        run_delta = (
            points[(op.id, impl.id, "run", r2)] - points[(op.id, impl.id, "run", r1)]
        )
        noop_delta = (
            points[(op.id, impl.id, "noop", r2)] - points[(op.id, impl.id, "noop", r1)]
        )
        results.setdefault(op.id, {})[impl.id] = {
            "ir_per_op": round((run_delta - noop_delta) / span, 1),
            "baseline_ir_per_op": round(noop_delta / span, 1),
            "inner": op.inner,
            "rounds": list(op.rounds),
            "points": {
                variant: {
                    str(rounds): points[(op.id, impl.id, variant, rounds)]
                    for rounds in op.rounds
                }
                for variant in ("run", "noop")
            },
        }

    payload = {
        "schema": 1,
        "mode": "callgrind",
        "metadata": {
            **metadata(python, args.valgrind, is_bracketed, stage),
            "drift_ir": drift,
        },
        "operations": {op.id: op.label for op, _ in cells},
        "implementations": {impl.id: impl.label for _, impl in cells},
        "cells": results,
    }

    text = json.dumps(payload, indent=2)
    if args.output:
        with open(args.output, "w") as fp:
            fp.write(text + "\n")
        print(f"wrote {args.output} in {elapsed:.0f}s")
    else:
        print(text)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except DriverError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1) from None
