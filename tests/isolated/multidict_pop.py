# Test for memory leaks surrounding deletion of values.
# SEE: https://github.com/aio-libs/multidict/issues/1232

import gc
import sys
import psutil
import os
from multidict import MultiDict

def get_memory_usage():
    process = psutil.Process(os.getpid())
    memory_info = process.memory_info()
    return memory_info.rss / (1024 * 1024)

keys = [f"X-Any-{i}" for i in range(1000)]
headers = {key: key * 2 for key in keys}


def _run_isolated_case() -> None:
    for _ in range(10):
        for _ in range(1000):
            result = MultiDict()
            result.update(headers)
            for k in keys:
                result.pop(k)
                # popitem() currently is unaffected but the others all have memory leaks...
                # result.popone(k)
                # result.popall(k)
            del result
        gc.collect()
        usage = get_memory_usage()
        # We should never go over 100MB
        if usage > 100:
            sys.exit(1)
    sys.exit(0)

if __name__ == "__main__":
    _run_isolated_case()

