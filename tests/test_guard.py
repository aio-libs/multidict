import pytest

from multidict import MultiDict


def test_guard_items(
    cs_md_class: type[MultiDict[str]],
) -> None:
    md = cs_md_class({"a": "b"})
    it = iter(md.items())
    md["a"] = "c"
    with pytest.raises(RuntimeError):
        next(it)


def test_guard_keys(
    cs_md_class: type[MultiDict[str]],
) -> None:
    md = cs_md_class({"a": "b"})
    it = iter(md.keys())
    md["a"] = "c"
    with pytest.raises(RuntimeError):
        next(it)


def test_guard_values(
    cs_md_class: type[MultiDict[str]],
) -> None:
    md = cs_md_class({"a": "b"})
    it = iter(md.values())
    md["a"] = "c"
    with pytest.raises(RuntimeError):
        next(it)
