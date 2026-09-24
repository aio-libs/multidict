"""The C extension keeps bounded pools of freed hash tables and object
shells in its module state and hands them out again.

None of that is visible from Python, and these tests are mostly about
keeping it that way: an object built out of a recycled block has to
behave exactly like a freshly allocated one, whatever the block was used
for before. They run on both backends, where the pure-Python one is the
control that says the expectations are about multidict and not about the
pool. The one test that checks the pool is actually used is C-extension
only, since there is nothing to reuse otherwise.
"""

import gc
import sys
import sysconfig
from collections.abc import Iterator

import pytest

from multidict import MultiDict

# Table sizes are powers of two from 8 up, each holding two thirds of its
# slots, so a multidict crosses into a new one at 6, 11, 22, 43 and 86
# items. Those and the last item that still fits the size below are where
# a recycled table of the wrong size class would show up.
SIZE_CLASS_EDGES = (1, 5, 6, 10, 11, 21, 22, 42, 43, 85, 86)

# More rounds than any pool is deep, so the tests cover a pool filling
# up, handing blocks back out, and overflowing.
ROUNDS = 40

_FREE_THREADED = bool(sysconfig.get_config_var("Py_GIL_DISABLED"))
# PyPy has no allocation counter at all, and PYTHONMALLOC=malloc makes
# CPython's count nothing.
_COUNTS_BLOCKS = getattr(sys, "getallocatedblocks", lambda: 0)() > 0


@pytest.fixture
def gc_off() -> Iterator[None]:
    """Keep a collection from landing in the middle of a measurement."""
    gc.collect()
    gc.disable()
    yield
    gc.enable()


def _pairs(count: int) -> list[tuple[str, str]]:
    return [(f"k{i}", f"v{i}") for i in range(count)]


@pytest.mark.parametrize("count", SIZE_CLASS_EDGES)
def test_clear_and_refill_round_trips(
    any_multidict_class: type[MultiDict[str]], count: int
) -> None:
    """A table handed back and taken again holds the same thing."""
    pairs = _pairs(count)
    md = any_multidict_class(pairs)
    for _ in range(ROUNDS):
        md.clear()
        assert list(md.items()) == []
        md.extend(pairs)
        assert list(md.items()) == pairs


@pytest.mark.parametrize("count", SIZE_CLASS_EDGES)
def test_build_and_drop_round_trips(
    any_multidict_class: type[MultiDict[str]], count: int
) -> None:
    """Every table in a build-and-drop loop comes back empty."""
    pairs = _pairs(count)
    for _ in range(ROUNDS):
        md = any_multidict_class(pairs)
        assert list(md.items()) == pairs
        del md


@pytest.mark.parametrize("count", SIZE_CLASS_EDGES)
def test_copies_interleaved_with_clears(
    any_multidict_class: type[MultiDict[str]], count: int
) -> None:
    """A copy is built differently from a fresh table but shares the pool.

    A clone is copied over byte for byte rather than initialised, so a
    block passing between the two paths is where the two would disagree.
    """
    pairs = _pairs(count)
    base = any_multidict_class(pairs)
    for _ in range(ROUNDS):
        clone = base.copy()
        assert list(clone.items()) == pairs
        clone.clear()
        fresh = any_multidict_class(pairs)
        assert list(fresh.items()) == pairs
        del clone, fresh


def test_growing_back_over_a_freed_table(
    any_multidict_class: type[MultiDict[str]],
) -> None:
    """Shrinking and regrowing walks the size classes in both directions."""
    pairs = _pairs(max(SIZE_CLASS_EDGES))
    md = any_multidict_class()
    for _ in range(ROUNDS):
        for key, value in pairs:
            md[key] = value
        assert len(md) == len(pairs)
        for key, _value in reversed(pairs):
            del md[key]
        assert len(md) == 0


def test_live_multidicts_never_share_storage(
    any_multidict_class: type[MultiDict[str]],
) -> None:
    """No block is ever handed out twice."""
    live: list[MultiDict[str]] = []
    for i in range(ROUNDS * 4):
        live.append(any_multidict_class([(f"k{i}", f"v{i}")]))
        if len(live) > ROUNDS:
            del live[0]
        assert len({id(md) for md in live}) == len(live)
    for i, md in enumerate(live, start=len(live) * 3):
        assert list(md.items()) == [(f"k{i}", f"v{i}")]


@pytest.mark.c_extension
@pytest.mark.skipif(_FREE_THREADED, reason="nothing is pooled on this build")
@pytest.mark.skipif(not _COUNTS_BLOCKS, reason="allocations are not counted")
@pytest.mark.usefixtures("gc_off")
def test_freed_blocks_are_reused() -> None:
    """The pools are actually used, not just correct when bypassed.

    A batch built on a pool an identical batch has just filled takes
    fewer blocks from the allocator than one built on a drained pool.
    Draining is what ties the comparison to the pools: the two batches
    do the same work otherwise, so with MULTIDICT_NO_FREELIST set both
    measure the same and this fails.
    """
    c_ext = pytest.importorskip("multidict._multidict")

    def blocks_for_a_batch(drain: bool) -> int:
        if drain:
            c_ext._freelist_clear()
        before = sys.getallocatedblocks()
        batch = [MultiDict(_pairs(20)) for _ in range(ROUNDS)]
        taken = sys.getallocatedblocks() - before
        del batch
        return taken

    blocks_for_a_batch(True)  # let the interpreter's own caches fill
    cold = blocks_for_a_batch(True)
    warm = blocks_for_a_batch(False)
    assert warm < cold, (cold, warm)
