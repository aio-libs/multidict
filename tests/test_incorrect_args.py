
"""Test passing invalid arguments to the methods of the MultiDict class."""
import pytest
from multidict import MultiDict


def test_getall_args(
    any_multidict_class: type[MultiDict[int]]
) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.getall()
    with pytest.raises(TypeError):
        d.getall("a", "b", "c")

def test_getone_args(
    any_multidict_class: type[MultiDict[int]]
) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.getone()
    with pytest.raises(TypeError):
        d.getone("a", "b", "c")

def test_get_args(
    any_multidict_class: type[MultiDict[int]]
) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.get()
    with pytest.raises(TypeError):
        d.get("a", "b", "c")

def test_setdefault_args(any_multidict_class: type[MultiDict[int]]) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.setdefault()
    with pytest.raises(TypeError):
        d.setdefault("a", "b", "c")

def test_popone_args(
    any_multidict_class: type[MultiDict[int]]
) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.popone()
    with pytest.raises(TypeError):
        d.popone("a", "b", "c")

def test_pop_args(
    any_multidict_class: type[MultiDict[int]]
) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.pop()
    with pytest.raises(TypeError):
        d.pop("a", "b", "c")

def test_popall_args(
    any_multidict_class: type[MultiDict[int]]
) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.popall()
    with pytest.raises(TypeError):
        d.popall("a", "b", "c")
