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
    md: MultiDictStr = multidict.MultiDict(key="val")
    testcapi.md_clear(md)
    assert len(md) == 0


@pytest.mark.parametrize(
    "key, expected",
    [
        pytest.param("key", ("val", True), id="found"),
        pytest.param("key2", ("default", False), id="notfound"),
    ],
)
def test_md_setdefault(key: str, expected: tuple[str, bool]) -> None:
    md: MultiDictStr = multidict.MultiDict(key="val")
    ret = testcapi.md_setdefault(md, key, "default")
    assert ret == expected
