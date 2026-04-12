import threading
from typing import Type, Union

import pytest

from multidict import CIMultiDict, MultiDict


@pytest.mark.parametrize("cls", [CIMultiDict, MultiDict])
def test_race_condition_iterator_vs_mutation(
    cls: Union[Type[CIMultiDict[str]], Type[MultiDict[str]]],
) -> None:
    """Test that concurrent iterations and mutations do not cause a memory safety violation.

    This test specifically triggers use-after-free scenarios if the underlying C extension
    hash table `md->keys` resizes concurrently during an unresolved iteration sequence.
    Under free-threaded CPython (GIL disabled), this previously resulted in a SIGSEGV.

    With the issue fixed, the code securely catches size mutations and cleanly raises
    a standard Python `RuntimeError` ('MultiDict is changed during iteration'), preventing
    crashes.
    """
    if cls.__module__ == "multidict._multidict_py":
        pytest.skip("Test is only applicable to the C extension")

    md: Union[CIMultiDict[str], MultiDict[str]] = cls()
    for i in range(8):
        md[f"init-{i}"] = f"v{i}"

    errors: list[tuple[str, int, str, str, str]] = []

    def writer(target: Union[CIMultiDict[str], MultiDict[str]]) -> None:
        for i in range(5000):
            try:
                target[f"k-{i % 64}"] = f"v{i}"
                if i % 13 == 0:
                    try:
                        del target[f"k-{i % 64}"]
                    except KeyError:
                        pass  # Deleting non-existent keys dynamically is fine
            except RuntimeError:
                pass  # "MultiDict is changed during iteration" is expected
            except Exception as e:
                import traceback

                errors.append(
                    ("writer", i, type(e).__name__, str(e), traceback.format_exc())
                )

    def reader(target: Union[CIMultiDict[str], MultiDict[str]]) -> None:
        for i in range(5000):
            try:
                list(target.items())
                list(target.keys())
                list(target.values())
            except RuntimeError:
                # "MultiDict is changed during iteration" is exactly the expected
                # and memory-safe outcome when iterating a resizing dictionary.
                pass
            except Exception as e:
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
