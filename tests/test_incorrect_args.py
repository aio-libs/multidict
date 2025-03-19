"""Test passing invalid arguments to the methods of the MultiDict class."""

from dataclasses import dataclass
from typing import Any

import pytest

from multidict import MultiDict


@dataclass(frozen=True)
class InvalidTestedMethodArgs:
    """A set of arguments passed to methods under test."""

    test_id: str
    positional: tuple[Any, ...]
    keyword: dict[str, Any]

    def __str__(self) -> str:
        """Render a test identifier as a string."""
        return self.test_id


COMMON_ARGS = pytest.mark.parametrize(
    "tested_method_args",
    (
        InvalidTestedMethodArgs("no_args", (), {}),
        InvalidTestedMethodArgs("too_many_args", ("a", "b", "c"), {}),
        InvalidTestedMethodArgs("wrong_kwarg", (), {"wrong": 1}),
        InvalidTestedMethodArgs(
            "wrong_kwarg_and_too_many_args",
            ("a",),
            {"wrong": 1},
        ),
    ),
    ids=str,
)


@COMMON_ARGS
def test_getall_args(
    any_multidict_class: type[MultiDict[int]],
    tested_method_args: InvalidTestedMethodArgs,
) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.getall(*tested_method_args.positional, **tested_method_args.keyword)


@COMMON_ARGS
def test_getone_args(
    any_multidict_class: type[MultiDict[int]],
    tested_method_args: InvalidTestedMethodArgs,
) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.getone(*tested_method_args.positional, **tested_method_args.keyword)


@COMMON_ARGS
def test_get_args(
    any_multidict_class: type[MultiDict[int]],
    tested_method_args: InvalidTestedMethodArgs,
) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.get(*tested_method_args.positional, **tested_method_args.keyword)


@COMMON_ARGS
def test_setdefault_args(
    any_multidict_class: type[MultiDict[int]],
    tested_method_args: InvalidTestedMethodArgs,
) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.setdefault(
            *tested_method_args.positional,
            **tested_method_args.keyword,
        )


@COMMON_ARGS
def test_popone_args(
    any_multidict_class: type[MultiDict[int]],
    tested_method_args: InvalidTestedMethodArgs,
) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.popone(*tested_method_args.positional, **tested_method_args.keyword)


@COMMON_ARGS
def test_pop_args(
    any_multidict_class: type[MultiDict[int]],
    tested_method_args: InvalidTestedMethodArgs,
) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.pop(*tested_method_args.positional, **tested_method_args.keyword)


@COMMON_ARGS
def test_popall_args(
    any_multidict_class: type[MultiDict[int]],
    tested_method_args: InvalidTestedMethodArgs,
) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    with pytest.raises(TypeError):
        d.popall(*tested_method_args.positional, **tested_method_args.keyword)
