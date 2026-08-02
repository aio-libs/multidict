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

        if sys.version_info >= (3, 13) and not sys._is_gil_enabled():
            warnings.warn(
                "The multidict C extension is not available. "
                "The pure-Python fallback is not thread-safe "
                "under free-threaded CPython (GIL disabled). "
                "Avoid concurrent iteration and mutation of the same "
                "MultiDict instance across threads.",
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
