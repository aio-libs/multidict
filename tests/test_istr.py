import gc
import sys
from typing import Callable, Type

import pytest

IMPLEMENTATION = getattr(sys, "implementation")  # to suppress mypy error
GIL_ENABLED = getattr(sys, "_is_gil_enabled", lambda: True)()


def test_ctor(ci_str_class: Type[str]) -> None:
    s = ci_str_class()
    assert "" == s


def test_ctor_str(ci_str_class: Type[str]) -> None:
    s = ci_str_class("aBcD")
    assert "aBcD" == s


def test_ctor_istr(ci_str_class: Type[str]) -> None:
    s = ci_str_class("A")
    s2 = ci_str_class(s)
    assert "A" == s
    assert s == s2


def test_ctor_buffer(ci_str_class: Type[str]) -> None:
    s = ci_str_class(b"aBc")
    assert "b'aBc'" == s


def test_ctor_repr(ci_str_class: Type[str]) -> None:
    s = ci_str_class(None)
    assert "None" == s


def test_str(ci_str_class: Type[str]) -> None:
    s = ci_str_class("aBcD")
    s1 = str(s)
    assert s1 == "aBcD"
    assert type(s1) is str


def test_eq(ci_str_class: Type[str]) -> None:
    s1 = "Abc"
    s2 = ci_str_class(s1)
    assert s1 == s2


@pytest.fixture
def create_istrs(ci_str_class: Type[str]) -> Callable[[], None]:
    """Make a callable populating memory with a few ``istr`` objects."""

    def _create_strs() -> None:
        ci_str_class("foobarbaz")
        istr2 = ci_str_class()
        ci_str_class(istr2)

    return _create_strs


@pytest.mark.skipif(
    IMPLEMENTATION.name != "cpython",
    reason="PyPy has different GC implementation",
)
@pytest.mark.skipif(
    not GIL_ENABLED,
    reason="free threading has different GC implementation",
)
def test_leak(create_istrs: Callable[[], None]) -> None:
    gc.collect()
    cnt = len(gc.get_objects())
    for _ in range(10000):
        create_istrs()

    gc.collect()
    cnt2 = len(gc.get_objects())
    assert abs(cnt - cnt2) < 10  # on other GC impls these numbers are not equal
