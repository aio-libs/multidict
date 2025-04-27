from collections.abc import Callable
from typing import TypeVar, Union

import pytest

from multidict import CIMultiDict, CIMultiDictProxy, MultiDict, MultiDictProxy

_T = TypeVar("_T")
_MD_Types = Union[
    MultiDict[_T], CIMultiDict[_T], MultiDictProxy[_T], CIMultiDictProxy[_T]
]
GetVersion = Callable[[_MD_Types[_T]], int]


def test_getversion_bad_param(md_getversion: GetVersion[str]) -> None:
    with pytest.raises(TypeError):
        md_getversion(1)  # type: ignore[arg-type]


def test_ctor(
    any_md_class: type[MultiDict[str]],
    md_getversion: GetVersion[str],
) -> None:
    m1 = any_md_class()
    v1 = md_getversion(m1)
    m2 = any_md_class()
    v2 = md_getversion(m2)
    assert v1 != v2


def test_add(
    any_md_class: type[MultiDict[str]],
    any_md_proxy_class: type[MultiDictProxy[str]],
    md_getversion: GetVersion[str],
) -> None:
    m = any_md_class()
    p = any_md_proxy_class(m)
    v = md_getversion(m)
    assert v == md_getversion(p)
    m.add("key", "val")
    v2 = md_getversion(m)
    assert v2 > v
    assert v2 == md_getversion(p)


def test_delitem(
    any_md_class: type[MultiDict[str]],
    any_md_proxy_class: type[MultiDictProxy[str]],
    md_getversion: GetVersion[str],
) -> None:
    m = any_md_class()
    p = any_md_proxy_class(m)
    m.add("key", "val")
    v = md_getversion(m)
    assert v == md_getversion(p)
    del m["key"]
    v2 = md_getversion(m)
    assert v2 > v
    assert v2 == md_getversion(p)


def test_delitem_not_found(
    any_md_class: type[MultiDict[str]],
    any_md_proxy_class: type[MultiDictProxy[str]],
    md_getversion: GetVersion[str],
) -> None:
    m = any_md_class()
    p = any_md_proxy_class(m)
    m.add("key", "val")
    v = md_getversion(m)
    assert v == md_getversion(p)
    with pytest.raises(KeyError):
        del m["notfound"]
    assert md_getversion(m) == v
    assert v == md_getversion(p)


def test_setitem(
    any_md_class: type[MultiDict[str]],
    any_md_proxy_class: type[MultiDictProxy[str]],
    md_getversion: GetVersion[str],
) -> None:
    m = any_md_class()
    p = any_md_proxy_class(m)
    m.add("key", "val")
    v = md_getversion(m)
    assert v == md_getversion(p)
    m["key"] = "val2"
    v2 = md_getversion(m)
    assert v2 > v
    assert v2 == md_getversion(p)


def test_setitem_not_found(
    any_md_class: type[MultiDict[str]],
    any_md_proxy_class: type[MultiDictProxy[str]],
    md_getversion: GetVersion[str],
) -> None:
    m = any_md_class()
    p = any_md_proxy_class(m)
    m.add("key", "val")
    v = md_getversion(m)
    assert v == md_getversion(p)
    m["notfound"] = "val2"
    v2 = md_getversion(m)
    assert v2 > v
    assert v2 == md_getversion(p)


def test_clear(
    any_md_class: type[MultiDict[str]],
    any_md_proxy_class: type[MultiDictProxy[str]],
    md_getversion: GetVersion[str],
) -> None:
    m = any_md_class()
    p = any_md_proxy_class(m)
    m.add("key", "val")
    v = md_getversion(m)
    assert v == md_getversion(p)
    m.clear()
    v2 = md_getversion(m)
    assert v2 > v
    assert v2 == md_getversion(p)


def test_setdefault(
    any_md_class: type[MultiDict[str]],
    any_md_proxy_class: type[MultiDictProxy[str]],
    md_getversion: GetVersion[str],
) -> None:
    m = any_md_class()
    p = any_md_proxy_class(m)
    m.add("key", "val")
    v = md_getversion(m)
    assert v == md_getversion(p)
    m.setdefault("key2", "val2")
    v2 = md_getversion(m)
    assert v2 > v
    assert v2 == md_getversion(p)


def test_popone(
    any_md_class: type[MultiDict[str]],
    any_md_proxy_class: type[MultiDictProxy[str]],
    md_getversion: GetVersion[str],
) -> None:
    m = any_md_class()
    p = any_md_proxy_class(m)
    m.add("key", "val")
    v = md_getversion(m)
    assert v == md_getversion(p)
    m.popone("key")
    v2 = md_getversion(m)
    assert v2 > v
    assert v2 == md_getversion(p)


def test_popone_default(
    any_md_class: type[MultiDict[str]],
    any_md_proxy_class: type[MultiDictProxy[str]],
    md_getversion: GetVersion[str],
) -> None:
    m = any_md_class()
    p = any_md_proxy_class(m)
    m.add("key", "val")
    v = md_getversion(m)
    assert v == md_getversion(p)
    m.popone("key2", "default")
    v2 = md_getversion(m)
    assert v2 == v
    assert v2 == md_getversion(p)


def test_popone_key_error(
    any_md_class: type[MultiDict[str]],
    any_md_proxy_class: type[MultiDictProxy[str]],
    md_getversion: GetVersion[str],
) -> None:
    m = any_md_class()
    p = any_md_proxy_class(m)
    m.add("key", "val")
    v = md_getversion(m)
    assert v == md_getversion(p)
    with pytest.raises(KeyError):
        m.popone("key2")
    v2 = md_getversion(m)
    assert v2 == v
    assert v2 == md_getversion(p)


def test_pop(
    any_md_class: type[MultiDict[str]],
    any_md_proxy_class: type[MultiDictProxy[str]],
    md_getversion: GetVersion[str],
) -> None:
    m = any_md_class()
    p = any_md_proxy_class(m)
    m.add("key", "val")
    v = md_getversion(m)
    assert v == md_getversion(p)
    m.pop("key")
    v2 = md_getversion(m)
    assert v2 > v
    assert v2 == md_getversion(p)


def test_pop_default(
    any_md_class: type[MultiDict[str]],
    any_md_proxy_class: type[MultiDictProxy[str]],
    md_getversion: GetVersion[str],
) -> None:
    m = any_md_class()
    p = any_md_proxy_class(m)
    m.add("key", "val")
    v = md_getversion(m)
    assert v == md_getversion(p)
    m.pop("key2", "default")
    v2 = md_getversion(m)
    assert v2 == v
    assert v2 == md_getversion(p)


def test_pop_key_error(
    any_md_class: type[MultiDict[str]],
    any_md_proxy_class: type[MultiDictProxy[str]],
    md_getversion: GetVersion[str],
) -> None:
    m = any_md_class()
    p = any_md_proxy_class(m)
    m.add("key", "val")
    v = md_getversion(m)
    assert v == md_getversion(p)
    with pytest.raises(KeyError):
        m.pop("key2")
    v2 = md_getversion(m)
    assert v2 == v
    assert v2 == md_getversion(p)


def test_popall(
    any_md_class: type[MultiDict[str]],
    any_md_proxy_class: type[MultiDictProxy[str]],
    md_getversion: GetVersion[str],
) -> None:
    m = any_md_class()
    p = any_md_proxy_class(m)
    m.add("key", "val")
    v = md_getversion(m)
    assert v == md_getversion(p)
    m.popall("key")
    v2 = md_getversion(m)
    assert v2 > v
    assert v2 == md_getversion(p)


def test_popall_default(
    any_md_class: type[MultiDict[str]],
    any_md_proxy_class: type[MultiDictProxy[str]],
    md_getversion: GetVersion[str],
) -> None:
    m = any_md_class()
    p = any_md_proxy_class(m)
    m.add("key", "val")
    v = md_getversion(m)
    assert v == md_getversion(p)
    m.popall("key2", "default")
    v2 = md_getversion(m)
    assert v2 == v
    assert v2 == md_getversion(p)


def test_popall_key_error(
    any_md_class: type[MultiDict[str]],
    any_md_proxy_class: type[MultiDictProxy[str]],
    md_getversion: GetVersion[str],
) -> None:
    m = any_md_class()
    p = any_md_proxy_class(m)
    m.add("key", "val")
    v = md_getversion(m)
    assert v == md_getversion(p)
    with pytest.raises(KeyError):
        m.popall("key2")
    v2 = md_getversion(m)
    assert v2 == v
    assert v2 == md_getversion(p)


def test_popitem(
    any_md_class: type[MultiDict[str]],
    any_md_proxy_class: type[MultiDictProxy[str]],
    md_getversion: GetVersion[str],
) -> None:
    m = any_md_class()
    p = any_md_proxy_class(m)
    m.add("key", "val")
    v = md_getversion(m)
    assert v == md_getversion(p)
    m.popitem()
    v2 = md_getversion(m)
    assert v2 > v
    assert v2 == md_getversion(p)


def test_popitem_key_error(
    any_md_class: type[MultiDict[str]],
    any_md_proxy_class: type[MultiDictProxy[str]],
    md_getversion: GetVersion[str],
) -> None:
    m = any_md_class()
    p = any_md_proxy_class(m)
    v = md_getversion(m)
    assert v == md_getversion(p)
    with pytest.raises(KeyError):
        m.popitem()
    v2 = md_getversion(m)
    assert v2 == v
    assert v2 == md_getversion(p)
