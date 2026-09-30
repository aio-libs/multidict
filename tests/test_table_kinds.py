"""A MultiDict whose keys are all exact str stores them in a compact table.

Any other key moves the table to the full layout, keeping every entry and
its position; clear() lets the table start compact again. Only the size
shows the layout, so these compare sizes and check the contents against
the pure-Python implementation.
"""

import gc
import threading
import weakref
from collections.abc import Callable, Iterator
from concurrent.futures import ThreadPoolExecutor
from typing import Any

import pytest

import multidict._multidict_py as py

pytestmark = pytest.mark.c_extension
c = pytest.importorskip("multidict._multidict")


class StrKey(str):
    owner: object


KEYS = [f"k{i}" for i in range(20)]


def _items(md: Any) -> list[tuple[str, object]]:
    return [(str(k), v) for k, v in md.items()]


def test_str_keys_use_the_compact_table() -> None:
    md = c.MultiDict((k, 1) for k in KEYS)
    ci = c.CIMultiDict((k, 1) for k in KEYS)
    assert md.__sizeof__() < ci.__sizeof__()


# Each takes the multidict and a key factory; "k7" exists, "new" does not.
OPS: dict[str, Callable[[Any, Callable[[str], str]], object]] = {
    "add": lambda d, k: d.add(k("new"), "v"),
    "setitem_new": lambda d, k: d.__setitem__(k("new"), "v"),
    "setitem_existing": lambda d, k: d.__setitem__(k("k7"), "v"),
    "setdefault": lambda d, k: d.setdefault(k("new"), "v"),
    "extend": lambda d, k: d.extend([(k("k7"), "v")]),
    "update_existing": lambda d, k: d.update([(k("k7"), "v")]),
    "merge": lambda d, k: d.merge([(k("new"), "v")]),
}


@pytest.mark.parametrize("op", OPS)
@pytest.mark.parametrize("wrap", ["istr", "subclass"])
def test_a_non_str_key_moves_to_the_full_table(op: str, wrap: str) -> None:
    def make(mod: Any) -> Any:
        d = mod.MultiDict((k, i) for i, k in enumerate(KEYS))
        key = mod.istr if wrap == "istr" else StrKey
        OPS[op](d, key)
        return d

    compact = c.MultiDict((k, i) for i, k in enumerate(KEYS))
    d = make(c)
    expected = make(py)
    assert _items(d) == _items(expected)
    assert len(d) == len(expected)
    assert d.__sizeof__() > compact.__sizeof__()
    assert d["k3"] == 3


def test_clear_starts_compact_again() -> None:
    d = c.MultiDict((k, 1) for k in KEYS)
    d.add(c.istr("X"), 2)
    d.clear()
    d.extend((k, 1) for k in KEYS)
    assert d.__sizeof__() == c.MultiDict((k, 1) for k in KEYS).__sizeof__()


@pytest.mark.parametrize("convert", [False, True])
def test_copy_keeps_the_layout(convert: bool) -> None:
    d = c.MultiDict((k, 1) for k in KEYS)
    if convert:
        d.add(StrKey("s"), 2)
    copy = d.copy()
    assert copy.__sizeof__() == d.__sizeof__()
    assert list(copy.items()) == list(d.items())


def test_move_inside_update_keeps_its_marks() -> None:
    # The move happens between update()'s items, on a table whose earlier
    # items are marked by index; the result must match the pure backend.
    def run(mod: Any) -> list[tuple[str, object]]:
        d = mod.MultiDict([("a", 0), ("a", 0), ("b", 0), ("a", 0)])

        def items() -> Iterator[tuple[str, int]]:
            yield ("a", 1)
            yield (StrKey("b"), 2)
            yield ("a", 3)

        d.update(items())
        return _items(d)

    assert run(c) == run(py)


def test_move_during_iteration_is_refused() -> None:
    d = c.MultiDict((k, 1) for k in KEYS)
    it = iter(d)
    next(it)
    d.add(StrKey("s"), 2)
    with pytest.raises(RuntimeError, match="changed during iteration"):
        next(it)


def test_cycle_through_a_moved_key_is_collected() -> None:
    d = c.MultiDict((k, 1) for k in KEYS)
    key = StrKey("cycle")
    key.owner = d
    d[key] = 1
    ref = weakref.ref(d)
    del d, key
    gc.collect()
    assert ref() is None


def test_lookups_race_a_move() -> None:
    # Readers keep looking up while another thread moves the table; each
    # lookup sees the old table or the new one, never a torn entry. A key
    # can be missing only in between the clear() and the extend().
    stop = threading.Event()
    d = c.MultiDict((k, i) for i, k in enumerate(KEYS))

    def read() -> None:
        while not stop.is_set():
            for i, k in enumerate(KEYS):
                assert d.get(k) in (i, None)

    with ThreadPoolExecutor(4) as pool:
        readers = [pool.submit(read) for _ in range(4)]
        try:
            for n in range(200):
                d.add(StrKey(f"s{n}"), n)
                d.clear()
                d.extend((k, i) for i, k in enumerate(KEYS))
        finally:
            stop.set()
        for f in readers:
            f.result(timeout=60)
