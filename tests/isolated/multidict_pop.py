# Test for memory leaks surrounding deletion of values or
# bad cleanups.
# SEE: https://github.com/aio-libs/multidict/issues/1232
# We want to make sure that bad predictions or bougus claims
# of memory leaks can be prevented in the future.

import gc
import sys
import psutil
import os
from multidict import MultiDict


def trim_ram() -> None:
    """Forces python garbage collection."""
    gc.collect()


def get_memory_usage() -> int:
    process = psutil.Process(os.getpid())
    memory_info = process.memory_info()
    return memory_info.rss / (1024 * 1024)  # type: ignore[no-any-return]


keys = [f"X-Any-{i}" for i in range(1000)]
headers = {key: key * 2 for key in keys}


def check_for_leak() -> None:
    trim_ram()
    usage = get_memory_usage()
    # We should never go over 100MB
    if usage > 100:
        sys.exit(1)


def _test_pop() -> None:
    for _ in range(10):
        for _ in range(1000):
            result = MultiDict(headers)
            for k in keys:
                result.pop(k)
        check_for_leak()


def _test_popall() -> None:
    for _ in range(10):
        for _ in range(1000):
            result = MultiDict(headers)
            for k in keys:
                result.popall(k)
        check_for_leak()


def _test_popone() -> None:
    for _ in range(10):
        for _ in range(1000):
            result = MultiDict(headers)
            for k in keys:
                result.popone(k)
        check_for_leak()


def _test_del() -> None:
    for _ in range(10):
        for _ in range(1000):
            result = MultiDict(headers)
            for k in keys:
                del result[k]
        check_for_leak()


def _run_isolated_case() -> None:
    _test_pop()
    _test_popall()
    _test_popone()
    _test_del()
    sys.exit(0)


if __name__ == "__main__":
    _run_isolated_case()
