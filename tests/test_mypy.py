import sys
import types

import pytest

import multidict


def test_classes_not_abstract() -> None:
    d1 = multidict.MultiDict({"a": "b"})  # type: multidict.MultiDict[str]
    d2 = multidict.CIMultiDict({"a": "b"})  # type: multidict.CIMultiDict[str]

    d3 = multidict.MultiDictProxy(d1)
    d4 = multidict.CIMultiDictProxy(d2)

    d1.getone("a")
    d2.getall("a")
    d3.getone("a")
    d4.getall("a")


@pytest.mark.skipif(
    sys.version_info >= (3, 9),
    reason="Python 3.9 GenericAlias cannot be used in isinstance()/issubclass() checks",
)
@pytest.mark.skipif(
    sys.version_info < (3, 7),
    reason="Python 3.5 and 3.6 has no __class_getitem__ method",
)
def test_class_getitem(_multidict) -> None:
    assert issubclass(_multidict.MultiDict[str], _multidict.MultiDict)
    assert issubclass(_multidict.MultiDictProxy[str], _multidict.MultiDictProxy)
    assert issubclass(_multidict.CIMultiDict[str], _multidict.CIMultiDict)
    assert issubclass(_multidict.CIMultiDictProxy[str], _multidict.CIMultiDictProxy)


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
