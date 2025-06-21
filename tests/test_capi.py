import multidict
import pytest

pytest.importorskip("multidict._multidict")
testcapi = pytest.importorskip("testcapi")

pytestmark = pytest.mark.capi

MultiDictStr = multidict.MultiDict[str]


# TODO: Diagnose and Fix Segfault
@pytest.mark.skip("md_new() triggers a segfault afterwards no clue as to why...")
def test_md_new() -> None:
    # XXX: Triggers segfault
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
