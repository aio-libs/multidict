import pathlib
import platform
import subprocess
import sys

import pytest

IS_PYPY = platform.python_implementation() == "PyPy"


@pytest.mark.parametrize(
    ("script", "message"),
    (
        (
            "multidict_extend_dict.py",
            "dict leaked after extend()",
        ),
        (
            "multidict_extend_multidict.py",
            "MultiDict leaked after extend()",
        ),
        (
            "multidict_extend_tuple.py",
            "tuple leaked after extend()",
        ),
        (
            "multidict_update_multidict.py",
            "MultiDict leaked after update()",
        ),
    ),
)
@pytest.mark.xfail(reason="memory leak https://github.com/aio-libs/multidict/issues/1117")
def test_leak(script: str, message: str) -> None:
    """Run isolated leak test script and check for leaks."""
    leak_test_script = pathlib.Path(__file__).parent.joinpath("isolated", script)

    with subprocess.Popen(
        [sys.executable, "-u", str(leak_test_script)],
        stdout=subprocess.PIPE,
    ) as proc:
        assert proc.wait() == 0, message
