import os
import platform

NO_EXTENSIONS = os.environ.get("MULTIDICT_NO_EXTENSIONS", "0") == "1"

PYPY = platform.python_implementation() == "PyPy"

USE_EXTENSIONS = not NO_EXTENSIONS and not PYPY

if USE_EXTENSIONS:
    try:
        from . import _multidict  # noqa
    except ImportError:
        USE_EXTENSIONS = False
