import gc
import sys
from collections.abc import Callable

import pytest

IMPLEMENTATION = getattr(sys, "implementation")  # to suppress mypy error


def test_ctor(case_insensitive_str_class: type[str]) -> None:
    s = case_insensitive_str_class()
    assert "" == s


def test_ctor_str(case_insensitive_str_class: type[str]) -> None:
    s = case_insensitive_str_class("aBcD")
    assert "aBcD" == s


def test_ctor_istr(case_insensitive_str_class: type[str]) -> None:
    s = case_insensitive_str_class("A")
    s2 = case_insensitive_str_class(s)
    assert "A" == s
    assert s == s2


def test_ctor_buffer(case_insensitive_str_class: type[str]) -> None:
    s = case_insensitive_str_class(b"aBc")
    assert "b'aBc'" == s


def test_ctor_repr(case_insensitive_str_class: type[str]) -> None:
    s = case_insensitive_str_class(None)
    assert "None" == s


def test_ctor_encoding(case_insensitive_str_class: type[str]) -> None:
    s = case_insensitive_str_class(b"aBc", "utf-8")
    assert "aBc" == s
    assert "abc" == s.lower()


def test_ctor_encoding_and_errors(case_insensitive_str_class: type[str]) -> None:
    s = case_insensitive_str_class(b"\xff", "utf-8", "replace")
    assert "\ufffd" == s


def test_ctor_keyword_arguments(case_insensitive_str_class: type[str]) -> None:
    s = case_insensitive_str_class(object=b"aBc", encoding="utf-8", errors="strict")
    assert "aBc" == s


def test_ctor_object_keyword_only(case_insensitive_str_class: type[str]) -> None:
    s = case_insensitive_str_class(object="aBc")
    assert "aBc" == s


def test_ctor_too_many_arguments(case_insensitive_str_class: type[str]) -> None:
    with pytest.raises(TypeError):
        case_insensitive_str_class("a", "utf-8", "strict", "extra")  # type: ignore[call-overload]


def test_ctor_unknown_keyword(case_insensitive_str_class: type[str]) -> None:
    with pytest.raises(TypeError):
        case_insensitive_str_class(bogus=1)  # type: ignore[call-overload]


def test_ctor_encoding_rejects_str(case_insensitive_str_class: type[str]) -> None:
    with pytest.raises(TypeError):
        case_insensitive_str_class("aBc", "utf-8")  # type: ignore[call-overload]


def test_subclass_rejected(case_insensitive_str_class: type[str]) -> None:
    with pytest.raises(TypeError, match="is not an acceptable base type"):
        type("Sub", (case_insensitive_str_class,), {})


def test_multiple_inheritance_subclass_rejected(
    case_insensitive_str_class: type[str],
) -> None:
    class Mixin:
        pass

    with pytest.raises(TypeError, match="is not an acceptable base type"):
        type("Sub", (Mixin, case_insensitive_str_class), {})


def test_str(case_insensitive_str_class: type[str]) -> None:
    s = case_insensitive_str_class("aBcD")
    s1 = str(s)
    assert s1 == "aBcD"
    assert type(s1) is str


def test_eq(case_insensitive_str_class: type[str]) -> None:
    s1 = "Abc"
    s2 = case_insensitive_str_class(s1)
    assert s1 == s2


@pytest.fixture
def create_istrs(case_insensitive_str_class: type[str]) -> Callable[[], None]:
    """Make a callable populating memory with a few ``istr`` objects."""

    def _create_strs() -> None:
        case_insensitive_str_class("foobarbaz")
        istr2 = case_insensitive_str_class()
        case_insensitive_str_class(istr2)

    return _create_strs


@pytest.mark.skipif(
    IMPLEMENTATION.name != "cpython",
    reason="PyPy has different GC implementation",
)
def test_leak(
    create_istrs: Callable[[], None], case_insensitive_str_class: type[str]
) -> None:
    gc.collect()
    for _ in range(10000):
        create_istrs()

    gc.collect()
    assert not any(
        isinstance(obj, case_insensitive_str_class) for obj in gc.get_objects()
    )
