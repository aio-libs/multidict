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
    driver.self_check(cells, "unused")
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
    with pytest.raises(
        driver.DriverError, match=r"^broken cannot be imported \(.*\); remedy$"
    ):
        driver.self_check(cells, "remedy")


@pytest.mark.parametrize("multidict_only", [False, True])
def test_main_checks_only_selected_cells(
    driver: ModuleType,
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: pathlib.Path,
    multidict_only: bool,
) -> None:
    checked: list[tuple[object, str]] = []
    collected: list[object] = []

    def collect(args: object, python: str, stage_dir: str, cells: object) -> int:
        collected.append(cells)
        return 0

    stage_dir = tmp_path / "stage"
    stage_dir.mkdir()
    monkeypatch.setattr(driver, "self_check", lambda *args: checked.append(args))
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
    [(cells, remedy)] = checked
    assert [cells] == collected == [expected]
    assert "--impl" in remedy
    assert {impl.id for _, impl in expected} == {"multidict_py"}
    assert not stage_dir.exists()


def test_main_self_check_checks_every_cell(
    driver: ModuleType, monkeypatch: pytest.MonkeyPatch
) -> None:
    checked: list[tuple[object, str]] = []
    monkeypatch.setattr(driver, "self_check", lambda *args: checked.append(args))
    argv = ["callgrind_driver.py", "--self-check", "--impl", "multidict_py"]
    monkeypatch.setattr(sys, "argv", argv)
    assert driver.main() == 0
    [(cells, remedy)] = checked
    assert cells == driver.operations.selected()
    assert "--impl" not in remedy
