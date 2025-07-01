from __future__ import annotations

import argparse
import importlib.resources
import json
import os
import pickle
from dataclasses import dataclass
from functools import cached_property
from importlib import import_module
from types import ModuleType
from typing import Callable, Type, Union

import pytest

from multidict import (
    CIMultiDict,
    MultiDict,
    MultiDictProxy,
    MultiMapping,
    MutableMultiMapping,
)

C_EXT_MARK = pytest.mark.c_extension


@dataclass(frozen=True)
class MultidictImplementation:
    """A facade for accessing importable multidict module variants.

    An instance essentially represents a c-extension or a pure-python module.
    The actual underlying module is accessed dynamically through a property and
    is cached.

    It also has a text tag depending on what variant it is, and a string
    representation suitable for use in Pytest's test IDs via parametrization.
    """

    is_pure_python: bool
    """A flag showing whether this is a pure-python module or a C-extension."""

    @cached_property
    def tag(self) -> str:
        """Return a text representation of the pure-python attribute."""
        return "py" if self.is_pure_python else "c"

    @cached_property
    def imported_module(self) -> ModuleType:
        """Return a loaded importable containing a multidict variant."""
        importable_module = "_multidict_py" if self.is_pure_python else "_multidict"
        return import_module(f"multidict.{importable_module}")

    def __str__(self) -> str:
        """Render the implementation facade instance as a string."""
        return self.tag


@pytest.fixture(
    scope="session",
    params=(
        pytest.param(
            MultidictImplementation(is_pure_python=False),
            marks=C_EXT_MARK,
        ),
        MultidictImplementation(is_pure_python=True),
    ),
    ids=str,
)
def multidict_implementation(request: pytest.FixtureRequest) -> MultidictImplementation:
    """Return a multidict variant facade."""
    return request.param  # type: ignore[no-any-return]


@pytest.fixture(scope="session")
def _gcov_configuration() -> None:
    """Configure the C-extension gcda write location in debug mode."""
    # NOTE: This is not using `monkeypatch` because it's unavailable with the
    # NOTE: session scope. Additionally, we need these environment variables
    # NOTE: to survive until the module gets to writing data to disk.
    # NOTE: We don't currently run with `pytest-xdist` but might have to
    # NOTE: improve this if we start, to avoid data races. Also, should we
    # NOTE: integrate `tmp_path` instead of writing to `site-packages/`?

    tracing_data_dir_path = (
        # FIXME: read from `pyproject.toml`?
        importlib.resources.files('multidict')
        / '__tracing-data__'
    )
    if not tracing_data_dir_path.is_dir():
        # NOTE: The C-extention was probably compiled without tracing or
        # NOTE: packaging is borked.
        return

    build_meta_json_path = tracing_data_dir_path / 'build-metadata.json'
    gcov_env = json.loads(build_meta_json_path.read_text(encoding='utf-8'))
    os.environ.update(gcov_env)


@pytest.fixture(scope="session")
def multidict_module(
    multidict_implementation: MultidictImplementation,
    request: pytest.FixtureRequest,
) -> ModuleType:
    """Return a pre-imported module containing a multidict variant."""
    if not multidict_implementation.is_pure_python:
        request.getfixturevalue('_gcov_configuration')
    return multidict_implementation.imported_module


@pytest.fixture(
    scope="session",
    params=("MultiDict", "CIMultiDict"),
    ids=("cs", "ci"),
)
def any_multidict_class_name(request: pytest.FixtureRequest) -> str:
    """Return a class name of a mutable multidict implementation."""
    return request.param  # type: ignore[no-any-return]


@pytest.fixture(scope="session")
def any_multidict_class(
    any_multidict_class_name: str,
    multidict_module: ModuleType,
) -> Type[MutableMultiMapping[str]]:
    """Return a class object of a mutable multidict implementation."""
    return getattr(multidict_module, any_multidict_class_name)  # type: ignore[no-any-return]


@pytest.fixture(scope="session")
def case_sensitive_multidict_class(
    multidict_module: ModuleType,
) -> Type[MultiDict[str]]:
    """Return a case-sensitive mutable multidict class."""
    return multidict_module.MultiDict  # type: ignore[no-any-return]


@pytest.fixture(scope="session")
def case_insensitive_multidict_class(
    multidict_module: ModuleType,
) -> Type[CIMultiDict[str]]:
    """Return a case-insensitive mutable multidict class."""
    return multidict_module.CIMultiDict  # type: ignore[no-any-return]


@pytest.fixture(scope="session")
def case_insensitive_str_class(multidict_module: ModuleType) -> Type[str]:
    """Return a case-insensitive string class."""
    return multidict_module.istr  # type: ignore[no-any-return]


@pytest.fixture(scope="session")
def any_multidict_proxy_class_name(any_multidict_class_name: str) -> str:
    """Return a class name of an immutable multidict implementation."""
    return f"{any_multidict_class_name}Proxy"


@pytest.fixture(scope="session")
def any_multidict_proxy_class(
    any_multidict_proxy_class_name: str,
    multidict_module: ModuleType,
) -> Type[MultiMapping[str]]:
    """Return an immutable multidict implementation class object."""
    return getattr(multidict_module, any_multidict_proxy_class_name)  # type: ignore[no-any-return]


@pytest.fixture(scope="session")
def case_sensitive_multidict_proxy_class(
    multidict_module: ModuleType,
) -> Type[MutableMultiMapping[str]]:
    """Return a case-sensitive immutable multidict class."""
    return multidict_module.MultiDictProxy  # type: ignore[no-any-return]


@pytest.fixture(scope="session")
def case_insensitive_multidict_proxy_class(
    multidict_module: ModuleType,
) -> Type[MutableMultiMapping[str]]:
    """Return a case-insensitive immutable multidict class."""
    return multidict_module.CIMultiDictProxy  # type: ignore[no-any-return]


@pytest.fixture(scope="session")
def multidict_getversion_callable(
    multidict_module: ModuleType,
) -> Callable[[Union[MultiDict[object], MultiDictProxy[object]]], int]:
    """Return a ``getversion()`` function for current implementation."""
    return multidict_module.getversion  # type: ignore[no-any-return]


def pytest_addoption(
    parser: pytest.Parser,
    pluginmanager: pytest.PytestPluginManager,
) -> None:
    """Define a new ``--c-extensions`` flag.

    This lets the callers deselect tests executed against the C-extension
    version of the ``multidict`` implementation.
    """
    del pluginmanager

    parser.addoption(
        "--c-extensions",  # disabled with `--no-c-extensions`
        action=argparse.BooleanOptionalAction,
        default=True,
        dest="c_extensions",
        help="Test C-extensions (on by default)",
    )


def pytest_collection_modifyitems(
    session: pytest.Session,
    config: pytest.Config,
    items: list[pytest.Item],
) -> None:
    """Deselect tests against C-extensions when requested via CLI."""
    test_c_extensions = config.getoption("--c-extensions") is True

    if test_c_extensions:
        return

    selected_tests: list[pytest.Item] = []
    deselected_tests: list[pytest.Item] = []

    for item in items:
        c_ext = item.get_closest_marker(C_EXT_MARK.name) is not None

        target_items_list = deselected_tests if c_ext else selected_tests
        target_items_list.append(item)

    config.hook.pytest_deselected(items=deselected_tests)
    items[:] = selected_tests


def pytest_configure(config: pytest.Config) -> None:
    """Declare the C-extension marker in config."""
    config.addinivalue_line(
        "markers",
        f"{C_EXT_MARK.name}: tests running against the C-extension implementation.",
    )


def pytest_generate_tests(metafunc: pytest.Metafunc) -> None:
    if "pickle_protocol" in metafunc.fixturenames:
        metafunc.parametrize(
            "pickle_protocol", list(range(pickle.HIGHEST_PROTOCOL + 1)), scope="session"
        )
