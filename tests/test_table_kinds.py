"""A MultiDict whose keys are all exact str, or a CIMultiDict whose keys are
all exact istr, stores them in a compact table.

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


def test_lookups_race_deletes_that_free_keys() -> None:
    # Each key's only reference is the table's, so a delete frees it while
    # readers may be probing its slot. The probes are equal to the keys but
    # not the same objects, so every lookup reads through a stored key.
    stop = threading.Event()
    d = c.MultiDict((k, i) for i, k in enumerate(KEYS))
    probes = ["".join(k) for k in KEYS]

    def read() -> None:
        while not stop.is_set():
            for i, k in enumerate(probes):
                assert d.get(k) == i
            assert d.get("gone") is None

    with ThreadPoolExecutor(4) as pool:
        readers = [pool.submit(read) for _ in range(4)]
        try:
            for n in range(2000):
                d[f"fresh-{n}"] = n
                del d[f"fresh-{n}"]
        finally:
            stop.set()
        for f in readers:
            f.result(timeout=60)
    assert _items(d) == [(k, i) for i, k in enumerate(KEYS)]


ISTR_KEYS = [c.istr(k) for k in KEYS]


def _ci_items(md: Any) -> list[tuple[str, object]]:
    return [(str(k), v) for k, v in md.items()]


def test_istr_keys_use_the_compact_table() -> None:
    ci = c.CIMultiDict((k, 1) for k in ISTR_KEYS)
    assert ci.__sizeof__() < c.CIMultiDict((k, 1) for k in KEYS).__sizeof__()
    assert ci.__sizeof__() == c.MultiDict((k, 1) for k in KEYS).__sizeof__()


@pytest.mark.parametrize("op", OPS)
@pytest.mark.parametrize("wrap", ["str", "subclass"])
def test_a_non_istr_key_moves_the_ci_table(op: str, wrap: str) -> None:
    def make(mod: Any) -> Any:
        d = mod.CIMultiDict((mod.istr(k), i) for i, k in enumerate(KEYS))
        OPS[op](d, str if wrap == "str" else StrKey)
        return d

    compact = c.CIMultiDict((k, i) for i, k in enumerate(ISTR_KEYS))
    d = make(c)
    expected = make(py)
    assert _ci_items(d) == _ci_items(expected)
    assert d.__sizeof__() > compact.__sizeof__()
    assert d["K3"] == 3


def test_ci_clear_starts_compact_again() -> None:
    d = c.CIMultiDict((k, 1) for k in ISTR_KEYS)
    d.add("X", 2)
    d.clear()
    d.extend((k, 1) for k in ISTR_KEYS)
    assert d.__sizeof__() == c.CIMultiDict((k, 1) for k in ISTR_KEYS).__sizeof__()


@pytest.mark.parametrize("convert", [False, True])
def test_ci_copy_keeps_the_layout(convert: bool) -> None:
    d = c.CIMultiDict((k, 1) for k in ISTR_KEYS)
    if convert:
        d.add("s", 2)
    copy = d.copy()
    assert copy.__sizeof__() == d.__sizeof__()
    assert _ci_items(copy) == _ci_items(d)


# The constructor picks the layout from the first key it will get; a later
# key of the other sort still lands in the right one.
CTOR_ARGS: dict[str, Callable[[Any], tuple[tuple[object, ...], dict[str, object]]]] = {
    "list_istr_first": lambda m: (([(m.istr("A"), 1), ("b", 2)],), {}),
    "list_str_first": lambda m: (([("a", 1), (m.istr("B"), 2)],), {}),
    "tuple_istr": lambda m: ((((m.istr("A"), 1), (m.istr("B"), 2)),), {}),
    "list_of_lists": lambda m: (([[m.istr("A"), 1], [m.istr("B"), 2]],), {}),
    "dict_istr": lambda m: (({m.istr("A"): 1, m.istr("B"): 2},), {}),
    "dict_str": lambda m: (({"a": 1, m.istr("B"): 2},), {}),
    "kwargs": lambda m: ((), {"a": 1, "B": 2}),
    "list_and_kwargs": lambda m: (([(m.istr("A"), 1)],), {"b": 2}),
    "empty_list": lambda m: (([],), {}),
    "bad_pair": lambda m: (([(m.istr("A"), 1, 0)],), {}),
}


@pytest.mark.parametrize("arg", CTOR_ARGS)
def test_ci_constructor_arguments(arg: str) -> None:
    def make(mod: Any) -> object:
        args, kwargs = CTOR_ARGS[arg](mod)
        try:
            return _ci_items(mod.CIMultiDict(*args, **kwargs))
        except ValueError:
            return ValueError

    assert make(c) == make(py)


def test_ci_constructor_from_a_ci_multidict() -> None:
    for keys in (ISTR_KEYS, KEYS):
        src = c.CIMultiDict((k, 1) for k in keys)
        d = c.CIMultiDict(src)
        assert d.__sizeof__() == src.__sizeof__()
        assert _ci_items(d) == _ci_items(src)


def test_ci_move_inside_update_keeps_its_marks() -> None:
    def run(mod: Any) -> list[tuple[str, object]]:
        a, b = mod.istr("a"), mod.istr("b")
        d = mod.CIMultiDict([(a, 0), (a, 0), (b, 0), (a, 0)])

        def items() -> Iterator[tuple[str, int]]:
            yield (a, 1)
            yield ("B", 2)
            yield (a, 3)

        d.update(items())
        return _ci_items(d)

    assert run(c) == run(py)


@pytest.mark.parametrize("compact", [True, False])
def test_lookups_with_every_key_sort(compact: bool) -> None:
    # A lookup borrows the identity of an exact str or istr and computes
    # it for anything else.
    keys = ISTR_KEYS if compact else KEYS
    ci = c.CIMultiDict((k, i) for i, k in enumerate(keys))
    for probe in ("k3", "K3", c.istr("K3"), StrKey("K3")):
        assert ci[probe] == 3
        assert probe in ci
        assert ci.get(probe) == 3
        assert ci.getone(probe) == 3
        assert c.CIMultiDictProxy(ci)[probe] == 3
    assert ci.get("missing") is None
    assert "Missing" not in ci
    md = c.MultiDict((k, i) for i, k in enumerate(KEYS))
    for probe in ("k3", c.istr("k3"), StrKey("k3")):
        assert md[probe] == 3
        assert probe in md
        assert md.get(probe) == 3
    assert md.get("K3") is None
    with pytest.raises(TypeError):
        ci.get(1)


def test_ci_lookups_race_a_move() -> None:
    stop = threading.Event()
    d = c.CIMultiDict((k, i) for i, k in enumerate(ISTR_KEYS))

    def read() -> None:
        while not stop.is_set():
            for i, k in enumerate(ISTR_KEYS):
                assert d.get(k) in (i, None)

    with ThreadPoolExecutor(4) as pool:
        readers = [pool.submit(read) for _ in range(4)]
        try:
            for n in range(200):
                d.add(f"S{n}", n)
                d.clear()
                d.extend((k, i) for i, k in enumerate(ISTR_KEYS))
        finally:
            stop.set()
        for f in readers:
            f.result(timeout=60)


@pytest.mark.parametrize("method", ["extend", "update", "merge"])
@pytest.mark.parametrize("shape", ["full", "holes", "empty"])
def test_ci_kwargs_move_the_table_while_it_grows(method: str, shape: str) -> None:
    # Keyword names are str, so the batch moves a compact table to the full
    # layout up front, growing it in the same rebuild.
    def make(mod: Any) -> list[tuple[str, object]]:
        d = mod.CIMultiDict((mod.istr(k), i) for i, k in enumerate(KEYS))
        if shape == "holes":
            for k in KEYS[::3]:
                del d[k]
        elif shape == "empty":
            d.clear()
        getattr(d, method)([(mod.istr("K1"), "x")], **{f"w{i}": i for i in range(30)})
        assert d["W5"] == 5
        return _ci_items(d)

    assert make(c) == make(py)


def test_ci_lookups_race_deletes_that_free_keys() -> None:
    # Each key's only reference is the table's, so a delete frees it while
    # readers may be probing its slot; a lookup must read nothing through
    # the freed key.
    stop = threading.Event()
    d = c.CIMultiDict((k, i) for i, k in enumerate(ISTR_KEYS))

    def read() -> None:
        while not stop.is_set():
            for i, k in enumerate(ISTR_KEYS):
                assert d.get(k) == i
            assert d.get("gone") is None

    with ThreadPoolExecutor(4) as pool:
        readers = [pool.submit(read) for _ in range(4)]
        try:
            for n in range(2000):
                d[c.istr(f"Fresh-{n}")] = n
                del d[f"fresh-{n}"]
        finally:
            stop.set()
        for f in readers:
            f.result(timeout=60)
    assert _ci_items(d) == [(str(k), i) for i, k in enumerate(ISTR_KEYS)]
