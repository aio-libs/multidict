"""Multidict implementation.

HTTP Headers and URL query string require specific data structure:
multidict. It behaves mostly like a dict but it can have
several values for the same key.
"""

from typing import TYPE_CHECKING

from ._abc import MultiMapping, MutableMultiMapping
from ._compat import USE_EXTENSIONS

__all__ = (
    "MultiMapping",
    "MutableMultiMapping",
    "MultiDictProxy",
    "CIMultiDictProxy",
    "MultiDict",
    "CIMultiDict",
    "upstr",
    "istr",
    "getversion",
)

__version__ = "6.2.0"


if TYPE_CHECKING or not USE_EXTENSIONS:
    from ._multidict_py import (
        CIMultiDict,
        CIMultiDictProxy,
        MultiDict,
        MultiDictProxy,
        getversion,
        istr,
    )
else:
    from ._multidict import (
        _KeysView,
        _ItemsView,
        _ValuesView,
        CIMultiDict,
        CIMultiDictProxy,
        MultiDict,
        MultiDictProxy,
        getversion,
        istr,
    )
    from collections.abc import KeysView, ItemsView, ValuesView

    MultiMapping.register(MultiDictProxy)
    MutableMultiMapping.register(MultiDict)
    KeysView.register(_KeysView)
    ItemsView.register(_ItemsView)
    ValuesView.register(_ValuesView)


upstr = istr
