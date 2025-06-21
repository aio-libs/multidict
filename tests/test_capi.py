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
