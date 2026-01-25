"""Memory leak test for to_dict()."""
import gc
import tracemalloc

from multidict import MultiDict


def get_mem() -> int:
    gc.collect()
    gc.collect()
    gc.collect()
    return tracemalloc.get_traced_memory()[0]


def test_to_dict_leak() -> None:
    tracemalloc.start()
    for _ in range(100):
        d = MultiDict([("a", 1), ("b", 2)])
        d.to_dict()
    get_mem()

    mem_before = get_mem()
    for _ in range(1_000_000):
        d = MultiDict([("a", 1), ("b", 2)])
        d.to_dict()
    mem_after = get_mem()

    tracemalloc.stop()

    growth = mem_after - mem_before
    assert growth < 50_000, f"Memory grew by {growth} bytes, possible leak"


if __name__ == "__main__":
    test_to_dict_leak()
    print("PASSED: No memory leak detected in to_dict()")
