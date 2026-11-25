"""Keys of any hashable type.

A key that is no str is its own identity in either class: it matches the
keys equal to it by __hash__ and __eq__, as a dict key does, while str
keys keep their own rules, case folded in a CIMultiDict.
"""

import gc
import pickle
import sys
import threading
from collections.abc import Callable
from concurrent.futures import ThreadPoolExecutor
from typing import Any

import pytest

import multidict._multidict_py as _pure

# keys of any type, which the annotations do not describe yet
_Class = Any


class Key:
    """Equal to a str of the same text, hashing like one."""

    def __init__(self, text: str) -> None:
        self.text = text

    def __hash__(self) -> int:
        return hash(self.text)

    def __eq__(self, other: object) -> bool:
        if isinstance(other, Key):
            return self.text == other.text
        if isinstance(other, str):
            return self.text == other
        return NotImplemented

    def __repr__(self) -> str:
        return f"Key({self.text!r})"


class Collide:
    """Every instance hashes alike; equal only to itself."""

    def __hash__(self) -> int:
        return 7

    def __eq__(self, other: object) -> bool:
        return self is other


def test_int_keys(any_multidict_class: _Class) -> None:
    d = any_multidict_class([(1, "a"), (2, "b")])
    d.add(1, "c")
    assert d[1] == "a"
    assert d.getall(1) == ["a", "c"]
    assert 2 in d
    assert 3 not in d
    assert d.get(3) is None
    assert list(d.keys()) == [1, 2]
    assert len(d.keys()) == 2
    assert list(d.items()) == [(1, "a"), (2, "b"), (1, "c")]


def test_equal_keys_of_other_types(
    any_multidict_class: _Class,
) -> None:
    d = any_multidict_class()
    d.add(1, "int")
    d.add(1.0, "float")
    d.add(True, "bool")
    assert d.getall(1.0) == ["int", "float", "bool"]
    assert list(d) == [1]
    assert len(d.keys()) == 1
    assert d.to_dict() == {1: ["int", "float", "bool"]}
    d[True] = "one"
    assert list(d.items()) == [(True, "one")]


def test_str_and_other_keys(any_multidict_class: _Class) -> None:
    d = any_multidict_class(key="str")
    d[None] = "none"
    d[("a", 1)] = "tuple"
    d[frozenset({1})] = "frozenset"
    d[b"key"] = "bytes"
    assert d["key"] == "str"
    assert d[None] == "none"
    assert d[("a", 1)] == "tuple"
    assert d[frozenset({1})] == "frozenset"
    assert d[b"key"] == "bytes"
    assert len(d) == 5


def test_case_insensitive_str_keys_still_fold(
    case_insensitive_multidict_class: _Class,
) -> None:
    d = case_insensitive_multidict_class()
    d.add("Key", "a")
    d.add(1, "b")
    d.add("KEY", "c")
    d.add(b"KEY", "d")
    assert d.getall("key") == ["a", "c"]
    assert d.getall(b"KEY") == ["d"]
    assert b"key" not in d
    assert list(d.keys()) == ["Key", 1, b"KEY"]


def test_nan_key(any_multidict_class: _Class) -> None:
    nan = float("nan")
    d = any_multidict_class()
    d[nan] = "a"
    assert d[nan] == "a"
    assert float("nan") not in d


def test_unhashable_key(any_multidict_class: _Class) -> None:
    d = any_multidict_class(a=1)
    with pytest.raises(TypeError, match="unhashable"):
        d[[1]] = 1
    with pytest.raises(TypeError, match="unhashable"):
        [1] in d
    with pytest.raises(TypeError, match="unhashable"):
        d.add([1], 1)
    assert ([1], 1) not in d.items()
    assert list(d.items()) == [("a", 1)]


def test_key_equal_to_a_str(any_multidict_class: _Class) -> None:
    d = any_multidict_class(k="str")
    assert d[Key("k")] == "str"
    d.add(Key("k"), "key")
    assert d.getall("k") == ["str", "key"]
    assert d.getall(Key("k")) == ["str", "key"]
    del d[Key("k")]
    assert not d


def test_colliding_keys(any_multidict_class: _Class) -> None:
    keys = [Collide() for _ in range(10)]
    d = any_multidict_class((k, i) for i, k in enumerate(keys))
    for i, k in enumerate(keys):
        assert d[k] == i
    assert Collide() not in d
    del d[keys[3]]
    assert keys[3] not in d
    assert d.popall(keys[4]) == [4]
    assert len(d) == 8


JOIN_OPS: dict[str, Callable[[Any], object]] = {
    "add": lambda d: d.add(1, "new"),
    "setitem": lambda d: d.__setitem__(1, "new"),
    "setdefault": lambda d: d.setdefault(1, "new"),
    "extend": lambda d: d.extend([(1, "new")]),
    "update": lambda d: d.update([(1, "new")]),
    "merge": lambda d: d.merge([(1, "new")]),
    "update_dict": lambda d: d.update({1: "new"}),
}


@pytest.mark.parametrize("op", JOIN_OPS)
@pytest.mark.parametrize("first", ["str", "subclass", "mixed"])
def test_any_key_joins_str_keys(
    any_multidict_class: _Class, op: str, first: str
) -> None:
    class Sub(str):
        pass

    keys: list[str] = [f"k{i}" for i in range(20)]
    if first == "subclass":
        keys = [Sub(k) for k in keys]
    elif first == "mixed":
        keys = keys + [Sub("k3")]
    d = any_multidict_class((k, i) for i, k in enumerate(keys))
    JOIN_OPS[op](d)
    assert d[1] == "new"
    assert d["k3"] == 3
    assert d.getall("k3") == ([3, 20] if first == "mixed" else [3])
    assert len(d) == len(keys) + 1


def test_mutations_keep_order(any_multidict_class: _Class) -> None:
    d = any_multidict_class([(1, "a"), ("x", "b"), (1, "c"), (2, "d"), (1, "e")])
    d[1] = "z"
    assert list(d.items()) == [(1, "z"), ("x", "b"), (2, "d")]
    assert d.popone(2) == "d"
    d.update([(1, "y"), (3, "w")])
    assert list(d.items()) == [(1, "y"), ("x", "b"), (3, "w")]
    d.merge([(1, "skip"), (4, "v")])
    assert list(d.items()) == [(1, "y"), ("x", "b"), (3, "w"), (4, "v")]
    assert d.popitem() == (4, "v")
    d.extend(d)
    assert d.getall(1) == ["y", "y"]
    d.clear()
    d["s"] = "str only"
    assert list(d.items()) == [("s", "str only")]


def test_copy_eq_pickle(
    any_multidict_class: _Class,
    any_multidict_proxy_class: _Class,
    pickle_protocol: int,
) -> None:
    d = any_multidict_class([(1, "a"), ("x", "b"), ((2, 3), "c"), (1, "d")])
    assert d.copy() == d
    assert any_multidict_class(d) == d
    assert d == any_multidict_class([(1.0, "a"), ("x", "b"), ((2, 3), "c"), (1, "d")])
    assert d != any_multidict_class([(2, "a"), ("x", "b"), ((2, 3), "c"), (1, "d")])
    loaded = pickle.loads(pickle.dumps(d, pickle_protocol))
    assert list(loaded.items()) == list(d.items())
    proxy = any_multidict_proxy_class(d)
    assert proxy.getall(1) == ["a", "d"]
    assert proxy.copy() == d


def test_repr(any_multidict_class: _Class) -> None:
    d = any_multidict_class([(1, "a"), ("x", "b"), (None, 2)])
    name = any_multidict_class.__name__
    assert repr(d) == f"<{name}(1: 'a', 'x': 'b', None: 2)>"
    assert repr(d.keys()) == "<_KeysView(1, 'x', None)>"


def test_views(any_multidict_class: _Class) -> None:
    d = any_multidict_class([(1, "a"), ("x", "b"), (1, "c")])
    keys = d.keys()
    assert keys & {1, 2} == {1}
    assert {1, 2} & keys == {1}
    assert keys | {2} == {1, "x", 2}
    assert {2} | keys == {1, "x", 2}
    assert keys - {1} == {"x"}
    assert {1, 2} - keys == {2}
    assert keys ^ {1, 2} == {"x", 2}
    assert not keys.isdisjoint({1})
    assert keys.isdisjoint({3})
    items = d.items()
    assert (1, "c") in items
    assert (1, "z") not in items
    assert items & {(1, "a"), (2, "b")} == {(1, "a")}
    assert items - {(1, "a")} == {("x", "b"), (1, "c")}


def test_eq_runs_key_eq(any_multidict_class: _Class) -> None:
    a = any_multidict_class([(Key("k"), 1), ("x", 2)])
    b = any_multidict_class([("k", 1), ("x", 2)])
    assert a == b
    assert b == a
    assert a != any_multidict_class([("k", 2), ("x", 2)])


class Mutator:
    """Mutates the multidict it is in while compared, once armed."""

    target: Any = None

    def __init__(self, text: str, action: str) -> None:
        self.text = text
        self.action = action

    def __hash__(self) -> int:
        return hash(self.text)

    def __eq__(self, other: object) -> bool:
        target, Mutator.target = Mutator.target, None
        if target is not None:
            if self.action == "clear":
                target.clear()
            elif self.action == "add":
                for i in range(30):
                    target.add(f"grow{i}", i)
            else:
                raise RuntimeError("boom")
        if isinstance(other, Mutator):
            return self.text == other.text
        return NotImplemented


OPS: dict[str, Any] = {
    "getitem": lambda d, k: d[k],
    "get": lambda d, k: d.get(k),
    "getall": lambda d, k: d.getall(k, None),
    "contains": lambda d, k: k in d,
    "setitem": lambda d, k: d.__setitem__(k, "v"),
    "add": lambda d, k: d.add(k, "v"),
    "setdefault": lambda d, k: d.setdefault(k, "v"),
    "delitem": lambda d, k: d.pop(k, None),
    "popall": lambda d, k: d.popall(k, None),
    "update": lambda d, k: d.update([(k, "v")]),
    "merge": lambda d, k: d.merge([(k, "v")]),
    "extend": lambda d, k: d.extend([(k, "v")]),
    "keys_and": lambda d, k: d.keys() & {k},
    "items_contains": lambda d, k: (k, 0) in d.items(),
}


@pytest.mark.parametrize("op", OPS)
@pytest.mark.parametrize("action", ["clear", "add", "raise"])
def test_eq_mutating_the_multidict(
    any_multidict_class: _Class, op: str, action: str
) -> None:
    stored = Mutator("m", action)
    d = any_multidict_class([(stored, 0), ("s", 1), (stored, 2)])
    Mutator.target = d
    try:
        OPS[op](d, Mutator("m", action))
    except RuntimeError as exc:
        assert action == "raise" or "changed" in str(exc)
    except KeyError:
        # cleared by __eq__ before the match
        assert action == "clear" and op == "getitem"
    Mutator.target = None
    gc.collect()
    # still consistent: every key is found
    for k, v in list(d.items()):
        assert v in d.getall(k)


class MutateThenRaise:
    """Equal after changing the multidict it is in, then raising."""

    target: Any = None
    calls = 0

    def __hash__(self) -> int:
        return 3

    def __eq__(self, other: object) -> bool:
        MutateThenRaise.calls += 1
        if MutateThenRaise.calls > 1:
            raise RuntimeError("boom")
        MutateThenRaise.target.add("x", 1)
        return True


def test_add_failing_after_the_key_matched(any_multidict_class: _Class) -> None:
    # The add takes the stored key's identity, starts over on the change
    # and then fails; the probe keeps its references.
    d = any_multidict_class([(MutateThenRaise(), 0), (1, 1)])
    MutateThenRaise.target = d
    MutateThenRaise.calls = 0
    probe = MutateThenRaise()
    refs = sys.getrefcount(probe)
    with pytest.raises(RuntimeError, match="boom"):
        d.add(probe, "v")
    MutateThenRaise.target = None
    gc.collect()
    assert MutateThenRaise.calls == 2
    assert sys.getrefcount(probe) == refs
    assert len(d) == 3


@pytest.mark.threaded
def test_threads_mixing_key_types(any_multidict_class: _Class) -> None:
    if any_multidict_class.__module__ == _pure.__name__ and not _pure._FREE_THREADED:
        # the pure-Python reads are unlocked on GIL builds; see
        # _gil_build_race_skip in test_multidict.py
        pytest.skip("pure-Python reads race writers on GIL builds")
    # The table moves between kinds as str-only content is cleared and
    # other keys come back, under lookups that resolve their probes.
    d = any_multidict_class((f"s{i}", i) for i in range(8))
    collide = [Collide() for _ in range(4)]
    stop = threading.Event()

    def writer() -> None:
        for i in range(300):
            d.add(i % 16, i)
            d[collide[i % 4]] = i
            d.setdefault(f"s{i % 8}", i)
            d.pop(i % 16, None)
            if i % 50 == 0:
                d.clear()

    def reader() -> None:
        while not stop.is_set():
            for k in (3, "s3", collide[1], 3.0, None):
                d.get(k)
                d.getall(k, None)
                _ = k in d
            try:
                list(d.items())
            except RuntimeError:  # pragma: no cover
                pass  # changed during iteration

    with ThreadPoolExecutor(4) as pool:
        readers = [pool.submit(reader) for _ in range(2)]
        try:
            writers = [pool.submit(writer) for _ in range(2)]
            for f in writers:
                f.result(timeout=60)
        finally:
            stop.set()
        for f in readers:
            f.result(timeout=60)
    for k, v in list(d.items()):
        assert v in d.getall(k)
