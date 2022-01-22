import sys
import types

import pytest

import multidict


def test_classes_not_abstract() -> None:
    d1 = multidict.MultiDict({"a": "b"})  # type: multidict.MultiDict[str,str]
    d2 = multidict.CIMultiDict({"a": "b"})  # type: multidict.CIMultiDict[str,str]

    d3 = multidict.MultiDictProxy(d1)
    d4 = multidict.CIMultiDictProxy(d2)

    d1.getone("a")
    d2.getall("a")
    d3.getone("a")
    d4.getall("a")


@pytest.mark.skipif(
    sys.version_info < (3, 9), reason="Python 3.9 is required for GenericAlias"
)
def test_generic_alias(_multidict) -> None:

    assert _multidict.MultiDict[int] == types.GenericAlias(_multidict.MultiDict, (int,))
    assert _multidict.MultiDictProxy[int] == types.GenericAlias(
        _multidict.MultiDictProxy, (int,)
    )
    assert _multidict.CIMultiDict[int] == types.GenericAlias(
        _multidict.CIMultiDict, (int,)
    )
    assert _multidict.CIMultiDictProxy[int] == types.GenericAlias(
        _multidict.CIMultiDictProxy, (int,)
    )
