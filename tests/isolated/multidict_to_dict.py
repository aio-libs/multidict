"""Memory leak test for to_dict()."""

import gc
import os

import psutil
from multidict import MultiDict


process = psutil.Process(os.getpid())


def trim_ram() -> None:
    gc.collect()


def get_memory_usage() -> int:
    memory_info = process.memory_info()
    return memory_info.rss // (1024 * 1024)


def test_to_dict_leak() -> None:
    for _ in range(100):
        d = MultiDict([("a", 1), ("b", 2)])
        d.to_dict()
    trim_ram()

    mem_before = get_memory_usage()
    for _ in range(1_000_000):
        d = MultiDict([("a", 1), ("b", 2)])
        d.to_dict()
    trim_ram()
    mem_after = get_memory_usage()

    growth = mem_after - mem_before
    assert growth < 50, f"Memory grew by {growth} MB, possible leak"


if __name__ == "__main__":
    test_to_dict_leak()
    print("PASSED: No memory leak detected in to_dict()")
