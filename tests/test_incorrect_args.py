"""Test passing invalid arguments to the methods of the MultiDict class."""

import pytest

from multidict import MultiDict


def test_getall_args(any_multidict_class: type[MultiDict[int]]) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.getall()
    with pytest.raises(TypeError):
        d.getall("a", "b", "c")
    with pytest.raises(TypeError):
        d.getall(wrong=1)
    with pytest.raises(TypeError):
        d.getall("a", wrong=1)


def test_getone_args(any_multidict_class: type[MultiDict[int]]) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.getone()
    with pytest.raises(TypeError):
        d.getone("a", "b", "c")
    with pytest.raises(TypeError):
        d.getone(wrong=1)
    with pytest.raises(TypeError):
        d.getone("a", wrong=1)


def test_get_args(any_multidict_class: type[MultiDict[int]]) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.get()
    with pytest.raises(TypeError):
        d.get("a", "b", "c")
    with pytest.raises(TypeError):
        d.get(wrong=1)
    with pytest.raises(TypeError):
        d.get("a", wrong=1)


def test_setdefault_args(any_multidict_class: type[MultiDict[int]]) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.setdefault()
    with pytest.raises(TypeError):
        d.setdefault("a", "b", "c")
    with pytest.raises(TypeError):
        d.setdefault(wrong=1)
    with pytest.raises(TypeError):
        d.setdefault("a", wrong=1)


def test_popone_args(any_multidict_class: type[MultiDict[int]]) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.popone()
    with pytest.raises(TypeError):
        d.popone("a", "b", "c")
    with pytest.raises(TypeError):
        d.popone(wrong=1)
    with pytest.raises(TypeError):
        d.popone("a", wrong=1)


def test_pop_args(any_multidict_class: type[MultiDict[int]]) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.pop()
    with pytest.raises(TypeError):
        d.pop("a", "b", "c")
    with pytest.raises(TypeError):
        d.pop(wrong=1)
    with pytest.raises(TypeError):
        d.pop("a", wrong=1)


def test_popall_args(any_multidict_class: type[MultiDict[int]]) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.popall()
    with pytest.raises(TypeError):
        d.popall("a", "b", "c")
    with pytest.raises(TypeError):
        d.popall(wrong=1)
    with pytest.raises(TypeError):
        d.popall("a", wrong=1)
