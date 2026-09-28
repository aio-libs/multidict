import importlib
import pathlib
import sys
from types import ModuleType

import pytest

BENCHMARKS = pathlib.Path(__file__).parent.parent / "benchmarks"


class Stop(Exception):
    pass


@pytest.fixture
def driver(monkeypatch: pytest.MonkeyPatch) -> ModuleType:
    monkeypatch.syspath_prepend(str(BENCHMARKS))
    return importlib.import_module("callgrind_driver")


def test_self_check_selected_cells(
    driver: ModuleType, capsys: pytest.CaptureFixture[str]
) -> None:
    cells = driver.operations.selected(impl_id="multidict_py")
    driver.self_check(cells)
    assert capsys.readouterr().out == f"self-check passed: {len(cells)} cells\n"


def test_self_check_unimportable_impl(driver: ModuleType) -> None:
    operations = driver.operations
    broken = operations.Impl(
        "broken",
        "broken",
        operations.Kind.MULTIDICT,
        operations._attr("multidict._no_such_module", "MultiDict"),
    )
    cells = [(operations.OPERATIONS_BY_ID["copy"], broken)]
    with pytest.raises(driver.DriverError, match="^broken cannot be imported"):
        driver.self_check(cells)


@pytest.mark.parametrize("multidict_only", [False, True])
def test_main_checks_only_selected_cells(
    driver: ModuleType, monkeypatch: pytest.MonkeyPatch, multidict_only: bool
) -> None:
    checked: list[object] = []

    def bracketed(python: str) -> bool:
        raise Stop

    monkeypatch.setattr(driver, "self_check", checked.append)
    monkeypatch.setattr(driver, "bracketed", bracketed)
    argv = ["callgrind_driver.py", "--impl", "multidict_py"]
    argv += ["--valgrind", sys.executable]
    if multidict_only:
        argv.append("--include-multidict-only")
    monkeypatch.setattr(sys, "argv", argv)
    with pytest.raises(Stop):
        driver.main()
    expected = driver.operations.selected(
        impl_id="multidict_py", shared_only=not multidict_only
    )
    assert checked == [expected]
    assert {impl.id for _, impl in expected} == {"multidict_py"}
