"""Measure one (implementation, operation, variant, rounds) cell under Callgrind.

Run by ``callgrind_driver.py``, never directly.  The interesting output is the
``I refs`` line Callgrind writes to stderr on exit.

``setup`` runs outside the instrumented region, so the rebuild a destructive
operation needs is not counted.  When the Valgrind client requests are not
available the whole process is counted instead, which the driver still reduces
with the same arithmetic; those numbers are approximate rather than exact, so
the driver collects them only when asked to with ``--whole-process``.
"""

import json
import os
import sys
import sysconfig

import operations

import multidict

WARMUP = 3

try:
    from pytest_codspeed.instruments.hooks import dist_instrument_hooks as _hooks

    _start = _hooks.callgrind_start_instrumentation
    _stop = _hooks.callgrind_stop_instrumentation
    BRACKETED = True
except Exception:  # pragma: no cover - depends on the installed pytest-codspeed
    BRACKETED = False

    def _start() -> None:
        pass

    def _stop() -> None:
        pass


def main(argv: list[str]) -> int:
    impl_id, op_id, variant, rounds_arg = argv
    rounds = int(rounds_arg)
    impl = operations.IMPLEMENTATIONS_BY_ID[impl_id]
    op = operations.OPERATIONS_BY_ID[op_id]
    case = operations.build(op, impl)
    body = case.run if variant == "run" else case.noop

    for _ in range(WARMUP):
        body(case.setup())

    for _ in range(rounds):
        target = case.setup()
        _start()
        body(target)
        _stop()

    return 0


def probe(impl_ids: list[str]) -> None:
    """Import what measuring these implementations would, and say from where."""
    for impl_id in impl_ids:
        operations.build(*operations.selected(impl_id=impl_id)[0])

    paths = sysconfig.get_paths()
    site = tuple(os.path.realpath(paths[k]) + os.sep for k in ("purelib", "platlib"))
    stdlib = tuple(
        os.path.realpath(paths[k]) + os.sep for k in ("stdlib", "platstdlib")
    )
    here = os.path.dirname(os.path.realpath(__file__)) + os.sep
    modules = {}
    multidict_files = []
    for name, module in list(sys.modules.items()):
        path = getattr(module, "__file__", None)
        if path is None:
            continue
        path = os.path.realpath(path)
        if path.startswith(here) or (
            path.startswith(stdlib) and not path.startswith(site)
        ):
            continue
        top = sys.modules[name.partition(".")[0]]
        top_path = os.path.realpath(top.__file__)
        modules[top.__name__] = (
            os.path.dirname(top_path) if hasattr(top, "__path__") else top_path
        )
        if top is multidict:
            multidict_files.append(path)

    json.dump(
        {
            "bracketed": BRACKETED,
            "multidict_file": os.path.realpath(multidict.__file__),
            "multidict_files": sorted(multidict_files),
            "modules": dict(sorted(modules.items())),
        },
        sys.stdout,
    )


if __name__ == "__main__":
    if sys.argv[1:2] == ["--probe"]:
        probe(sys.argv[2:])
        raise SystemExit(0)
    raise SystemExit(main(sys.argv[1:]))
