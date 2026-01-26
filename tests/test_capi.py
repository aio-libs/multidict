import multidict
import pytest

pytest.importorskip("multidict._multidict")
testcapi = pytest.importorskip("testcapi")

pytestmark = pytest.mark.capi

MultiDictStr = multidict.MultiDict[str]


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


def test_md_contains() -> None:
    d = multidict.MultiDict([("key", "one")])
    assert testcapi.md_contains(d, "key")
    testcapi.md_del(d, "key")
    assert testcapi.md_contains(d, "key") is False


def test_md_del() -> None:
    d = multidict.MultiDict([("key", "one"), ("key", "two")], foo="bar")
    assert list(d.keys()) == ["key", "key", "foo"]

    testcapi.md_del(d, "key")
    assert d == {"foo": "bar"}
    assert list(d.items()) == [("foo", "bar")]

    with pytest.raises(KeyError, match="key"):
        testcapi.md_del(d, "key")


def test_md_get_all() -> None:
    d: MultiDictStr = multidict.MultiDict()
    d.add("key1", "val1")
    d.add("key2", "val2")
    d.add("key1", "val3")
    ret = testcapi.md_getall(d, "key1")
    assert (["val1", "val3"], True) == ret


def test_md_get_all_miss() -> None:
    d = multidict.MultiDict([("key", "value1")], key="value2")
    assert testcapi.md_getall(d, "x")[1] is False


def test_md_getone() -> None:
    d: MultiDictStr = multidict.MultiDict(key="val1")
    d.add("key", "val2")
    assert testcapi.md_getone(d, "key") == ("val1", True)


def test_md_getone_miss() -> None:
    d: MultiDictStr = multidict.MultiDict([("key", "value1")], key="value2")
    assert testcapi.md_getone(d, "x")[1] is False


@pytest.mark.skip(
    "GC/WeakRef Releated Bug: SEE: https://github.com/aio-libs/multidict/pull/1190#discussion_r2162536248"
)
def test_md_new() -> None:
    md = testcapi.md_new(0)
    assert isinstance(md, multidict.MultiDict)
    assert len(md) == 0


def test_md_popall() -> None:
    d: MultiDictStr = multidict.MultiDict()

    d.add("key1", "val1")
    d.add("key2", "val2")
    d.add("key1", "val3")
    ret = testcapi.md_popall(d, "key1")
    assert (["val1", "val3"], True) == ret
    assert {"key2": "val2"} == d


def test_md_popall_key_miss() -> None:
    d: MultiDictStr = multidict.MultiDict()
    assert testcapi.md_popall(d, "x")[1] is False


def test_md_popone() -> None:
    d: MultiDictStr = multidict.MultiDict()
    d.add("key", "val1")
    d.add("key2", "val2")
    d.add("key", "val3")

    assert ("val1", True) == testcapi.md_popone(d, "key")
    assert [("key2", "val2"), ("key", "val3")] == list(d.items())


def test_md_popone_miss() -> None:
    d: MultiDictStr = multidict.MultiDict(other="val")
    assert testcapi.md_popone(d, "x")[1] is False


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


def test_md_setdefault() -> None:
    md: MultiDictStr = multidict.MultiDict([("key", "one"), ("key", "two")], foo="bar")
    assert ("one", True) == testcapi.md_setdefault(md, "key", "three")
    assert (None, False) == testcapi.md_setdefault(md, "otherkey", "three")
    assert "otherkey" in md
    assert "three" == md["otherkey"]


def test_md_update_from_md() -> None:
    d1: MultiDictStr = multidict.MultiDict({"key": "val1", "k": "v1"})
    d2: MultiDictStr = multidict.MultiDict({"foo": "bar", "k": "v2"})

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
    d1: MultiDictStr = multidict.MultiDict({"key": "val1", "k": "v1"})
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
    d1: MultiDictStr = multidict.MultiDict({"key": "val1", "k": "v1"})
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
    assert testcapi.md_type() is multidict.MultiDict


def test_md_version() -> None:
    d = multidict.MultiDict()  # type: ignore[var-annotated]
    assert testcapi.md_version(d) != 0
