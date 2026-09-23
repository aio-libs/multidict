"""Measure one (implementation, operation, variant, rounds) cell under Callgrind.

Run by ``callgrind_driver.py``, never directly.  The interesting output is the
``I refs`` line Callgrind writes to stderr on exit.

``setup`` runs outside the instrumented region, so the rebuild a destructive
operation needs is not counted.  When the Valgrind client requests are not
available the whole process is counted instead, which the driver still reduces
with the same arithmetic; those numbers are approximate rather than exact, so
the driver warns about them.
"""

import sys

import operations

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


if __name__ == "__main__":
    if "--bracketed" in sys.argv:
        print("yes" if BRACKETED else "no")
        raise SystemExit(0)
    raise SystemExit(main(sys.argv[1:]))
