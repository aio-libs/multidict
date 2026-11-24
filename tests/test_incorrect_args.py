"""Test passing invalid arguments to the methods of the MultiDict class."""

from dataclasses import dataclass
from typing import cast

import pytest

from multidict import MultiDict


@dataclass(frozen=True)
class InvalidTestedMethodArgs:
    """A set of arguments passed to methods under test."""

    test_id: str
    positional: tuple[object, ...]
    keyword: dict[str, object]

    def __str__(self) -> str:
        """Render a test identifier as a string."""
        return self.test_id


@pytest.fixture(
    scope="module",
    params=(
        InvalidTestedMethodArgs("no_args", (), {}),
        InvalidTestedMethodArgs("too_many_args", ("a", "b", "c"), {}),
        InvalidTestedMethodArgs("wrong_kwarg", (), {"wrong": 1}),
        InvalidTestedMethodArgs(
            "wrong_kwarg_and_too_many_args",
            ("a",),
            {"wrong": 1},
        ),
        InvalidTestedMethodArgs(
            "key_passed_positionally_and_by_name",
            ("a", "b"),
            {"key": "c"},
        ),
    ),
    ids=str,
)
def tested_method_args(
    request: pytest.FixtureRequest,
) -> InvalidTestedMethodArgs:
    """Return an instance of a parameter set."""
    return cast(InvalidTestedMethodArgs, request.param)


@pytest.fixture(scope="module")
def multidict_object(
    any_multidict_class: type[MultiDict[int]],
) -> MultiDict[int]:
    return any_multidict_class([("a", 1), ("a", 2)])


def test_getall_args(
    multidict_object: MultiDict[int],
    tested_method_args: InvalidTestedMethodArgs,
) -> None:
    with pytest.raises(TypeError, match=r".*argument.*"):
        multidict_object.getall(
            *tested_method_args.positional,
            **tested_method_args.keyword,
        )


def test_getone_args(
    multidict_object: MultiDict[int],
    tested_method_args: InvalidTestedMethodArgs,
) -> None:
    with pytest.raises(TypeError, match=r".*argument.*"):
        multidict_object.getone(
            *tested_method_args.positional,
            **tested_method_args.keyword,
        )


def test_get_args(
    multidict_object: MultiDict[int],
    tested_method_args: InvalidTestedMethodArgs,
) -> None:
    with pytest.raises(TypeError, match=r".*argument.*"):
        multidict_object.get(
            *tested_method_args.positional,
            **tested_method_args.keyword,
        )


def test_setdefault_args(
    multidict_object: MultiDict[int],
    tested_method_args: InvalidTestedMethodArgs,
) -> None:
    with pytest.raises(TypeError, match=r".*argument.*"):
        multidict_object.setdefault(
            *tested_method_args.positional,
            **tested_method_args.keyword,
        )


def test_add_args(
    multidict_object: MultiDict[int],
    tested_method_args: InvalidTestedMethodArgs,
) -> None:
    with pytest.raises(TypeError, match=r".*argument.*"):
        multidict_object.add(
            *tested_method_args.positional,
            **tested_method_args.keyword,
        )


def test_popone_args(
    multidict_object: MultiDict[int],
    tested_method_args: InvalidTestedMethodArgs,
) -> None:
    with pytest.raises(TypeError, match=r".*argument.*"):
        multidict_object.popone(
            *tested_method_args.positional,
            **tested_method_args.keyword,
        )


def test_pop_args(
    multidict_object: MultiDict[int],
    tested_method_args: InvalidTestedMethodArgs,
) -> None:
    with pytest.raises(TypeError, match=r".*argument.*"):
        multidict_object.pop(
            *tested_method_args.positional,
            **tested_method_args.keyword,
        )


def test_popall_args(
    multidict_object: MultiDict[int],
    tested_method_args: InvalidTestedMethodArgs,
) -> None:
    with pytest.raises(TypeError, match=r".*argument.*"):
        multidict_object.popall(
            *tested_method_args.positional,
            **tested_method_args.keyword,
        )


@pytest.mark.parametrize(
    ("method_name", "second_argname"),
    (
        ("getall", "default"),
        ("getone", "default"),
        ("get", "default"),
        ("add", "value"),
        ("setdefault", "default"),
        ("popone", "default"),
        ("pop", "default"),
        ("popall", "default"),
    ),
)
def test_key_passed_positionally_and_by_name(
    multidict_object: MultiDict[int],
    method_name: str,
    second_argname: str,
) -> None:
    """Reject a key given both positionally and by name."""
    with pytest.raises(
        TypeError,
        match=r"got multiple values for argument 'key'$",
    ):
        getattr(multidict_object, method_name)(
            "a",
            **{second_argname: 1, "key": "b"},
        )


def test_add_missing_both_args(multidict_object: MultiDict[int]) -> None:
    with pytest.raises(
        TypeError,
        match=r"missing 2 required positional arguments: 'key' and 'value'$",
    ):
        multidict_object.add()  # type: ignore[call-arg]


def test_add_missing_value(multidict_object: MultiDict[int]) -> None:
    with pytest.raises(
        TypeError,
        match=r"missing 1 required positional argument: 'value'$",
    ):
        multidict_object.add("a")  # type: ignore[call-arg]


def test_add_too_many_args_reports_a_fixed_count(
    multidict_object: MultiDict[int],
) -> None:
    """``add()`` takes a fixed number of arguments, not a range."""
    with pytest.raises(
        TypeError,
        match=r"takes (exactly )?\d+ positional arguments",
    ):
        multidict_object.add("a", "b", "c")  # type: ignore[call-arg]


def test_getall_too_many_args_reports_a_range(
    multidict_object: MultiDict[int],
) -> None:
    """``getall()`` has an optional second argument, so it takes a range."""
    with pytest.raises(
        TypeError,
        match=r"takes from \d+ to \d+ positional arguments",
    ):
        multidict_object.getall("a", "b", "c")


class _FalsyNonIterable:
    """Neither a mapping nor an iterable, but false in a boolean context."""

    def __len__(self) -> int:
        return 0


_FALSY_NON_ITERABLES = pytest.mark.parametrize(
    "arg",
    (None, 0, False, 0.0, 0j, _FalsyNonIterable()),
    ids=("None", "int", "bool", "float", "complex", "len_zero"),
)


@_FALSY_NON_ITERABLES
def test_init_rejects_falsy_non_iterable(
    any_multidict_class: type[MultiDict[int]],
    arg: object,
) -> None:
    """A falsy argument is not taken for a missing one."""
    with pytest.raises(TypeError, match="not iterable"):
        any_multidict_class(arg)
    with pytest.raises(TypeError, match="not iterable"):
        any_multidict_class(arg, a=1)


@_FALSY_NON_ITERABLES
@pytest.mark.parametrize("method_name", ("extend", "update", "merge"))
def test_bulk_methods_reject_falsy_non_iterable(
    any_multidict_class: type[MultiDict[int]],
    method_name: str,
    arg: object,
) -> None:
    """A falsy argument is not taken for a missing one."""
    d = any_multidict_class([("a", 1)])
    with pytest.raises(TypeError, match="not iterable"):
        getattr(d, method_name)(arg)
    assert list(d.items()) == [("a", 1)]
