"""Registry of benchmarked operations, shared by every benchmark entry point.

``benchmark.py`` drives it with :mod:`pyperf` wall clock, ``callgrind_child.py``
drives it under Callgrind, and ``render_tables.py`` turns the results into the
tables in ``docs/benchmark.rst``.

Every operation has the same shape: ``rounds x (untimed setup + inner timed
operations)``.  Keeping the rebuild in ``setup`` is what lets destructive
operations such as ``pop()`` be measured without counting the cost of putting
the mapping back together.

``Case.noop`` is the same loop with the operation elided.  Subtracting it
removes the driving ``for``, which is worth ~470 instructions per iteration and
would otherwise compress every ratio toward 1.  It does not remove the bound
method lookup and call of method-based operations; a caller pays those too.
"""

import enum
import importlib
from collections.abc import Callable
from dataclasses import dataclass
from typing import Any

SIZE = 200
UPDATE_SIZE = 100
#: A request's worth of headers.  The allocation an operation makes is a
#: fixed cost, so at SIZE it is divided by 200 entries and disappears;
#: these are the shapes where it is most of the work.
SMALL = 20


class Kind(enum.Flag):
    DICT = enum.auto()
    MULTIDICT = enum.auto()


BOTH = Kind.DICT | Kind.MULTIDICT


@dataclass(frozen=True, slots=True)
class Case:
    setup: Callable[[], Any]
    run: Callable[[Any], None]
    noop: Callable[[Any], None]


@dataclass(frozen=True, slots=True)
class Impl:
    id: str
    label: str
    kind: Kind
    load: Callable[[], type]
    load_istr: Callable[[], type] | None = None
    load_proxy: Callable[[], type] | None = None


@dataclass(frozen=True, slots=True)
class Operation:
    id: str
    label: str
    kinds: Kind
    make: Callable[..., Case]
    inner: int
    rounds: tuple[int, int]
    istr: bool = False
    proxy: bool = False
    size: int = SIZE


def _attr(module: str, name: str) -> Callable[[], type]:
    def load() -> type:
        return getattr(importlib.import_module(module), name)  # type: ignore[no-any-return]

    return load


IMPLEMENTATIONS = (
    Impl("dict", "``dict``", Kind.DICT, lambda: dict),
    Impl(
        "multidict_c",
        "``MultiDict``",
        Kind.MULTIDICT,
        _attr("multidict._multidict", "MultiDict"),
        _attr("multidict._multidict", "istr"),
        _attr("multidict._multidict", "MultiDictProxy"),
    ),
    Impl(
        "cimultidict_c",
        "``CIMultiDict``",
        Kind.MULTIDICT,
        _attr("multidict._multidict", "CIMultiDict"),
        _attr("multidict._multidict", "istr"),
        _attr("multidict._multidict", "CIMultiDictProxy"),
    ),
    Impl(
        "multidict_py",
        "``MultiDict`` (Python)",
        Kind.MULTIDICT,
        _attr("multidict._multidict_py", "MultiDict"),
        _attr("multidict._multidict_py", "istr"),
        _attr("multidict._multidict_py", "MultiDictProxy"),
    ),
    Impl(
        "cimultidict_py",
        "``CIMultiDict`` (Python)",
        Kind.MULTIDICT,
        _attr("multidict._multidict_py", "CIMultiDict"),
        _attr("multidict._multidict_py", "istr"),
        _attr("multidict._multidict_py", "CIMultiDictProxy"),
    ),
)

IMPLEMENTATIONS_BY_ID = {impl.id: impl for impl in IMPLEMENTATIONS}


def _keys(size: int) -> list[str]:
    return [f"key{i}" for i in range(size)]


def _make_ctor_items(cls: type, size: int) -> Case:
    items = [(k, k) for k in _keys(size)]
    sink = None

    def setup() -> Any:
        return items

    def run(it: Any) -> None:
        nonlocal sink
        sink = cls(it)

    def noop(it: Any) -> None:
        nonlocal sink
        sink = it

    return Case(setup, run, noop)


def _make_ctor_empty(cls: type, size: int) -> Case:
    sink: Any = None

    def setup() -> Any:
        return None

    def run(_: Any) -> None:
        nonlocal sink
        sink = cls()

    def noop(_: Any) -> None:
        nonlocal sink
        sink = None

    return Case(setup, run, noop)


def _make_items_view(cls: type, size: int) -> Case:
    target = cls((k, k) for k in _keys(size))
    sink = None

    def setup() -> Any:
        return target

    def run(d: Any) -> None:
        nonlocal sink
        sink = d.items()

    def noop(d: Any) -> None:
        nonlocal sink
        sink = d

    return Case(setup, run, noop)


def _make_iter_new(cls: type, size: int) -> Case:
    target = cls((k, k) for k in _keys(size))
    sink = None

    def setup() -> Any:
        return target

    def run(d: Any) -> None:
        nonlocal sink
        sink = iter(d)

    def noop(d: Any) -> None:
        nonlocal sink
        sink = d

    return Case(setup, run, noop)


def _make_copy(cls: type, size: int) -> Case:
    target = cls((k, k) for k in _keys(size))
    sink = None

    def setup() -> Any:
        return target

    def run(d: Any) -> None:
        nonlocal sink
        sink = d.copy()

    def noop(d: Any) -> None:
        nonlocal sink
        sink = d

    return Case(setup, run, noop)


def _make_getitem_hit(cls: type, size: int) -> Case:
    keys = _keys(size)
    target = cls((k, k) for k in keys)
    sink = None

    def setup() -> Any:
        return target

    def run(d: Any) -> None:
        nonlocal sink
        for k in keys:
            sink = d[k]

    def noop(d: Any) -> None:
        nonlocal sink
        for k in keys:
            sink = k

    return Case(setup, run, noop)


def _make_get_miss(cls: type, size: int) -> Case:
    target = cls((k, k) for k in _keys(size))
    misses = [f"{k}-absent" for k in _keys(size)]
    sink = None

    def setup() -> Any:
        return target

    def run(d: Any) -> None:
        nonlocal sink
        for k in misses:
            sink = d.get(k)

    def noop(d: Any) -> None:
        nonlocal sink
        for k in misses:
            sink = k

    return Case(setup, run, noop)


def _make_contains_hit(cls: type, size: int) -> Case:
    keys = _keys(size)
    target = cls((k, k) for k in keys)
    sink = None

    def setup() -> Any:
        return target

    def run(d: Any) -> None:
        nonlocal sink
        for k in keys:
            sink = k in d

    def noop(d: Any) -> None:
        nonlocal sink
        for k in keys:
            sink = k

    return Case(setup, run, noop)


def _make_setitem_replace(cls: type, size: int) -> Case:
    keys = _keys(size)
    target = cls((k, k) for k in keys)

    def setup() -> Any:
        return target

    def run(d: Any) -> None:
        for k in keys:
            d[k] = k

    def noop(d: Any) -> None:
        for k in keys:
            pass

    return Case(setup, run, noop)


def _make_setitem_insert(cls: type, size: int) -> Case:
    keys = _keys(size)
    empty = cls()

    def setup() -> Any:
        return empty.copy()

    def run(d: Any) -> None:
        for k in keys:
            d[k] = k

    def noop(d: Any) -> None:
        for k in keys:
            pass

    return Case(setup, run, noop)


def _make_setdefault_new(cls: type, size: int) -> Case:
    keys = _keys(size)
    empty = cls()
    sink = None

    def setup() -> Any:
        return empty.copy()

    def run(d: Any) -> None:
        nonlocal sink
        for k in keys:
            sink = d.setdefault(k, k)

    def noop(d: Any) -> None:
        nonlocal sink
        for k in keys:
            sink = k

    return Case(setup, run, noop)


def _make_delitem(cls: type, size: int) -> Case:
    keys = _keys(size)
    base = cls((k, k) for k in keys)

    def setup() -> Any:
        return base.copy()

    def run(d: Any) -> None:
        for k in keys:
            del d[k]

    def noop(d: Any) -> None:
        for k in keys:
            pass

    return Case(setup, run, noop)


def _make_pop(cls: type, size: int) -> Case:
    keys = _keys(size)
    base = cls((k, k) for k in keys)
    sink = None

    def setup() -> Any:
        return base.copy()

    def run(d: Any) -> None:
        nonlocal sink
        for k in keys:
            sink = d.pop(k)

    def noop(d: Any) -> None:
        nonlocal sink
        for k in keys:
            sink = k

    return Case(setup, run, noop)


def _make_popitem(cls: type, size: int) -> Case:
    keys = _keys(size)
    base = cls((k, k) for k in keys)
    sink = None

    def setup() -> Any:
        return base.copy()

    def run(d: Any) -> None:
        nonlocal sink
        for _ in keys:
            sink = d.popitem()

    def noop(d: Any) -> None:
        nonlocal sink
        for k in keys:
            sink = k

    return Case(setup, run, noop)


def _make_update(cls: type, size: int) -> Case:
    base = cls((k, k) for k in _keys(size))
    other = {k: k for k in _keys(UPDATE_SIZE)}

    def setup() -> Any:
        return base.copy()

    def run(d: Any) -> None:
        d.update(other)

    def noop(d: Any) -> None:
        pass

    return Case(setup, run, noop)


def _make_clear(cls: type, size: int) -> Case:
    base = cls((k, k) for k in _keys(size))

    def setup() -> Any:
        return base.copy()

    def run(d: Any) -> None:
        d.clear()

    def noop(d: Any) -> None:
        pass

    return Case(setup, run, noop)


def _make_iter_keys(cls: type, size: int) -> Case:
    keys = _keys(size)
    target = cls((k, k) for k in keys)
    sink = None

    def setup() -> Any:
        return target

    def run(d: Any) -> None:
        nonlocal sink
        for k in d:
            sink = k

    def noop(d: Any) -> None:
        nonlocal sink
        for k in keys:
            sink = k

    return Case(setup, run, noop)


def _make_iter_items(cls: type, size: int) -> Case:
    pairs = [(k, k) for k in _keys(size)]
    target = cls(pairs)
    sink = None

    def setup() -> Any:
        return target

    def run(d: Any) -> None:
        nonlocal sink
        for k, v in d.items():
            sink = v

    def noop(d: Any) -> None:
        nonlocal sink
        for k, v in pairs:
            sink = v

    return Case(setup, run, noop)


def _make_add(cls: type, size: int, istr_cls: type | None = None) -> Case:
    wrap = istr_cls if istr_cls is not None else str
    keys = [wrap(k) for k in _keys(size)]
    empty = cls()

    def setup() -> Any:
        return empty.copy()

    def run(d: Any) -> None:
        for k in keys:
            d.add(k, k)

    def noop(d: Any) -> None:
        for k in keys:
            pass

    return Case(setup, run, noop)


def _make_proxy_new(cls: type, size: int, proxy_cls: type | None = None) -> Case:
    assert proxy_cls is not None
    target = cls((k, k) for k in _keys(size))
    sink = None

    def setup() -> Any:
        return target

    def run(d: Any) -> None:
        nonlocal sink
        sink = proxy_cls(d)

    def noop(d: Any) -> None:
        nonlocal sink
        sink = d

    return Case(setup, run, noop)


def _make_getall(cls: type, size: int, istr_cls: type | None = None) -> Case:
    wrap = istr_cls if istr_cls is not None else str
    keys = [wrap(k) for k in _keys(size)]
    target = cls((k, k) for k in keys)
    sink = None

    def setup() -> Any:
        return target

    def run(d: Any) -> None:
        nonlocal sink
        for k in keys:
            sink = d.getall(k)

    def noop(d: Any) -> None:
        nonlocal sink
        for k in keys:
            sink = k

    return Case(setup, run, noop)


def _make_getitem_istr(cls: type, size: int, istr_cls: type | None = None) -> Case:
    wrap = istr_cls if istr_cls is not None else str
    keys = [wrap(k) for k in _keys(size)]
    target = cls((k, k) for k in keys)
    sink = None

    def setup() -> Any:
        return target

    def run(d: Any) -> None:
        nonlocal sink
        for k in keys:
            sink = d[k]

    def noop(d: Any) -> None:
        nonlocal sink
        for k in keys:
            sink = k

    return Case(setup, run, noop)


def _make_setitem_istr(cls: type, size: int, istr_cls: type | None = None) -> Case:
    wrap = istr_cls if istr_cls is not None else str
    keys = [wrap(k) for k in _keys(size)]
    target = cls((k, k) for k in keys)

    def setup() -> Any:
        return target

    def run(d: Any) -> None:
        for k in keys:
            d[k] = k

    def noop(d: Any) -> None:
        for k in keys:
            pass

    return Case(setup, run, noop)


#: Operations common to :class:`dict` and the multidict classes.  These are the
#: rows of the comparison tables in ``docs/benchmark.rst``.
SHARED_OPERATIONS = (
    Operation("ctor_items", "``cls(items)``", BOTH, _make_ctor_items, 1, (20, 60)),
    Operation("ctor_empty", "``cls()``", BOTH, _make_ctor_empty, 1, (20, 60)),
    Operation(
        "ctor_small",
        "``cls(items)``, 20 items",
        BOTH,
        _make_ctor_items,
        1,
        (20, 60),
        size=SMALL,
    ),
    Operation("copy", "``d.copy()``", BOTH, _make_copy, 1, (20, 60)),
    Operation(
        "copy_small",
        "``d.copy()``, 20 items",
        BOTH,
        _make_copy,
        1,
        (20, 60),
        size=SMALL,
    ),
    Operation("getitem_hit", "``d[key]``", BOTH, _make_getitem_hit, SIZE, (2, 6)),
    Operation("get_miss", "``d.get(key)``, miss", BOTH, _make_get_miss, SIZE, (2, 6)),
    Operation("contains_hit", "``key in d``", BOTH, _make_contains_hit, SIZE, (2, 6)),
    Operation(
        "setitem_replace",
        "``d[key] = v``, existing key",
        BOTH,
        _make_setitem_replace,
        SIZE,
        (2, 6),
    ),
    Operation(
        "setitem_insert",
        "``d[key] = v``, new key",
        BOTH,
        _make_setitem_insert,
        SIZE,
        (2, 6),
    ),
    Operation(
        "setdefault_new",
        "``d.setdefault(key, v)``, new key",
        BOTH,
        _make_setdefault_new,
        SIZE,
        (2, 6),
    ),
    Operation("delitem", "``del d[key]``", BOTH, _make_delitem, SIZE, (2, 6)),
    Operation("pop", "``d.pop(key)``", BOTH, _make_pop, SIZE, (2, 6)),
    Operation("popitem", "``d.popitem()``", BOTH, _make_popitem, SIZE, (2, 6)),
    Operation(
        "update",
        "``d.update(other)``, 100 existing keys",
        BOTH,
        _make_update,
        1,
        (20, 60),
    ),
    Operation("clear", "``d.clear()``", BOTH, _make_clear, 1, (20, 60)),
    Operation("iter_keys", "``for k in d``", BOTH, _make_iter_keys, SIZE, (2, 6)),
    Operation(
        "iter_items", "``for k, v in d.items()``", BOTH, _make_iter_items, SIZE, (2, 6)
    ),
    Operation("items_view", "``d.items()``", BOTH, _make_items_view, 1, (20, 60)),
    Operation("iter_new", "``iter(d)``", BOTH, _make_iter_new, 1, (20, 60)),
)

#: Operations :class:`dict` has no counterpart for.  Available to the runners,
#: never rendered into the comparison tables.
MULTIDICT_OPERATIONS = (
    Operation("add", "``d.add(key, v)``", Kind.MULTIDICT, _make_add, SIZE, (2, 6)),
    Operation(
        "getall", "``d.getall(key)``", Kind.MULTIDICT, _make_getall, SIZE, (2, 6)
    ),
    Operation(
        "proxy_new",
        "``Proxy(d)``",
        Kind.MULTIDICT,
        _make_proxy_new,
        1,
        (20, 60),
        proxy=True,
        size=SMALL,
    ),
    Operation(
        "getitem_istr",
        "``d[key]``, ``istr`` key",
        Kind.MULTIDICT,
        _make_getitem_istr,
        SIZE,
        (2, 6),
        istr=True,
    ),
    Operation(
        "setitem_istr",
        "``d[key] = v``, ``istr`` key",
        Kind.MULTIDICT,
        _make_setitem_istr,
        SIZE,
        (2, 6),
        istr=True,
    ),
    Operation(
        "add_istr",
        "``d.add(key, v)``, ``istr`` key",
        Kind.MULTIDICT,
        _make_add,
        SIZE,
        (2, 6),
        istr=True,
    ),
)

OPERATIONS = SHARED_OPERATIONS + MULTIDICT_OPERATIONS
OPERATIONS_BY_ID = {op.id: op for op in OPERATIONS}


def build(op: Operation, impl: Impl) -> Case:
    """Instantiate ``op`` for ``impl``, importing the backend on demand."""
    cls = impl.load()
    if op.istr:
        if impl.load_istr is None:
            raise ValueError(f"{impl.id} has no istr type")
        return op.make(cls, op.size, impl.load_istr())
    if op.proxy:
        if impl.load_proxy is None:
            raise ValueError(f"{impl.id} has no proxy type")
        return op.make(cls, op.size, impl.load_proxy())
    return op.make(cls, op.size)


def selected(
    *, impl_id: str | None = None, shared_only: bool = False
) -> list[tuple[Operation, Impl]]:
    """Enumerate the (operation, implementation) cells to measure."""
    impls = (
        [IMPLEMENTATIONS_BY_ID[impl_id]]
        if impl_id is not None
        else list(IMPLEMENTATIONS)
    )
    ops = SHARED_OPERATIONS if shared_only else OPERATIONS
    return [(op, impl) for op in ops for impl in impls if op.kinds & impl.kind]
