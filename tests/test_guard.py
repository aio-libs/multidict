import pytest

from multidict import CIMultiDict, MultiDict

_MD_Classes = type[MultiDict[str]] | type[CIMultiDict[str]]


def test_guard_items(
    case_sensitive_multidict_class: type[MultiDict[str]],
) -> None:
    md = case_sensitive_multidict_class({"a": "b"})
    it = iter(md.items())
    md["a"] = "c"
    with pytest.raises(RuntimeError):
        next(it)


def test_guard_keys(
    case_sensitive_multidict_class: type[MultiDict[str]],
) -> None:
    md = case_sensitive_multidict_class({"a": "b"})
    it = iter(md.keys())
    md["a"] = "c"
    with pytest.raises(RuntimeError):
        next(it)


def test_guard_values(
    case_sensitive_multidict_class: type[MultiDict[str]],
) -> None:
    md = case_sensitive_multidict_class({"a": "b"})
    it = iter(md.values())
    md["a"] = "c"
    with pytest.raises(RuntimeError):
        next(it)


def test_guard_update_existing_key(any_multidict_class: _MD_Classes) -> None:
    md = any_multidict_class({"a": "b", "z": "y"})
    it = iter(md)
    next(it)
    md.update({"a": "c"})
    with pytest.raises(RuntimeError):
        next(it)


def test_guard_items_update_existing_key(any_multidict_class: _MD_Classes) -> None:
    md = any_multidict_class({"a": "b"})
    it = iter(md.items())
    md.update({"a": "c"})
    with pytest.raises(RuntimeError):
        next(it)


def test_guard_keys_update_existing_key(any_multidict_class: _MD_Classes) -> None:
    md = any_multidict_class({"a": "b"})
    it = iter(md.keys())
    md.update({"a": "c"})
    with pytest.raises(RuntimeError):
        next(it)


def test_guard_values_update_existing_key(any_multidict_class: _MD_Classes) -> None:
    md = any_multidict_class({"a": "b"})
    it = iter(md.values())
    md.update({"a": "c"})
    with pytest.raises(RuntimeError):
        next(it)
