"""Duplicate-key bookkeeping on large tables and across resizes."""

from collections.abc import Iterable, Iterator

import pytest

from multidict import MultiDict

# More entries than the C bitmap's 4 KB inline buffer covers (32768 bits).
BIG = 40000


def _model_update(
    pairs: Iterable[tuple[str, int]], new_pairs: Iterable[tuple[str, int]]
) -> list[tuple[str, int]]:
    entries = [[k, v, True] for k, v in pairs]
    by_key: dict[str, list[int]] = {}
    for i, (k, _, _) in enumerate(entries):
        by_key.setdefault(k, []).append(i)  # type: ignore[arg-type]
    written: set[int] = set()
    for k, v in new_pairs:
        found = False
        for i in by_key.get(k, []):
            if i in written:
                continue
            e = entries[i]
            if not found:
                found = True
                e[1] = v
                e[2] = True
                written.add(i)
            else:
                e[2] = False
        if not found:
            written.add(len(entries))
            by_key.setdefault(k, []).append(len(entries))
            entries.append([k, v, True])
    return [(k, v) for k, v, live in entries if live]  # type: ignore[misc]


def _model_merge(
    pairs: list[tuple[str, int]], new_pairs: Iterable[tuple[str, int]]
) -> list[tuple[str, int]]:
    existing = {k for k, _ in pairs}
    return pairs + [(k, v) for k, v in new_pairs if k not in existing]


def _big_pairs() -> list[tuple[str, int]]:
    pairs = [(f"k{i}", i) for i in range(BIG)]
    # More values than the C finder tracks before starting its bitmap.
    for i in range(0, BIG, 3500):
        pairs.insert(i, ("dup", -i))
    return pairs


def test_getall_large_table(any_multidict_class: type[MultiDict[int]]) -> None:
    pairs = _big_pairs()
    md = any_multidict_class(pairs)
    expected = [v for k, v in pairs if k == "dup"]
    assert md.getall("dup") == expected
    assert md.getall("k123") == [123]
    assert md.getall("missing", None) is None
    assert ("dup", -38500) in md.items()
    assert ("dup", 1) not in md.items()


def test_to_dict_large_table(any_multidict_class: type[MultiDict[int]]) -> None:
    pairs = _big_pairs()
    md = any_multidict_class(pairs)
    d = md.to_dict()
    assert len(d) == BIG + 1
    assert d["dup"] == [v for k, v in pairs if k == "dup"]
    assert d["k39999"] == [39999]


def test_setitem_large_table(any_multidict_class: type[MultiDict[int]]) -> None:
    md = any_multidict_class(_big_pairs())
    md["dup"] = 7
    assert md.getall("dup") == [7]
    assert len(md) == BIG + 1
    assert next(iter(md.items())) == ("dup", 7)


def test_update_large_table(any_multidict_class: type[MultiDict[int]]) -> None:
    pairs = _big_pairs()
    new_pairs = [("dup", 1), ("k5", 5), ("dup", 2), ("new", 3), ("new", 4)]
    md = any_multidict_class(pairs)
    md.update(new_pairs)
    assert list(md.items()) == _model_update(pairs, new_pairs)


@pytest.mark.parametrize("size", [8, 3000])
@pytest.mark.parametrize("deleted", [0, 3])
def test_update_resizes_mid_batch(
    any_multidict_class: type[MultiDict[int]], deleted: int, size: int
) -> None:
    """Items come from a generator, so nothing is reserved up front and the
    batch resizes as it goes: first compacting away the deleted entries,
    then growing, with marks from before each resize still honoured."""
    pairs = [(f"k{i % 4}", i) for i in range(8)]
    pairs += [(f"b{i}", i) for i in range(size - 8)]
    md = any_multidict_class(pairs)
    for i in range(deleted):
        del md[f"k{i}"]
    pairs = [(k, v) for k, v in pairs if k not in {f"k{i}" for i in range(deleted)}]
    new_pairs = [("k3", -1)]
    new_pairs += [(f"n{i % (size // 4)}", i) for i in range(size * 4)]
    new_pairs += [("k3", -2), ("k3", -3), ("k3", -4)]

    md.update(p for p in new_pairs)
    assert list(md.items()) == _model_update(pairs, new_pairs)


def test_merge_resizes_mid_batch(
    any_multidict_class: type[MultiDict[int]],
) -> None:
    pairs = [(f"k{i % 4}", i) for i in range(8)]
    md = any_multidict_class(pairs)
    del md["k0"]
    pairs = [(k, v) for k, v in pairs if k != "k0"]
    new_pairs = [(f"n{i % 50}", i) for i in range(200)] + [("k1", -1)]

    md.merge(p for p in new_pairs)
    assert list(md.items()) == _model_merge(pairs, new_pairs)


def test_update_survives_mutation_between_items(
    any_multidict_class: type[MultiDict[int]],
) -> None:
    """A mutation made from outside the batch invalidates its bookkeeping;
    what the batch then does to repeated keys is unspecified, but the
    table has to come out whole, including entries it deletes later."""
    md = any_multidict_class([("a", 0), ("b", 0), ("c", 0), ("c", 0)])

    def items() -> Iterator[tuple[str, int]]:
        yield "a", 1
        md["z"] = 9
        yield "a", 2
        yield "c", 3

    md.update(items())
    assert len(md) == len(list(md.items()))
    assert md.getall("a")[-1] == 2
    assert md["b"] == 0
    assert md.getall("c") == [3]
    assert md["z"] == 9
