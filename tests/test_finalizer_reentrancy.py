"""A finalizer that mutates the multidict from inside one of its mutating
methods leaves the C implementation in the same state as the pure-Python
one, which serves as the oracle.

Every method runs on a multidict whose keys (or values) run the finalizer
when dropped, and the methods that consume an argument get an item that
runs it too. Each finalizer mutates the same multidict in a different way.
"""

import sys
from collections.abc import Callable, Iterator
from types import ModuleType
from typing import Any

import pytest
from finalizer_helpers import (
    FinalizerValue,
    Fuse,
    finalizer_key,
    finalizer_pair,
)

pytestmark = [
    pytest.mark.c_extension,
    pytest.mark.skipif(
        sys.implementation.name == "pypy",
        reason="__del__ does not run promptly on PyPy",
    ),
]

_ACTIONS: dict[str, Callable[[Any], object]] = {
    "add": lambda d: d.add("new", "N"),
    "add_same": lambda d: d.add("a", "A2"),
    "set_same": lambda d: d.__setitem__("a", "A3"),
    "update_same": lambda d: d.update([("a", "A4"), ("a", "A5")]),
    "del_other": lambda d: d.popall("b", None),
    "clear": lambda d: d.clear(),
    "reinit": lambda d: d.__init__([("r", "R")]),
    "grow": lambda d: d.extend((f"g{i}", i) for i in range(20)),
    "popitem": lambda d: d.popitem() if d else None,
}

_METHODS: dict[str, Callable[[Any, Callable[..., object]], object]] = {
    "setitem": lambda d, pairs: d.__setitem__("a", "S"),
    "delitem": lambda d, pairs: d.__delitem__("a"),
    "add": lambda d, pairs: d.add("a", "N"),
    "setdefault": lambda d, pairs: d.setdefault("a", "D"),
    "pop": lambda d, pairs: d.pop("a"),
    "popone": lambda d, pairs: d.popone("a"),
    "popall": lambda d, pairs: d.popall("a"),
    "popitem": lambda d, pairs: d.popitem(),
    "clear": lambda d, pairs: d.clear(),
    "extend": lambda d, pairs: d.extend(pairs("a", "E")),
    "update": lambda d, pairs: d.update(pairs("a", "U")),
    "update_dict": lambda d, pairs: d.update({"a": "U", "q": "Q"}),
    "update_md": lambda d, pairs: d.update(type(d)([("a", "U"), ("q", "Q")])),
    "merge": lambda d, pairs: d.merge(pairs("z", "M")),
    "init": lambda d, pairs: d.__init__(pairs("i", "I")),
    "init_md": lambda d, pairs: d.__init__(type(d)(i="I")),
    "copy": lambda d, pairs: d.copy(),
}

# Methods that only store or read, so no finalizer can run inside them;
# they are here to show that, and that the backends agree on the result.
_QUIET = {"add", "setdefault", "copy"}
# Methods that hand a removed value back to the caller, which keeps it.
_RETURN_VALUE = {"pop", "popone", "popall", "popitem"}
# Methods that consume finalizer-bearing pairs.
_PAIRS = {"extend", "update", "merge", "init"}


def _fires(cls_name: str, side: str, method: str) -> bool:
    """Whether the call itself drops a finalizer-bearing object."""
    return not (
        method in _QUIET
        or (side == "value" and method in _RETURN_VALUE)
        # a MultiDict hands back the popped key itself
        or (side == "key" and method == "popitem" and cls_name == "MultiDict")
        # a CIMultiDict stores an istr copy, never the key itself, so only
        # the pairs a method consumes drop one
        or (side == "key" and cls_name == "CIMultiDict" and method not in _PAIRS)
    )


def _plain(obj: object) -> object:
    """What a result looks like with the finalizer objects' types removed."""
    if isinstance(obj, (tuple, list)):
        return [_plain(item) for item in obj]
    if isinstance(obj, str):
        return str(obj)
    if hasattr(obj, "items"):
        return _plain(list(obj.items()))
    return repr(obj)


def _run(
    module: ModuleType, cls_name: str, method: str, side: str, action: str
) -> tuple[object, object, int, int]:
    d = getattr(module, cls_name)()
    fuse = Fuse(lambda: _ACTIONS[action](d))

    def pairs(key: str, value: str) -> Iterator[object]:
        # The item the multidict consumes second is the one that fires.
        yield ("q", "Q")
        yield finalizer_pair(fuse, key, value)
        yield (key, value + "2")

    for i, key in enumerate(["a", "b", "a", "c"]):
        if side == "key":
            d.add(finalizer_key(fuse, key), f"v{i}")
        else:
            d.add(key, FinalizerValue(fuse, f"v{i}"))

    fuse.on = True
    try:
        result = _METHODS[method](d, pairs)
    finally:
        fuse.on = False
    return _plain(result), _plain(d), len(d), fuse.fired


@pytest.mark.parametrize("action", _ACTIONS)
@pytest.mark.parametrize("method", _METHODS)
@pytest.mark.parametrize("side", ["key", "value"])
@pytest.mark.parametrize("cls_name", ["MultiDict", "CIMultiDict"])
def test_finalizer_mutation_matches_pure_python(
    cls_name: str, side: str, method: str, action: str
) -> None:
    c_module = pytest.importorskip("multidict._multidict")
    py_module = pytest.importorskip("multidict._multidict_py")

    c_outcome = _run(c_module, cls_name, method, side, action)
    py_outcome = _run(py_module, cls_name, method, side, action)
    assert c_outcome == py_outcome
    _, items, length, fired = py_outcome
    assert length == len(items)  # type: ignore[arg-type]
    assert (fired > 0) == _fires(cls_name, side, method)
