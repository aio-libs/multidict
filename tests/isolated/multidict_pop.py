# Test for memory leaks surrounding deletion of values.
# SEE: https://github.com/aio-libs/multidict/issues/1232
# We want to make sure that bad predictions or bougus claims
# of memory leaks can be prevented in the future.

import gc
import sys
import psutil
import os
import ctypes
from multidict import MultiDict
from typing import Optional


# Code was borrowed for testing with windows and other operating systems respectively.
# SEE: https://stackoverflow.com/a/79150440


def trim_windows_process_memory(pid: Optional[int] = None) -> bool:
    """Causes effect similar to malloc_trim on -nix."""

    SIZE_T = ctypes.c_uint32 if ctypes.sizeof(ctypes.c_void_p) == 4 else ctypes.c_uint64

    # Get a handle to the current process
    if not pid:
        pid = ctypes.windll.kernel32.GetCurrentProcess()

    # Sometimes FileNotFoundError can appear so the code was
    # changed to handle that workaround.

    ctypes.windll.kernel32.SetProcessWorkingSetSizeEx.argtypes = [
        ctypes.c_void_p,  # Process handle
        SIZE_T,  # Minimum working set size
        SIZE_T,  # Maximum working set size
        ctypes.c_ulong,  # Flags
    ]
    ctypes.windll.kernel32.SetProcessWorkingSetSizeEx.restype = ctypes.c_bool

    # Define constants for SetProcessWorkingSetSizeEx
    QUOTA_LIMITS_HARDWS_MIN_DISABLE = 0x00000002

    # Attempt to set the working set size
    if not ctypes.windll.kernel32.SetProcessWorkingSetSizeEx(
        pid, SIZE_T(-1), SIZE_T(-1), QUOTA_LIMITS_HARDWS_MIN_DISABLE
    ):
        # Retrieve the error code
        error_code = ctypes.windll.kernel32.GetLastError()
        # let's print it since we aren't using a logger...
        print(f"SetProcessWorkingSetSizeEx failed with error code: {error_code}")
        return False
    return True


def trim_ram() -> None:
    """Forces python garbage collection.
    Most importantly, calls malloc_trim/SetProcessWorkingSetSizeEx, which fixes pandas/libc (?) memory leak."""

    gc.collect()
    if sys.platform == "win32":
        assert trim_windows_process_memory(), "trim_ram failed"
    else:
        try:
            ctypes.CDLL("libc.so.6").malloc_trim(0)
        except Exception as e:
            print(" attempt failed")
            raise e


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
