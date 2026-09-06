"""
Multidict implementation.

HTTP Headers and URL query string require specific data structure:
multidict. It behaves mostly like a dict but it can have
several values for the same key.
"""

from typing import TYPE_CHECKING

from ._abc import MultiMapping, MutableMultiMapping
from ._compat import USE_EXTENSIONS

__all__ = (
    "CIMultiDict",
    "CIMultiDictProxy",
    "MultiDict",
    "MultiDictProxy",
    "MultiMapping",
    "MutableMultiMapping",
    "getversion",
    "istr",
    "upstr",
)

__version__ = "6.7.2.dev0"


if TYPE_CHECKING or not USE_EXTENSIONS:
    from ._multidict_py import (
        CIMultiDict,
        CIMultiDictProxy,
        MultiDict,
        MultiDictProxy,
        getversion,
        istr,
    )

    if not TYPE_CHECKING:
        import sys
        import warnings

        # ``sys._is_gil_enabled`` is CPython-private, and this branch is taken
        # on alternative implementations too (``_compat`` forces it on PyPy),
        # so probe for the attribute rather than inferring it from the version.
        # A diagnostic must not be able to break the import it diagnoses.
        if not getattr(sys, "_is_gil_enabled", lambda: True)():
            warnings.warn(
                "The multidict C extension is not in use, either because it "
                "is unavailable or because MULTIDICT_NO_EXTENSIONS is set. "
                "The pure-Python fallback is not thread-safe under "
                "free-threaded CPython (GIL disabled): concurrent mutation "
                "can leave a MultiDict internally inconsistent, so confine "
                "each instance to one thread.",
                RuntimeWarning,
                stacklevel=2,
            )
else:
    from collections.abc import ItemsView, KeysView, ValuesView

    from ._multidict import (
        CIMultiDict,
        CIMultiDictProxy,
        MultiDict,
        MultiDictProxy,
        _ItemsView,
        _KeysView,
        _ValuesView,
        getversion,
        istr,
    )

    MultiMapping.register(MultiDictProxy)
    MutableMultiMapping.register(MultiDict)
    KeysView.register(_KeysView)
    ItemsView.register(_ItemsView)
    ValuesView.register(_ValuesView)


upstr = istr
