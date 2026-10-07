import gc
import sys
from collections.abc import Callable

import pytest

import multidict
from multidict import CIMultiDict

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


# One of each str storage: empty, ASCII, 1-byte beyond ASCII, 2-byte and
# 4-byte characters. Built at run time, so none is a shared constant.
ALL_KINDS = ["", "Content-Type", "X-\xc4rger", "X-\u0416\u0443\u043a", "X-\U0001f600"]


@pytest.mark.parametrize(
    "value", ALL_KINDS, ids=["empty", "ascii", "latin1", "ucs2", "ucs4"]
)
@pytest.mark.parametrize("via", ["istr", "key"])
def test_copy_of_every_str_kind(
    case_insensitive_multidict_class: type[CIMultiDict[int]],
    case_insensitive_str_class: type[str],
    value: str,
    via: str,
) -> None:
    source = "".join(list(value))
    if via == "istr":
        made = case_insensitive_str_class(source)
    else:
        made = next(iter(case_insensitive_multidict_class([(source, 1)])))
    assert type(made) is case_insensitive_str_class
    assert made == value
    assert hash(made) == hash(value)
    assert len(made) == len(value)
    assert str(made) == value
    assert made.encode() == value.encode()
    assert made.lower() == value.lower()
    assert made[1:] == value[1:]
    assert repr(made) == repr(value)
    # parsing an encoding name caches its UTF-8 form, a buffer of its own
    # unless ASCII, which the istr frees with itself
    with pytest.raises(LookupError):
        "".encode(made)


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


def test_upstr_deprecated() -> None:
    with pytest.deprecated_call(match="upstr is deprecated, use istr instead"):
        assert multidict.upstr is multidict.istr


def test_upstr_in_dir() -> None:
    assert "upstr" in dir(multidict)
    assert "istr" in dir(multidict)


def test_upstr_not_in_all() -> None:
    assert "upstr" not in multidict.__all__


def test_unknown_module_attribute() -> None:
    with pytest.raises(AttributeError, match="has no attribute 'nonexistent'"):
        getattr(multidict, "nonexistent")
