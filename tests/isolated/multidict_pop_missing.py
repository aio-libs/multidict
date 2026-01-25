import gc
import psutil
import os
from multidict import MultiDict, CIMultiDict


def trim_ram() -> None:
    """Forces python garbage collection."""
    gc.collect()


process = psutil.Process(os.getpid())


def get_memory_usage() -> float:
    memory_info = process.memory_info()
    return memory_info.rss / (1024 * 1024)


initial_memory_usage = get_memory_usage()


def check_for_leak() -> None:
    trim_ram()
    usage = get_memory_usage() - initial_memory_usage
    # Threshold might need tuning, but 50MB is generous for "no leak"
    # With leak it grows unboundedly.
    assert usage < 50, f"Memory leaked at: {usage} MB"


def _test_pop_missing(cls: type[MultiDict[str] | CIMultiDict[str]], count: int) -> None:
    # Use dynamic keys for missing checks to ensure unique objects
    # if there is a ref leak on identity.
    d = cls()
    for j in range(count):
        key = f"MISSING_{j}"
        try:
            d.pop(key)
        except KeyError:
            pass
        d.pop(key, None)


def _run_isolated_case() -> None:
    # Warmup
    _test_pop_missing(MultiDict, max(100, 10))
    check_for_leak()

    # Run loop
    for _ in range(20):
        _test_pop_missing(MultiDict, 1000)
        _test_pop_missing(CIMultiDict, 1000)
        check_for_leak()


if __name__ == "__main__":
    _run_isolated_case()
