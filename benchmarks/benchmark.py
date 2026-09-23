"""Wall-clock benchmarks for MultiMapping and MutableMultiMapping.

Measures the operations in ``operations.py`` with :mod:`pyperf`.  Reported
times are per operation and exclude the untimed rebuild that destructive
operations need, but they do include the driving loop; ``callgrind_driver.py``
subtracts that and is what the tables in ``docs/benchmark.rst`` are built from.

Wall clock needs a tuned machine to be meaningful.  See
``docs/benchmark.rst`` for how to prepare one.
"""

import functools
import time

import operations
import pyperf


def bench(case: operations.Case, loops: int) -> float:
    total = 0.0
    perf_counter, setup, run = time.perf_counter, case.setup, case.run
    for _ in range(loops):
        target = setup()
        started = perf_counter()
        run(target)
        total += perf_counter() - started
    return total


def add_impl_option(cmd: list[str], args: object) -> None:
    if args.impl:  # type: ignore[attr-defined]
        cmd.extend(["--impl", args.impl])  # type: ignore[attr-defined]
    if args.shared_only:  # type: ignore[attr-defined]
        cmd.append("--shared-only")


if __name__ == "__main__":
    runner = pyperf.Runner(add_cmdline_args=add_impl_option)

    parser = runner.argparser
    parser.description = (
        "Allows to measure performance of MultiMapping and "
        "MutableMultiMapping implementations"
    )
    parser.add_argument(
        "--impl",
        choices=sorted(operations.IMPLEMENTATIONS_BY_ID),
        help="specific implementation to benchmark",
    )
    parser.add_argument(
        "--shared-only",
        action="store_true",
        help="only the operations a plain dict also has",
    )

    options = parser.parse_args()
    cells = operations.selected(impl_id=options.impl, shared_only=options.shared_only)
    prefixed = options.impl is None

    for op, impl in cells:
        case = operations.build(op, impl)
        name = f"(impl = {impl.id}) {op.id}" if prefixed else op.id
        runner.bench_time_func(
            name, functools.partial(bench, case), inner_loops=op.inner
        )
