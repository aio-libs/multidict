from dataclasses import dataclass
from functools import cache, cached_property
from importlib import import_module
from types import ModuleType
from typing import Callable, Type

import pytest
import os
from multidict import (
    MultiMapping,
    MutableMultiMapping,
)


skip_if_no_extensions = pytest.mark.skipif(os.environ.get("MULTIDICT_NO_EXTENSIONS"), "cython tests disabled")

@cache
def try_importing_c():
    return import_module("multidict._multidict")


@cache
def try_impotring_cython():
    return import_module("_multidict_cython")


@pytest.fixture(scope="module")
def c_module():
    return try_importing_c()


@pytest.fixture(scope="module")
def cython_module():
    return try_impotring_cython()


@dataclass(frozen=True)
class MultidictCythonImplementation:
    """A facade for accessing importable multidict module variants.

    An instance essentially represents a c-extension or a pure-python module.
    The actual underlying module is accessed dynamically through a property and
    is cached.

    It also has a text tag depending on what variant it is, and a string
    representation suitable for use in Pytest's test IDs via parametrization.
    """

    use_cython: bool
    """A flag showing whether this is a pure-python module or a C-extension."""

    @cached_property
    def tag(self) -> str:
        """Return a text representation of the pure-python attribute."""
        return "cython-extension" if self.use_cython else "c-extension"

    @cached_property
    def imported_module(self) -> ModuleType:
        """Return a loaded importable containing a multidict variant."""
        importable_module = (
            "_multidict_cython" if self.use_cython else "multidict._multidict"
        )
        return import_module(f"{importable_module}")

    def __str__(self) -> str:
        """Render the implementation facade instance as a string."""
        return f"{self.tag}-module"

    def get_class(self, attr: str) -> MultiMapping[str]:
        name = ("Cython_" + attr) if self.use_cython else attr
        return self.imported_module.__dict__[name]


@pytest.fixture(
    scope="session",
    params=(
        pytest.param(
            MultidictCythonImplementation(use_cython=False),
            marks=pytest.mark.c_extension,
        ),
        pytest.param(MultidictCythonImplementation(use_cython=True)),
    ),
    ids=str,
)
def c_or_cython_multidict_implementation(
    request: pytest.FixtureRequest,
) -> MultidictCythonImplementation:
    return request.param  # type: ignore[no-any-return]


@pytest.fixture(scope="session")
def any_multidict_c_or_cython_class(
    c_or_cython_multidict_implementation: MultidictCythonImplementation,
    any_multidict_class_name: str,
) -> Type[MutableMultiMapping[str]]:
    return c_or_cython_multidict_implementation.get_class(any_multidict_class_name)  # type: ignore[no-any-return]


@pytest.fixture(scope="session")
def any_cython_md_creation_func(
    any_multidict_class_name: str,
) -> Callable[[], MutableMultiMapping[str]]:
    return getattr(try_impotring_cython(), any_multidict_class_name.lower() + "_create")  # type: ignore


@skip_if_no_extensions
def test_cython_types_are_equivilent_to_c(
    cython_module: ModuleType, c_module: ModuleType
):
    assert cython_module.Cython_MultiDict == c_module.MultiDict  # type: ignore
    assert cython_module.Cython_MultiDictProxy == c_module.MultiDictProxy  # type: ignore
    assert cython_module.Cython_CIMultiDict == c_module.CIMultiDict  # type: ignore
    assert cython_module.Cython_CIMultiDictProxy == c_module.CIMultiDictProxy  # type: ignore

@skip_if_no_extensions
def test_cython_creation_of_multidict(
    cython_module: ModuleType,
    any_multidict_c_or_cython_class: Type[MutableMultiMapping[str]],
):
    md = cython_module.multidict_create()
    md.add("a", 1)
    b = any_multidict_c_or_cython_class()
    b.add("a", 1)
    assert md == b

@skip_if_no_extensions
def test_cython_addition(
    any_cython_md_creation_func: Callable[[], MutableMultiMapping[str]],
    cython_module: ModuleType,
    any_multidict_c_or_cython_class: Type[MutableMultiMapping[str]],
):
    md = any_cython_md_creation_func()
    cython_module.multidict_add(md)
    assert md == any_multidict_c_or_cython_class([("a", 1), ("b", 2)])

@skip_if_no_extensions
def test_cython_update(
    any_cython_md_creation_func: Callable[[], MutableMultiMapping[str]],
    cython_module: ModuleType,
    any_multidict_c_or_cython_class: Type[MutableMultiMapping[str]],
):
    md = any_cython_md_creation_func()
    cython_module.multidict_update(md, a=2, b=1)
    assert md == any_multidict_c_or_cython_class([("a", 2), ("b", 1)])

@skip_if_no_extensions
def test_cython_copy(
    any_cython_md_creation_func: Callable[[], MutableMultiMapping[str]],
    cython_module: ModuleType,
    any_multidict_c_or_cython_class: Type[MutableMultiMapping[str]],
):
    md = any_cython_md_creation_func()
    cython_module.multidict_update(md, a=2, b=1)
    new_md = cython_module.multidict_copy(md)
    assert new_md == any_multidict_c_or_cython_class([("a", 2), ("b", 1)])

@skip_if_no_extensions
def test_cython_get(
    any_cython_md_creation_func: Callable[[], MutableMultiMapping[str]],
    cython_module: ModuleType,
):
    md = any_cython_md_creation_func()
    cython_module.multidict_update(md, a=2, b=1)
    assert md.get("b") == 1
    assert cython_module.multidict_get(md, "a") == 2

    # XXX: Broken, this sends a number when it should've been None, no clue why this happens
    # assert cython_module.multidict_get(md, "I DONT EXIST!")
