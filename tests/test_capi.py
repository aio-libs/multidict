import multidict
import pytest

pytest.importorskip("multidict._multidict")
testcapi = pytest.importorskip("testcapi")

pytestmark = pytest.mark.capi

MultiDictStr = multidict.MultiDict[str]


def test_md_new() -> None:
    md = testcapi.md_new(0)
    assert isinstance(md, multidict.MultiDict)
    assert len(md) == 0


def test_md_type() -> None:
    assert testcapi.md_type() is multidict.MultiDict


def test_md_add() -> None:
    md: MultiDictStr = multidict.MultiDict()
    testcapi.md_add(md, "key", "value")
    assert len(md) == 1
    assert list(md.items()) == [("key", "value")]


def test_md_clear() -> None:
    previous = multidict.MultiDict([("key", "value")])
    md: MultiDictStr = previous.copy()
    testcapi.md_clear(md)
    assert md != previous


def test_set_default() -> None:
    md: MultiDictStr = multidict.MultiDict([("key", "one"), ("key", "two")], foo="bar")
    assert "one" == testcapi.md_set_default(md, "key", "three")
    assert "three" == testcapi.md_set_default(md, "otherkey", "three")
    assert "otherkey" in md
    assert "three" == md["otherkey"]


def test_del() -> None:
    d = multidict.MultiDict([("key", "one"), ("key", "two")], foo="bar")
    assert list(d.keys()) == ["key", "key", "foo"]

    testcapi.md_del(d, "key")
    assert d == {"foo": "bar"}
    assert list(d.items()) == [("foo", "bar")]

    with pytest.raises(KeyError, match="key"):
        testcapi.md_del(d, "key")


def test_md_version() -> None:
    d = multidict.MultiDict()  # type: ignore[var-annotated]
    assert testcapi.md_version(d) != 0


def test_md_contains() -> None:
    d = multidict.MultiDict([("key", "one")])
    assert testcapi.md_contains(d, "key")
    testcapi.md_del(d, "key")
    assert testcapi.md_contains(d, "key") is False


# I will deal with this one later, Seems beyond my control...


def test_md_get() -> None:
    d = multidict.MultiDict([("key", "one"), ("foo", "bar")])
    assert testcapi.md_get(d, "key") == "one"
    assert testcapi.md_get(d, "i dont exist") is None


def test_md_get_all() -> None:
    d = multidict.MultiDict([("key", "value1")], key="value2")
    assert testcapi.md_get_all(d, "key") == ["value1", "value2"]


def test_md_get_all_excpection() -> None:
    d = multidict.MultiDict([("key", "value1")], key="value2")
    with pytest.raises(KeyError, match="some_key"):
        testcapi.md_get_all(d, "some_key")


def test_md_pop() -> None:
    d: MultiDictStr = multidict.MultiDict()
    d.add("key", "val1")
    d.add("key", "val2")

    assert "val1" == testcapi.md_pop(d, "key")
    assert {"key": "val2"} == d


def test_md_popone() -> None:
    d: MultiDictStr = multidict.MultiDict()
    d.add("key", "val1")
    d.add("key2", "val2")
    d.add("key", "val3")

    assert "val1" == testcapi.md_popone(d, "key")
    assert [("key2", "val2"), ("key", "val3")] == list(d.items())


def test_md_popone_exception() -> None:
    md: MultiDictStr = multidict.MultiDict(other="val")
    with pytest.raises(KeyError, match="key"):
        testcapi.md_popone(md, "key")


def test_md_popall() -> None:
    d: MultiDictStr = multidict.MultiDict()

    d.add("key1", "val1")
    d.add("key2", "val2")
    d.add("key1", "val3")
    ret = testcapi.md_popall(d, "key1")
    assert ["val1", "val3"] == ret
    assert {"key2": "val2"} == d


def test_md_popall_key_error() -> None:
    d: MultiDictStr = multidict.MultiDict()
    with pytest.raises(KeyError, match="key"):
        testcapi.md_popall(d, "key")


def test_md_popitem() -> None:
    d: MultiDictStr = multidict.MultiDict()
    d.add("key", "val1")
    d.add("key", "val2")

    assert ("key", "val2") == testcapi.md_popitem(d)
    assert len(d) == 1
    assert [("key", "val1")] == list(d.items())


def test_md_replace() -> None:
    d: MultiDictStr = multidict.MultiDict()
    d.add("key", "val1")
    testcapi.md_replace(d, "key", "val2")
    assert "val2" == d["key"]
    testcapi.md_replace(d, "key", "val3")
    assert "val3" == d["key"]


def test_md_update_from_md() -> None:
    d1: MultiDictStr = multidict.MultiDict()
    d1.add("key", "val1")
    d2: MultiDictStr = multidict.MultiDict()
    d2.add("foo", "bar")
    testcapi.md_update_from_md(d1, d2, False)
    assert [("key", "val1"), ("foo", "bar")] == list(d1.items())


def test_md_update_from_dict() -> None:
    d1: MultiDictStr = multidict.MultiDict()
    d1.add("key", "val1")
    testcapi.md_update_from_dict(d1, {"foo": "bar"}, False)
    assert [("key", "val1"), ("foo", "bar")] == list(d1.items())


def test_md_update_from_seq() -> None:
    d1: MultiDictStr = multidict.MultiDict()
    testcapi.md_update_from_seq(d1, [("key", "val1"), ("foo", "bar")], False)
    assert [("key", "val1"), ("foo", "bar")] == list(d1.items())


def test_md_equals_1() -> None:
    d: MultiDictStr = multidict.MultiDict([("key", "val1")])
    assert testcapi.md_equals(d, multidict.MultiDict([("key", "val1")]))
    assert not testcapi.md_equals(d, multidict.MultiDict([("key", "val2")]))
    assert testcapi.md_equals(d, {"key": "val1"})
    assert not testcapi.md_equals(d, {"key": "not it"})
