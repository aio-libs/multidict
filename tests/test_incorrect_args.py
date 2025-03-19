"""Test passing invalid arguments to the methods of the MultiDict class."""

from typing import Any

import pytest

from multidict import MultiDict

COMMON_ARGS = pytest.mark.parametrize(
    ("positional_args", "keyword_args"),
    (
        ((), {}),
        (("a", "b", "c"), {}),
        ((), {"wrong": 1}),
        (("a",), {"wrong": 1}),
    ),
    ids=["no_args", "too_many_args", "wrong_kwarg", "wrong_kwarg_and_too_many_args"],
)


@COMMON_ARGS
def test_getall_args(
    any_multidict_class: type[MultiDict[int]],
    positional_args: tuple[Any, ...],
    keyword_args: dict[str, Any],
) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.getall(*positional_args, **keyword_args)


@COMMON_ARGS
def test_getone_args(
    any_multidict_class: type[MultiDict[int]],
    positional_args: tuple[Any, ...],
    keyword_args: dict[str, Any],
) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.getone(*positional_args, **keyword_args)


@COMMON_ARGS
def test_get_args(
    any_multidict_class: type[MultiDict[int]],
    positional_args: tuple[Any, ...],
    keyword_args: dict[str, Any],
) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.get(*positional_args, **keyword_args)


@COMMON_ARGS
def test_setdefault_args(
    any_multidict_class: type[MultiDict[int]],
    positional_args: tuple[Any, ...],
    keyword_args: dict[str, Any],
) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.setdefault(*positional_args, **keyword_args)


@COMMON_ARGS
def test_popone_args(
    any_multidict_class: type[MultiDict[int]],
    positional_args: tuple[Any, ...],
    keyword_args: dict[str, Any],
) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.popone(*positional_args, **keyword_args)


@COMMON_ARGS
def test_pop_args(
    any_multidict_class: type[MultiDict[int]],
    positional_args: tuple[Any, ...],
    keyword_args: dict[str, Any],
) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.pop(*positional_args, **keyword_args)


@COMMON_ARGS
def test_popall_args(
    any_multidict_class: type[MultiDict[int]],
    positional_args: tuple[Any, ...],
    keyword_args: dict[str, Any],
) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.popall(*positional_args, **keyword_args)
