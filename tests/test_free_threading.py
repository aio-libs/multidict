from __future__ import annotations

import os
import subprocess
import sys
import threading

import pytest

from multidict import CIMultiDict, MultiDict, MutableMultiMapping


@pytest.mark.c_extension
def test_race_condition_iterator_vs_mutation(
    any_multidict_class: type[CIMultiDict[str]] | type[MultiDict[str]],
) -> None:
    """Test that concurrent iterations and mutations do not cause a memory safety violation.

    This test specifically triggers use-after-free scenarios if the underlying C extension
    hash table ``md->keys`` resizes concurrently during an unresolved iteration sequence.
    Under free-threaded CPython (GIL disabled), this previously resulted in a SIGSEGV.

    With the issue fixed, the code securely catches size mutations and cleanly raises
    a standard Python ``RuntimeError`` ('MultiDict is changed during iteration'), preventing
    crashes.
    """
    if getattr(any_multidict_class, "__module__", "").endswith("_multidict_py"):
        pytest.skip("Test is only applicable to the C extension")

    md: MutableMultiMapping[str] = any_multidict_class()
    for i in range(8):
        md[f"init-{i}"] = f"v{i}"

    errors: list[tuple[str, int, str, str, str]] = []

    def writer(target: MutableMultiMapping[str]) -> None:
        for i in range(256):
            try:
                target[f"k-{i % 64}"] = f"v{i}"
            except RuntimeError:  # pragma: no cover
                # "MultiDict changed during iteration" is expected under contention
                pass
            except Exception as e:  # pragma: no cover
                import traceback

                errors.append(
                    ("writer", i, type(e).__name__, str(e), traceback.format_exc())
                )

    def reader(target: MutableMultiMapping[str]) -> None:
        for i in range(256):
            try:
                list(target.items())
                list(target.keys())
                list(target.values())
            except RuntimeError:
                # "MultiDict changed during iteration" is exactly the expected
                # and memory-safe outcome when iterating a resizing dictionary.
                pass
            except Exception as e:  # pragma: no cover
                errors.append(("reader", i, type(e).__name__, str(e), ""))

    threads = [
        threading.Thread(target=f, args=(md,)) for f in [writer, reader, writer, reader]
    ]

    for t in threads:
        t.start()
    for t in threads:
        t.join()

    # The test passes if it survives without a segmentation fault (SIGSEGV/SIGABRT).
    # If the C-extension is thread-safe, no Python exceptions other than RuntimeError
    # (handled above) should inadvertently surface to the user.
    assert not errors, f"Unexpected errors during concurrent execution: {errors}"


@pytest.mark.skipif(
    sys.version_info < (3, 13),
    reason="Free-threaded CPython warning requires Python 3.13+",
)
def test_pure_python_free_threaded_warning() -> None:
    """Test that a RuntimeWarning is emitted on free-threaded CPython without C ext."""
    script = (
        "import sys\n"
        "sys._is_gil_enabled = lambda: False\n"
        "import warnings\n"
        "with warnings.catch_warnings(record=True) as w:\n"
        "    warnings.simplefilter('always')\n"
        "    import multidict\n"
        "msgs = [str(x.message) for x in w if issubclass(x.category, RuntimeWarning)]\n"
        "assert any('not thread-safe' in m for m in msgs), "
        "f'Expected thread-safety warning, got: {msgs}'\n"
    )
    result = subprocess.run(
        [sys.executable, "-c", script],
        env={**os.environ, "MULTIDICT_NO_EXTENSIONS": "1"},
        capture_output=True,
        text=True,
    )
    assert result.returncode == 0, result.stderr

