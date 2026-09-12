import threading
from collections.abc import Callable
from concurrent.futures import ThreadPoolExecutor
from typing import TypeVar

import pytest

from multidict import CIMultiDict, CIMultiDictProxy, MultiDict, MultiDictProxy

_T = TypeVar("_T")
_MD_Types = MultiDict[_T] | CIMultiDict[_T] | MultiDictProxy[_T] | CIMultiDictProxy[_T]
GetVersion = Callable[[_MD_Types[_T]], int]


def test_getversion_bad_param(multidict_getversion_callable: GetVersion[str]) -> None:
    with pytest.raises(TypeError):
        multidict_getversion_callable(1)  # type: ignore[arg-type]


def test_ctor(
    any_multidict_class: type[MultiDict[str]],
    multidict_getversion_callable: GetVersion[str],
) -> None:
    m1 = any_multidict_class()
    v1 = multidict_getversion_callable(m1)
    m2 = any_multidict_class()
    v2 = multidict_getversion_callable(m2)
    assert v1 != v2


def test_add(
    any_multidict_class: type[MultiDict[str]],
    any_multidict_proxy_class: type[MultiDictProxy[str]],
    multidict_getversion_callable: GetVersion[str],
) -> None:
    m = any_multidict_class()
    p = any_multidict_proxy_class(m)
    v = multidict_getversion_callable(m)
    assert v == multidict_getversion_callable(p)
    m.add("key", "val")
    v2 = multidict_getversion_callable(m)
    assert v2 > v
    assert v2 == multidict_getversion_callable(p)


def test_delitem(
    any_multidict_class: type[MultiDict[str]],
    any_multidict_proxy_class: type[MultiDictProxy[str]],
    multidict_getversion_callable: GetVersion[str],
) -> None:
    m = any_multidict_class()
    p = any_multidict_proxy_class(m)
    m.add("key", "val")
    v = multidict_getversion_callable(m)
    assert v == multidict_getversion_callable(p)
    del m["key"]
    v2 = multidict_getversion_callable(m)
    assert v2 > v
    assert v2 == multidict_getversion_callable(p)


def test_delitem_not_found(
    any_multidict_class: type[MultiDict[str]],
    any_multidict_proxy_class: type[MultiDictProxy[str]],
    multidict_getversion_callable: GetVersion[str],
) -> None:
    m = any_multidict_class()
    p = any_multidict_proxy_class(m)
    m.add("key", "val")
    v = multidict_getversion_callable(m)
    assert v == multidict_getversion_callable(p)
    with pytest.raises(KeyError):
        del m["notfound"]
    assert multidict_getversion_callable(m) == v
    assert v == multidict_getversion_callable(p)


def test_setitem(
    any_multidict_class: type[MultiDict[str]],
    any_multidict_proxy_class: type[MultiDictProxy[str]],
    multidict_getversion_callable: GetVersion[str],
) -> None:
    m = any_multidict_class()
    p = any_multidict_proxy_class(m)
    m.add("key", "val")
    v = multidict_getversion_callable(m)
    assert v == multidict_getversion_callable(p)
    m["key"] = "val2"
    v2 = multidict_getversion_callable(m)
    assert v2 > v
    assert v2 == multidict_getversion_callable(p)


def test_setitem_not_found(
    any_multidict_class: type[MultiDict[str]],
    any_multidict_proxy_class: type[MultiDictProxy[str]],
    multidict_getversion_callable: GetVersion[str],
) -> None:
    m = any_multidict_class()
    p = any_multidict_proxy_class(m)
    m.add("key", "val")
    v = multidict_getversion_callable(m)
    assert v == multidict_getversion_callable(p)
    m["notfound"] = "val2"
    v2 = multidict_getversion_callable(m)
    assert v2 > v
    assert v2 == multidict_getversion_callable(p)


def test_clear(
    any_multidict_class: type[MultiDict[str]],
    any_multidict_proxy_class: type[MultiDictProxy[str]],
    multidict_getversion_callable: GetVersion[str],
) -> None:
    m = any_multidict_class()
    p = any_multidict_proxy_class(m)
    m.add("key", "val")
    v = multidict_getversion_callable(m)
    assert v == multidict_getversion_callable(p)
    m.clear()
    v2 = multidict_getversion_callable(m)
    assert v2 > v
    assert v2 == multidict_getversion_callable(p)


def test_setdefault(
    any_multidict_class: type[MultiDict[str]],
    any_multidict_proxy_class: type[MultiDictProxy[str]],
    multidict_getversion_callable: GetVersion[str],
) -> None:
    m = any_multidict_class()
    p = any_multidict_proxy_class(m)
    m.add("key", "val")
    v = multidict_getversion_callable(m)
    assert v == multidict_getversion_callable(p)
    m.setdefault("key2", "val2")
    v2 = multidict_getversion_callable(m)
    assert v2 > v
    assert v2 == multidict_getversion_callable(p)


def test_popone(
    any_multidict_class: type[MultiDict[str]],
    any_multidict_proxy_class: type[MultiDictProxy[str]],
    multidict_getversion_callable: GetVersion[str],
) -> None:
    m = any_multidict_class()
    p = any_multidict_proxy_class(m)
    m.add("key", "val")
    v = multidict_getversion_callable(m)
    assert v == multidict_getversion_callable(p)
    m.popone("key")
    v2 = multidict_getversion_callable(m)
    assert v2 > v
    assert v2 == multidict_getversion_callable(p)


def test_popone_default(
    any_multidict_class: type[MultiDict[str]],
    any_multidict_proxy_class: type[MultiDictProxy[str]],
    multidict_getversion_callable: GetVersion[str],
) -> None:
    m = any_multidict_class()
    p = any_multidict_proxy_class(m)
    m.add("key", "val")
    v = multidict_getversion_callable(m)
    assert v == multidict_getversion_callable(p)
    m.popone("key2", "default")
    v2 = multidict_getversion_callable(m)
    assert v2 == v
    assert v2 == multidict_getversion_callable(p)


def test_popone_key_error(
    any_multidict_class: type[MultiDict[str]],
    any_multidict_proxy_class: type[MultiDictProxy[str]],
    multidict_getversion_callable: GetVersion[str],
) -> None:
    m = any_multidict_class()
    p = any_multidict_proxy_class(m)
    m.add("key", "val")
    v = multidict_getversion_callable(m)
    assert v == multidict_getversion_callable(p)
    with pytest.raises(KeyError):
        m.popone("key2")
    v2 = multidict_getversion_callable(m)
    assert v2 == v
    assert v2 == multidict_getversion_callable(p)


def test_pop(
    any_multidict_class: type[MultiDict[str]],
    any_multidict_proxy_class: type[MultiDictProxy[str]],
    multidict_getversion_callable: GetVersion[str],
) -> None:
    m = any_multidict_class()
    p = any_multidict_proxy_class(m)
    m.add("key", "val")
    v = multidict_getversion_callable(m)
    assert v == multidict_getversion_callable(p)
    m.pop("key")
    v2 = multidict_getversion_callable(m)
    assert v2 > v
    assert v2 == multidict_getversion_callable(p)


def test_pop_default(
    any_multidict_class: type[MultiDict[str]],
    any_multidict_proxy_class: type[MultiDictProxy[str]],
    multidict_getversion_callable: GetVersion[str],
) -> None:
    m = any_multidict_class()
    p = any_multidict_proxy_class(m)
    m.add("key", "val")
    v = multidict_getversion_callable(m)
    assert v == multidict_getversion_callable(p)
    m.pop("key2", "default")
    v2 = multidict_getversion_callable(m)
    assert v2 == v
    assert v2 == multidict_getversion_callable(p)


def test_pop_key_error(
    any_multidict_class: type[MultiDict[str]],
    any_multidict_proxy_class: type[MultiDictProxy[str]],
    multidict_getversion_callable: GetVersion[str],
) -> None:
    m = any_multidict_class()
    p = any_multidict_proxy_class(m)
    m.add("key", "val")
    v = multidict_getversion_callable(m)
    assert v == multidict_getversion_callable(p)
    with pytest.raises(KeyError):
        m.pop("key2")
    v2 = multidict_getversion_callable(m)
    assert v2 == v
    assert v2 == multidict_getversion_callable(p)


def test_popall(
    any_multidict_class: type[MultiDict[str]],
    any_multidict_proxy_class: type[MultiDictProxy[str]],
    multidict_getversion_callable: GetVersion[str],
) -> None:
    m = any_multidict_class()
    p = any_multidict_proxy_class(m)
    m.add("key", "val")
    v = multidict_getversion_callable(m)
    assert v == multidict_getversion_callable(p)
    m.popall("key")
    v2 = multidict_getversion_callable(m)
    assert v2 > v
    assert v2 == multidict_getversion_callable(p)


def test_popall_default(
    any_multidict_class: type[MultiDict[str]],
    any_multidict_proxy_class: type[MultiDictProxy[str]],
    multidict_getversion_callable: GetVersion[str],
) -> None:
    m = any_multidict_class()
    p = any_multidict_proxy_class(m)
    m.add("key", "val")
    v = multidict_getversion_callable(m)
    assert v == multidict_getversion_callable(p)
    m.popall("key2", "default")
    v2 = multidict_getversion_callable(m)
    assert v2 == v
    assert v2 == multidict_getversion_callable(p)


def test_popall_key_error(
    any_multidict_class: type[MultiDict[str]],
    any_multidict_proxy_class: type[MultiDictProxy[str]],
    multidict_getversion_callable: GetVersion[str],
) -> None:
    m = any_multidict_class()
    p = any_multidict_proxy_class(m)
    m.add("key", "val")
    v = multidict_getversion_callable(m)
    assert v == multidict_getversion_callable(p)
    with pytest.raises(KeyError):
        m.popall("key2")
    v2 = multidict_getversion_callable(m)
    assert v2 == v
    assert v2 == multidict_getversion_callable(p)


def test_popitem(
    any_multidict_class: type[MultiDict[str]],
    any_multidict_proxy_class: type[MultiDictProxy[str]],
    multidict_getversion_callable: GetVersion[str],
) -> None:
    m = any_multidict_class()
    p = any_multidict_proxy_class(m)
    m.add("key", "val")
    v = multidict_getversion_callable(m)
    assert v == multidict_getversion_callable(p)
    m.popitem()
    v2 = multidict_getversion_callable(m)
    assert v2 > v
    assert v2 == multidict_getversion_callable(p)


def test_popitem_key_error(
    any_multidict_class: type[MultiDict[str]],
    any_multidict_proxy_class: type[MultiDictProxy[str]],
    multidict_getversion_callable: GetVersion[str],
) -> None:
    m = any_multidict_class()
    p = any_multidict_proxy_class(m)
    v = multidict_getversion_callable(m)
    assert v == multidict_getversion_callable(p)
    with pytest.raises(KeyError):
        m.popitem()
    v2 = multidict_getversion_callable(m)
    assert v2 == v
    assert v2 == multidict_getversion_callable(p)


def test_version_thread_safety(
    any_multidict_class: type[MultiDict[int]],
    multidict_getversion_callable: GetVersion[int],
) -> None:
    """Concurrently mutating independent multidicts must never hand out
    the same version number twice.

    Regression test for a version-counter race: every mutation derives
    its instance's version from one counter shared by all instances of
    the implementation (module state in the C extension, a module-level
    array in pure Python), so that unrelated multidicts can be compared
    and always disagree. Bumping that shared counter used to be a plain
    increment with no synchronization of its own, relying entirely on
    each instance's own lock; under a free-threaded build, two threads
    mutating two *different* instances could bump it at the same time and
    step on each other's update, handing out one version number to two
    objects, or a smaller one to a later mutation than an earlier one
    already got.
    """
    n_threads = 16
    n_iters = 3000
    all_versions: list[list[int]] = []
    lock = threading.Lock()

    def worker(_n: int) -> None:
        m = any_multidict_class()
        versions = []
        for i in range(n_iters):
            m["key"] = i
            versions.append(multidict_getversion_callable(m))
        with lock:
            all_versions.append(versions)

    with ThreadPoolExecutor(max_workers=n_threads) as executor:
        list(executor.map(worker, range(n_threads)))

    flat_versions = [v for versions in all_versions for v in versions]
    assert len(set(flat_versions)) == len(flat_versions)
