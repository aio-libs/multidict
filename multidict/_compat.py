import os
import platform

NO_EXTENSIONS = bool(os.environ.get("MULTIDICT_NO_EXTENSIONS"))

PYPY = platform.python_implementation() == "PyPy"

USE_EXTENSIONS = not NO_EXTENSIONS and not PYPY

if USE_EXTENSIONS:  # pragma: no branch
    try:
        from . import _multidict  # type: ignore[attr-defined]  # noqa: F401
    except ImportError:  # pragma: no cover
        USE_EXTENSIONS = False
