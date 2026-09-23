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

Instruction counts do not depend on scheduling, so the children are safe to run
in parallel on a loaded machine.
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
import time

import operations

import multidict

try:
    import multidict._multidict  # noqa: F401

    C_EXTENSION = True
except ImportError:  # pragma: no cover - depends on how multidict was built
    C_EXTENSION = False

I_REFS = re.compile(rb"^==\d+== I\s+refs:\s+([\d,]+)\s*$", re.M)

CHILD = os.path.join(os.path.dirname(os.path.abspath(__file__)), "callgrind_child.py")


class DriverError(RuntimeError):
    pass


def child_env() -> dict[str, str]:
    env = dict(os.environ)
    env.update(
        PYTHONHASHSEED="0",
        PYTHONDONTWRITEBYTECODE="1",
        LC_ALL="C",
        TZ="UTC",
    )
    env.pop("PYTHONSTARTUP", None)
    return env


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
    valgrind: str, python: str, impl_id: str, op_id: str, variant: str, rounds: int
) -> int:
    proc = subprocess.run(
        [
            valgrind,
            "--tool=callgrind",
            "--instr-atstart=no",
            "--callgrind-out-file=/dev/null",
            python,
            CHILD,
            impl_id,
            op_id,
            variant,
            str(rounds),
        ],
        capture_output=True,
        env=child_env(),
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


def bracketed(python: str) -> bool:
    proc = subprocess.run(
        [python, CHILD, "--bracketed"], capture_output=True, text=True, check=True
    )
    return proc.stdout.strip() == "yes"


def self_check() -> None:
    """Run every cell once without Valgrind and assert the end state."""
    for op, impl in operations.selected():
        case = operations.build(op, impl)
        for body in (case.run, case.noop):
            target = case.setup()
            body(target)
        target = case.setup()
        case.run(target)
        if op.id in {"delitem", "pop", "popitem", "clear"}:
            assert len(target) == 0, f"{op.id}/{impl.id} left {len(target)} items"
        elif op.id in {"setitem_insert", "setdefault_new", "add", "add_istr"}:
            assert len(target) == op.size, f"{op.id}/{impl.id} has {len(target)} items"
        elif op.id == "update":
            assert len(target) == op.size, f"{op.id}/{impl.id} has {len(target)} items"
    print(f"self-check passed: {len(operations.selected())} cells")


def metadata(python: str, valgrind: str, is_bracketed: bool) -> dict[str, object]:
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
    except OSError:
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
        "gil_enabled_runtime": sys._is_gil_enabled(),
        "valgrind_version": valgrind_version,
        "cpu_model": cpu_model,
        "platform": platform.platform(),
        "bracketed": is_bracketed,
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
    args = parser.parse_args()

    if args.self_check:
        self_check()
        return 0

    python = sys.executable
    check_interpreter(python)
    if not shutil.which(args.valgrind) and not os.path.exists(args.valgrind):
        raise DriverError(f"valgrind not found at {args.valgrind}")

    self_check()
    is_bracketed = bracketed(python)

    cells = operations.selected(
        impl_id=args.impl, shared_only=not args.include_multidict_only
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

    points: dict[tuple[str, str, str, int], int] = {}
    started = time.monotonic()
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = {
            pool.submit(
                measure, args.valgrind, python, impl.id, op.id, variant, rounds
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
    if points[(op.id, impl.id, "run", r2)] <= points[(op.id, impl.id, "run", r1)] * 1.5:
        raise DriverError(
            "instruction counts do not scale with the round count; Valgrind is "
            "not instrumenting the interpreter"
        )

    repeat = measure(args.valgrind, python, impl.id, op.id, "run", r1)
    drift = repeat - points[(op.id, impl.id, "run", r1)]
    tolerance = 0 if is_bracketed else points[(op.id, impl.id, "run", r1)] // 1000
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
            **metadata(python, args.valgrind, is_bracketed),
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
