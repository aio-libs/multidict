import runpy
import sys
from collections.abc import Callable
from pathlib import Path
from typing import cast

import pytest

SCRIPT = Path(__file__).parents[1] / "tools" / "release_notes_md.py"


@pytest.fixture(scope="module")
def convert() -> Callable[[str], str]:
    return cast(Callable[[str], str], runpy.run_path(str(SCRIPT))["convert"])


@pytest.mark.parametrize(
    ("rst", "md"),
    [
        (":issue:`1437`", "#1437"),
        (":pr:`1587`", "#1587"),
        (":commit:`8aee2a5`", "8aee2a5"),
        (":user:`asvetlov`", "[@asvetlov](https://github.com/asvetlov)"),
        (
            ":gh:`GHSA-1 <aio-libs/multidict/security/advisories/GHSA-1>`",
            "[GHSA-1](https://github.com/aio-libs/multidict/security/advisories/GHSA-1)",
        ),
        (":gh:`aio-libs/yarl`", "[aio-libs/yarl](https://github.com/aio-libs/yarl)"),
        (":doc:`capi`", "[capi](https://multidict.aio-libs.org/en/stable/capi.html)"),
        (
            ":doc:`the C API <capi>`",
            "[the C API](https://multidict.aio-libs.org/en/stable/capi.html)",
        ),
        (":ref:`and <python:and>`", "and"),
        (":class:`~multidict.istr`", "`istr`"),
        (":py:class:`multidict.MultiDict`", "`multidict.MultiDict`"),
        (":meth:`~multidict.MultiDict.getall`", "`getall()`"),
        (":py:func:`multidict.getversion`", "`multidict.getversion()`"),
        (":exc:`TypeError`", "`TypeError`"),
        (":c:func:`!PyDict_AddWatcher`", "`PyDict_AddWatcher()`"),
        (":c:macro:`MULTIDICT_MAX_WATCHERS`", "`MULTIDICT_MAX_WATCHERS`"),
        (
            ":doc:`the C\n  API <capi>`",
            "[the C API](https://multidict.aio-libs.org/en/stable/capi.html)",
        ),
        ("``literal`` and **bold**", "``literal`` and **bold**"),
    ],
)
def test_convert(convert: Callable[[str], str], rst: str, md: str) -> None:
    assert convert(rst) == md


def test_main(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    src = tmp_path / "CHANGES.rst"
    dst = tmp_path / "CHANGES.md"
    src.write_text(
        "7.0.0\n=====\n\n*(2026-09-26)*\n\n- Fixed :class:`~multidict.istr`"
        " -- by :user:`asvetlov`.\n",
        encoding="utf-8",
    )
    monkeypatch.setattr(sys, "argv", [str(SCRIPT), str(src), str(dst)])
    runpy.run_path(str(SCRIPT), run_name="__main__")
    assert dst.read_text(encoding="utf-8") == (
        "7.0.0\n=====\n\n*(2026-09-26)*\n\n- Fixed `istr`"
        " -- by [@asvetlov](https://github.com/asvetlov).\n"
    )
