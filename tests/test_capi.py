import pytest

from multidict import MultiDict, MultiDictProxy, istr, CIMultiDict, CIMultiDictProxy

pytest.importorskip("multidict._multidict")
testcapi = pytest.importorskip("testcapi")

pytestmark = pytest.mark.capi

MultiDictStr = MultiDict[str]
MultiDictProxyStr = MultiDictProxy[str]

CIMultiDictStr = CIMultiDict[str]
CIMultiDictProxyStr = MultiDictProxy[str]


def test_md_add() -> None:
    md: MultiDictStr = MultiDict()
    testcapi.md_add(md, "key", "value")
    assert len(md) == 1
    assert list(md.items()) == [("key", "value")]


def test_md_clear() -> None:
    previous = MultiDict([("key", "value")])
    md: MultiDictStr = previous.copy()
    testcapi.md_clear(md)
    assert md != previous


def test_md_contains() -> None:
    d = MultiDict([("key", "one")])
    assert testcapi.md_contains(d, "key")
    testcapi.md_del(d, "key")
    assert testcapi.md_contains(d, "key") is False


def test_md_del() -> None:
    d = MultiDict([("key", "one"), ("key", "two")], foo="bar")
    assert list(d.keys()) == ["key", "key", "foo"]

    testcapi.md_del(d, "key")
    assert d == {"foo": "bar"}
    assert list(d.items()) == [("foo", "bar")]

    with pytest.raises(KeyError, match="key"):
        testcapi.md_del(d, "key")


def test_md_get_all() -> None:
    d: MultiDictStr = MultiDict()
    d.add("key1", "val1")
    d.add("key2", "val2")
    d.add("key1", "val3")
    ret = testcapi.md_getall(d, "key1")
    assert (["val1", "val3"], True) == ret


def test_md_get_all_miss() -> None:
    d = MultiDict([("key", "value1")], key="value2")
    assert testcapi.md_getall(d, "x")[1] is False


def test_md_getone() -> None:
    d: MultiDictStr = MultiDict(key="val1")
    d.add("key", "val2")
    assert testcapi.md_getone(d, "key") == ("val1", True)


def test_md_getone_miss() -> None:
    d: MultiDictStr = MultiDict([("key", "value1")], key="value2")
    assert testcapi.md_getone(d, "x")[1] is False


def test_md_new() -> None:
    md = testcapi.md_new(0)
    assert isinstance(md, MultiDict)
    assert len(md) == 0


def test_md_popall() -> None:
    d: MultiDictStr = MultiDict()

    d.add("key1", "val1")
    d.add("key2", "val2")
    d.add("key1", "val3")
    ret = testcapi.md_popall(d, "key1")
    assert (["val1", "val3"], True) == ret
    assert {"key2": "val2"} == d


def test_md_popall_key_miss() -> None:
    d: MultiDictStr = MultiDict()
    assert testcapi.md_popall(d, "x")[1] is False


def test_md_popone() -> None:
    d: MultiDictStr = MultiDict()
    d.add("key", "val1")
    d.add("key2", "val2")
    d.add("key", "val3")

    assert ("val1", True) == testcapi.md_popone(d, "key")
    assert [("key2", "val2"), ("key", "val3")] == list(d.items())


def test_md_popone_miss() -> None:
    d: MultiDictStr = MultiDict(other="val")
    assert testcapi.md_popone(d, "x")[1] is False


def test_md_popitem() -> None:
    d: MultiDictStr = MultiDict()
    d.add("key", "val1")
    d.add("key", "val2")

    assert ("key", "val2") == testcapi.md_popitem(d)
    assert len(d) == 1
    assert [("key", "val1")] == list(d.items())


def test_md_replace() -> None:
    d: MultiDictStr = MultiDict()
    d.add("key", "val1")
    testcapi.md_replace(d, "key", "val2")
    assert "val2" == d["key"]
    testcapi.md_replace(d, "key", "val3")
    assert "val3" == d["key"]


def test_md_setdefault() -> None:
    md: MultiDictStr = MultiDict([("key", "one"), ("key", "two")], foo="bar")
    assert ("one", True) == testcapi.md_setdefault(md, "key", "three")
    assert (None, False) == testcapi.md_setdefault(md, "otherkey", "three")
    assert "otherkey" in md
    assert "three" == md["otherkey"]


def test_md_update_from_md() -> None:
    d1: MultiDictStr = MultiDict({"key": "val1", "k": "v1"})
    d2: MultiDictStr = MultiDict({"foo": "bar", "k": "v2"})

    d = d1.copy()
    testcapi.md_update_from_md(d, d2, 0)
    assert [("key", "val1"), ("k", "v1"), ("foo", "bar"), ("k", "v2")] == list(
        d.items()
    )

    d = d1.copy()
    testcapi.md_update_from_md(d, d2, 1)
    assert [("key", "val1"), ("k", "v2"), ("foo", "bar")] == list(d.items())

    d = d1.copy()
    testcapi.md_update_from_md(d, d2, 2)
    assert [("key", "val1"), ("k", "v1"), ("foo", "bar")] == list(d.items())


def test_md_update_from_dict() -> None:
    d1: MultiDictStr = MultiDict({"key": "val1", "k": "v1"})
    d2 = {"foo": "bar", "k": "v2"}

    d = d1.copy()
    testcapi.md_update_from_dict(d, d2, 0)
    assert [("key", "val1"), ("k", "v1"), ("foo", "bar"), ("k", "v2")] == list(
        d.items()
    )

    d = d1.copy()
    testcapi.md_update_from_dict(d, d2, 1)
    assert [("key", "val1"), ("k", "v2"), ("foo", "bar")] == list(d.items())

    d = d1.copy()
    testcapi.md_update_from_dict(d, d2, 2)
    assert [("key", "val1"), ("k", "v1"), ("foo", "bar")] == list(d.items())


def test_md_update_from_seq() -> None:
    d1: MultiDictStr = MultiDict({"key": "val1", "k": "v1"})
    lst = [("foo", "bar"), ("k", "v2")]

    d = d1.copy()
    testcapi.md_update_from_seq(d, lst, 0)
    assert [("key", "val1"), ("k", "v1"), ("foo", "bar"), ("k", "v2")] == list(
        d.items()
    )

    d = d1.copy()
    testcapi.md_update_from_seq(d, lst, 1)
    assert [("key", "val1"), ("k", "v2"), ("foo", "bar")] == list(d.items())

    d = d1.copy()
    testcapi.md_update_from_seq(d, lst, 2)
    assert [("key", "val1"), ("k", "v1"), ("foo", "bar")] == list(d.items())


def test_md_type() -> None:
    assert testcapi.md_type() is MultiDict


def test_md_version() -> None:
    d = MultiDict()  # type: ignore[var-annotated]
    assert testcapi.md_version(d) != 0


def test_md_proxy_new() -> None:
    mdp = testcapi.md_proxy_new(MultiDict({"a": 1}))
    assert mdp.getone("a") == 1


def test_md_proxy_contains() -> None:
    d: MultiDictProxyStr = MultiDictProxy(MultiDict([("key", "one")]))
    assert testcapi.md_proxy_contains(d, "key")


def test_md_proxy_getall() -> None:
    d: MultiDictStr = MultiDict()
    d.add("key1", "val1")
    d.add("key2", "val2")
    d.add("key1", "val3")
    proxy = MultiDictProxy(d)

    ret = testcapi.md_proxy_getall(proxy, "key1")
    assert (["val1", "val3"], True) == ret


def test_md_proxy_get_all_miss() -> None:
    d = MultiDictProxy(MultiDict([("key", "value1")], key="value2"))
    assert testcapi.md_proxy_getall(d, "x")[1] is False


def test_md_proxy_getone() -> None:
    m = MultiDict(key="val1")
    m.add("key", "val2")
    d: MultiDictProxyStr = MultiDictProxy(m)
    assert testcapi.md_proxy_getone(d, "key") == ("val1", True)


def test_md_proxy_getone_miss() -> None:
    d: MultiDictProxyStr = MultiDictProxy(MultiDict([("key", "value1")], key="value2"))
    assert testcapi.md_proxy_getone(d, "x")[1] is False


def test_md_proxy_type() -> None:
    assert testcapi.md_proxy_type() is MultiDictProxy


def test_istr_from_unicode() -> None:
    i = testcapi.istr_from_unicode("aBcD")
    assert isinstance(i, istr)
    assert i == "aBcD"


def test_istr_from_string() -> None:
    # bytes are used to represent char* in C...
    i = testcapi.istr_from_string(b"aBcD")
    assert isinstance(i, istr)
    assert i == "aBcD"


def test_istr_from_string_and_size() -> None:
    i = testcapi.istr_from_string_and_size(b"testingDO_NOT_SHOW", 7)
    assert isinstance(i, istr)
    assert i == "testing"


def test_istr_get_type() -> None:
    assert istr == testcapi.istr_get_type()


def test_ci_md_add() -> None:
    md: CIMultiDictStr = CIMultiDict()
    testcapi.ci_md_add(md, "key", "value")
    assert len(md) == 1
    assert list(md.items()) == [("key", "value")]


def test_ci_md_clear() -> None:
    previous = CIMultiDict([("key", "value")])
    md: CIMultiDictStr = previous.copy()
    testcapi.ci_md_clear(md)
    assert md != previous


def test_ci_md_contains() -> None:
    d = CIMultiDict([("key", "one")])
    assert testcapi.ci_md_contains(d, "key")
    testcapi.ci_md_del(d, "key")
    assert testcapi.ci_md_contains(d, "key") is False


def test_ci_md_del() -> None:
    d = CIMultiDict([("key", "one"), ("key", "two")], foo="bar")
    assert list(d.keys()) == ["key", "key", "foo"]

    testcapi.ci_md_del(d, "key")
    assert d == {"foo": "bar"}
    assert list(d.items()) == [("foo", "bar")]

    with pytest.raises(KeyError, match="key"):
        testcapi.ci_md_del(d, "key")


def test_ci_md_get_all() -> None:
    d: CIMultiDictStr = CIMultiDict()
    d.add("key1", "val1")
    d.add("key2", "val2")
    d.add("key1", "val3")
    ret = testcapi.ci_md_getall(d, "key1")
    assert (["val1", "val3"], True) == ret


def test_ci_md_get_all_miss() -> None:
    d = CIMultiDict([("key", "value1")], key="value2")
    assert testcapi.ci_md_getall(d, "x")[1] is False


def test_ci_md_getone() -> None:
    d: CIMultiDictStr = CIMultiDict(key="val1")
    d.add("key", "val2")
    assert testcapi.ci_md_getone(d, "key") == ("val1", True)


def test_ci_md_getone_miss() -> None:
    d: CIMultiDictStr = CIMultiDict([("key", "value1")], key="value2")
    assert testcapi.ci_md_getone(d, "x")[1] is False


def test_ci_md_new() -> None:
    md = testcapi.ci_md_new(0)
    assert isinstance(md, CIMultiDict)
    assert len(md) == 0


def test_ci_md_popall() -> None:
    d: CIMultiDictStr = CIMultiDict()

    d.add("key1", "val1")
    d.add("key2", "val2")
    d.add("key1", "val3")
    ret = testcapi.ci_md_popall(d, "key1")
    assert (["val1", "val3"], True) == ret
    assert {"key2": "val2"} == d


def test_ci_md_popall_key_miss() -> None:
    d: CIMultiDictStr = CIMultiDict()
    assert testcapi.ci_md_popall(d, "x")[1] is False


def test_ci_md_popone() -> None:
    d: CIMultiDictStr = CIMultiDict()
    d.add("key", "val1")
    d.add("key2", "val2")
    d.add("key", "val3")

    assert ("val1", True) == testcapi.ci_md_popone(d, "key")
    assert [("key2", "val2"), ("key", "val3")] == list(d.items())


def test_ci_md_popone_miss() -> None:
    d: CIMultiDictStr = CIMultiDict(other="val")
    assert testcapi.ci_md_popone(d, "x")[1] is False


def test_ci_md_popitem() -> None:
    d: CIMultiDictStr = CIMultiDict()
    d.add("key", "val1")
    d.add("key", "val2")

    assert ("key", "val2") == testcapi.ci_md_popitem(d)
    assert len(d) == 1
    assert [("key", "val1")] == list(d.items())


def test_ci_md_replace() -> None:
    d: CIMultiDictStr = CIMultiDict()
    d.add("key", "val1")
    testcapi.ci_md_replace(d, "key", "val2")
    assert "val2" == d["key"]
    testcapi.ci_md_replace(d, "key", "val3")
    assert "val3" == d["key"]


def test_ci_md_setdefault() -> None:
    md: CIMultiDictStr = CIMultiDict([("key", "one"), ("key", "two")], foo="bar")
    assert ("one", True) == testcapi.ci_md_setdefault(md, "key", "three")
    assert (None, False) == testcapi.ci_md_setdefault(md, "otherkey", "three")
    assert "otherkey" in md
    assert "three" == md["otherkey"]


def test_ci_md_update_from_md() -> None:
    d1: CIMultiDictStr = CIMultiDict({"key": "val1", "k": "v1"})
    d2: CIMultiDictStr = CIMultiDict({"foo": "bar", "k": "v2"})

    d = d1.copy()
    testcapi.ci_md_update_from_md(d, d2, 0)
    assert [("key", "val1"), ("k", "v1"), ("foo", "bar"), ("k", "v2")] == list(
        d.items()
    )

    d = d1.copy()
    testcapi.ci_md_update_from_md(d, d2, 1)
    assert [("key", "val1"), ("k", "v2"), ("foo", "bar")] == list(d.items())

    d = d1.copy()
    testcapi.ci_md_update_from_md(d, d2, 2)
    assert [("key", "val1"), ("k", "v1"), ("foo", "bar")] == list(d.items())


def test_ci_md_update_from_dict() -> None:
    d1: CIMultiDictStr = CIMultiDict({"key": "val1", "k": "v1"})
    d2 = {"foo": "bar", "k": "v2"}

    d = d1.copy()
    testcapi.ci_md_update_from_dict(d, d2, 0)
    assert [("key", "val1"), ("k", "v1"), ("foo", "bar"), ("k", "v2")] == list(
        d.items()
    )

    d = d1.copy()
    testcapi.ci_md_update_from_dict(d, d2, 1)
    assert [("key", "val1"), ("k", "v2"), ("foo", "bar")] == list(d.items())

    d = d1.copy()
    testcapi.ci_md_update_from_dict(d, d2, 2)
    assert [("key", "val1"), ("k", "v1"), ("foo", "bar")] == list(d.items())


def test_ci_md_update_from_seq() -> None:
    d1: CIMultiDictStr = CIMultiDict({"key": "val1", "k": "v1"})
    lst = [("foo", "bar"), ("k", "v2")]

    d = d1.copy()
    testcapi.ci_md_update_from_seq(d, lst, 0)
    assert [("key", "val1"), ("k", "v1"), ("foo", "bar"), ("k", "v2")] == list(
        d.items()
    )

    d = d1.copy()
    testcapi.ci_md_update_from_seq(d, lst, 1)
    assert [("key", "val1"), ("k", "v2"), ("foo", "bar")] == list(d.items())

    d = d1.copy()
    testcapi.ci_md_update_from_seq(d, lst, 2)
    assert [("key", "val1"), ("k", "v1"), ("foo", "bar")] == list(d.items())


def test_ci_md_type() -> None:
    assert testcapi.ci_md_type() is CIMultiDict


def test_ci_md_version() -> None:
    d = CIMultiDict()  # type: ignore[var-annotated]
    assert testcapi.ci_md_version(d) != 0


def test_ci_md_proxy_new() -> None:
    mdp = testcapi.ci_md_proxy_new(CIMultiDict({"a": 1}))
    assert mdp.getone("a") == 1


def test_ci_md_proxy_contains() -> None:
    d: CIMultiDictProxyStr = CIMultiDictProxy(CIMultiDict([("key", "one")]))
    assert testcapi.ci_md_proxy_contains(d, "key")


def test_ci_md_proxy_getall() -> None:
    d: CIMultiDictStr = CIMultiDict()
    d.add("key1", "val1")
    d.add("key2", "val2")
    d.add("key1", "val3")
    proxy = CIMultiDictProxy(d)

    ret = testcapi.ci_md_proxy_getall(proxy, "key1")
    assert (["val1", "val3"], True) == ret


def test_ci_md_proxy_get_all_miss() -> None:
    d = CIMultiDictProxy(CIMultiDict([("key", "value1")], key="value2"))
    assert testcapi.ci_md_proxy_getall(d, "x")[1] is False


def test_ci_md_proxy_getone() -> None:
    m = CIMultiDict(key="val1")
    m.add("key", "val2")
    d: CIMultiDictProxyStr = CIMultiDictProxy(m)
    assert testcapi.ci_md_proxy_getone(d, "key") == ("val1", True)


def test_ci_md_proxy_getone_miss() -> None:
    d: CIMultiDictProxyStr = CIMultiDictProxy(
        CIMultiDict([("key", "value1")], key="value2")
    )
    assert testcapi.ci_md_proxy_getone(d, "x")[1] is False


def test_ci_md_proxy_type() -> None:
    assert testcapi.ci_md_proxy_type() is CIMultiDictProxy
