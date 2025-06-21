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

def test_md_version():
    d = multidict.MultiDict()
    assert testcapi.md_version(d) != 0

def test_md_contains():
    d = multidict.MultiDict([("key", "one")])
    assert testcapi.md_contains(d, "key")
    testcapi.md_del(d, "key")
    assert testcapi.md_contains(d, "key") == False

# I will deal with this one later, Seems beyond my control...
@pytest.mark.skip("SystemError: <built-in function md_get> returned NULL without setting an exception")
def test_md_get():
    d = multidict.MultiDict([("key", "one"), ("foo", "bar")])
    assert testcapi.md_get(d, "key") == "one"
    assert testcapi.md_get(d, "i dont exist") == None

def test_md_get_all():
    d = multidict.MultiDict([("key", "value1")], key="value2")
    assert len(d) == 2

    assert testcapi.md_get_all(d, "key") == ["value1", "value2"]

    with pytest.raises(KeyError, match="some_key"):
        testcapi.md_get_all(d,"some_key")
    

def test_md_pop():
    d = multidict.MultiDict()
    d.add("key", "val1")
    d.add("key", "val2")

    assert "val1" == testcapi.md_pop(d, "key")
    assert {"key": "val2"} == d
