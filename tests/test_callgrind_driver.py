import importlib
import pathlib
import sys
from types import ModuleType

import pytest

BENCHMARKS = pathlib.Path(__file__).parent.parent / "benchmarks"


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
    driver: ModuleType,
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: pathlib.Path,
    multidict_only: bool,
) -> None:
    checked: list[object] = []
    collected: list[object] = []

    def collect(args: object, python: str, stage_dir: str, cells: object) -> int:
        collected.append(cells)
        return 0

    stage_dir = tmp_path / "stage"
    stage_dir.mkdir()
    monkeypatch.setattr(driver, "self_check", checked.append)
    monkeypatch.setattr(driver, "make_stage_dir", lambda: str(stage_dir))
    monkeypatch.setattr(driver, "collect", collect)
    argv = ["callgrind_driver.py", "--impl", "multidict_py"]
    argv += ["--valgrind", sys.executable]
    if multidict_only:
        argv.append("--include-multidict-only")
    monkeypatch.setattr(sys, "argv", argv)
    assert driver.main() == 0
    expected = driver.operations.selected(
        impl_id="multidict_py", shared_only=not multidict_only
    )
    assert checked == collected == [expected]
    assert {impl.id for _, impl in expected} == {"multidict_py"}
    assert not stage_dir.exists()


@pytest.mark.parametrize("impl_id", [None, "multidict_py"])
def test_main_self_check_checks_every_cell(
    driver: ModuleType, monkeypatch: pytest.MonkeyPatch, impl_id: str | None
) -> None:
    # Every operation, of the one implementation --impl names if given: the
    # hint self_check() gives for an unimportable one has to work here too.
    checked: list[object] = []
    monkeypatch.setattr(driver, "self_check", checked.append)
    argv = ["callgrind_driver.py", "--self-check"]
    argv += [] if impl_id is None else ["--impl", impl_id]
    monkeypatch.setattr(sys, "argv", argv)
    assert driver.main() == 0
    assert checked == [driver.operations.selected(impl_id=impl_id)]
