import pathlib
import platform
import subprocess
import sys

import pytest

IS_PYPY = platform.python_implementation() == "PyPy"


@pytest.mark.parametrize(
    ("script"),
    (
        "multidict_extend_dict.py",
        "multidict_extend_multidict.py",
        "multidict_extend_tuple.py",
        "multidict_update_multidict.py",
    ),
)
@pytest.mark.leaks
@pytest.mark.skipif(IS_PYPY, reason="leak testing is not supported on PyPy")
def test_leak(script: str) -> None:
    """Run isolated leak test script and check for leaks."""
    leak_test_script = pathlib.Path(__file__).parent.joinpath("isolated", script)

    with subprocess.Popen(
        [sys.executable, "-u", str(leak_test_script)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    ) as proc:
        exit_code = proc.wait()
        if exit_code != 0:
            outs, errs = proc.communicate()
            raise AssertionError(
                f"Process exited with code {exit_code}, "
                f"stdout: {outs!r}, stderr: {errs!r}"
            )
