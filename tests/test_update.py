from collections import deque
from collections.abc import Callable, Iterator

import pytest

from multidict import CIMultiDict, MultiDict
from multidict._multidict_py import MultiDict as PyMultiDict

_MD_Classes = type[MultiDict[int]] | type[CIMultiDict[int]]


def test_update_replace(any_multidict_class: _MD_Classes) -> None:
    obj1 = any_multidict_class([("a", 1), ("b", 2), ("a", 3), ("c", 10)])
    obj2 = any_multidict_class([("a", 4), ("b", 5), ("a", 6)])
    obj1.update(obj2)
    expected = [("a", 4), ("b", 5), ("a", 6), ("c", 10)]
    assert list(obj1.items()) == expected


def test_update_append(any_multidict_class: _MD_Classes) -> None:
    obj1 = any_multidict_class([("a", 1), ("b", 2), ("a", 3), ("c", 10)])
    obj2 = any_multidict_class([("a", 4), ("a", 5), ("a", 6)])
    obj1.update(obj2)
    expected = [("a", 4), ("b", 2), ("a", 5), ("c", 10), ("a", 6)]
    assert list(obj1.items()) == expected


def test_update_remove(any_multidict_class: _MD_Classes) -> None:
    obj1 = any_multidict_class([("a", 1), ("b", 2), ("a", 3), ("c", 10)])
    obj2 = any_multidict_class([("a", 4)])
    obj1.update(obj2)
    expected = [("a", 4), ("b", 2), ("c", 10)]
    assert list(obj1.items()) == expected


def test_update_replace_seq(any_multidict_class: _MD_Classes) -> None:
    obj1 = any_multidict_class([("a", 1), ("b", 2), ("a", 3), ("c", 10)])
    obj2 = [("a", 4), ("b", 5), ("a", 6)]
    obj1.update(obj2)
    expected = [("a", 4), ("b", 5), ("a", 6), ("c", 10)]
    assert list(obj1.items()) == expected


def test_update_replace_seq2(any_multidict_class: _MD_Classes) -> None:
    obj1 = any_multidict_class([("a", 1), ("b", 2), ("a", 3), ("c", 10)])
    obj1.update([("a", 4)], b=5, a=6)
    expected = [("a", 4), ("b", 5), ("a", 6), ("c", 10)]
    assert list(obj1.items()) == expected


def test_update_append_seq(any_multidict_class: _MD_Classes) -> None:
    obj1 = any_multidict_class([("a", 1), ("b", 2), ("a", 3), ("c", 10)])
    obj2 = [("a", 4), ("a", 5), ("a", 6)]
    obj1.update(obj2)
    expected = [("a", 4), ("b", 2), ("a", 5), ("c", 10), ("a", 6)]
    assert list(obj1.items()) == expected


def test_update_remove_seq(any_multidict_class: _MD_Classes) -> None:
    obj1 = any_multidict_class([("a", 1), ("b", 2), ("a", 3), ("c", 10)])
    obj2 = [("a", 4)]
    obj1.update(obj2)
    expected = [("a", 4), ("b", 2), ("c", 10)]
    assert list(obj1.items()) == expected


def test_update_md(case_sensitive_multidict_class: type[CIMultiDict[str]]) -> None:
    d = case_sensitive_multidict_class()
    d.add("key", "val1")
    d.add("key", "val2")
    d.add("key2", "val3")

    d.update(key="val")

    assert [("key", "val"), ("key2", "val3")] == list(d.items())


def test_update_istr_ci_md(
    case_insensitive_multidict_class: type[CIMultiDict[str]],
    case_insensitive_str_class: type[str],
) -> None:
    d = case_insensitive_multidict_class()
    d.add(case_insensitive_str_class("KEY"), "val1")
    d.add("key", "val2")
    d.add("key2", "val3")

    d.update({case_insensitive_str_class("key"): "val"})

    assert [("key", "val"), ("key2", "val3")] == list(d.items())


def test_update_ci_md(case_insensitive_multidict_class: type[CIMultiDict[str]]) -> None:
    d = case_insensitive_multidict_class()
    d.add("KEY", "val1")
    d.add("key", "val2")
    d.add("key2", "val3")

    d.update(Key="val")

    assert [("Key", "val"), ("key2", "val3")] == list(d.items())


def test_update_list_arg_and_kwds(any_multidict_class: _MD_Classes) -> None:
    obj = any_multidict_class()
    arg = [("a", 1)]
    obj.update(arg, b=2)
    assert list(obj.items()) == [("a", 1), ("b", 2)]
    assert arg == [("a", 1)]


def test_update_tuple_arg_and_kwds(any_multidict_class: _MD_Classes) -> None:
    obj = any_multidict_class()
    arg = (("a", 1),)
    obj.update(arg, b=2)
    assert list(obj.items()) == [("a", 1), ("b", 2)]
    assert arg == (("a", 1),)


def test_update_deque_arg_and_kwds(any_multidict_class: _MD_Classes) -> None:
    obj = any_multidict_class()
    arg = deque([("a", 1)])
    obj.update(arg, b=2)
    assert list(obj.items()) == [("a", 1), ("b", 2)]
    assert arg == deque([("a", 1)])


def test_update_with_second_md(any_multidict_class: _MD_Classes) -> None:
    obj1 = any_multidict_class()
    obj2 = any_multidict_class([("a", 2)])
    obj1.update(obj2)
    assert obj1 == obj2


def test_compact_after_deletion(any_multidict_class: _MD_Classes) -> None:
    # multidict is resized when it is filled up to 2/3 of the index table size
    NUM = 16 * 2 // 3
    obj = any_multidict_class((str(i), i) for i in range(NUM - 1))
    # keys.usable == 0
    # delete items, it adds empty entries but not reduce keys.usable
    for i in range(5):
        del obj[str(i)]
    # adding an entry requres keys resizing to remove empty entries
    dct = {str(i): i for i in range(100, 105)}
    obj.extend(dct)
    assert obj == {str(i): i for i in range(5, NUM - 1)} | dct


def test_update_with_empty_slots(any_multidict_class: _MD_Classes) -> None:
    # multidict is resized when it is filled up to 2/3 of the index table size
    obj = any_multidict_class([("0", 0), ("1", 1), ("1", 2)])
    del obj["0"]
    obj.update({"1": 100})
    assert obj == {"1": 100}


def test_pure_python_parse_args_size_hint_with_seq_and_kwargs() -> None:
    """Regression test for pure-Python ``_parse_args`` size hint.

    When a positional iterable and keyword arguments are both supplied,
    ``_parse_args`` previously yielded ``len(arg) + len(kwargs)`` after
    merging ``kwargs.items()`` into ``arg``. That double-counted the
    kwargs and over-allocated the internal hash table. The hint must
    equal the actual number of yielded entries.
    """
    md: PyMultiDict[int] = PyMultiDict()
    arg = [("a", 1), ("b", 2)]
    kwargs = {"c": 3, "d": 4}

    it = md._parse_args(arg, kwargs)
    size_hint = next(it)
    entries = list(it)

    assert size_hint == len(entries) == len(arg) + len(kwargs)


def test_pure_python_parse_args_size_hint_with_mapping_and_kwargs() -> None:
    """Same regression but exercising the mapping (``keys()``) branch."""
    md: PyMultiDict[int] = PyMultiDict()
    arg = {"a": 1, "b": 2}
    kwargs = {"c": 3, "d": 4}

    it = md._parse_args(arg, kwargs)
    size_hint = next(it)
    entries = list(it)

    assert size_hint == len(entries) == len(arg) + len(kwargs)


def test_pure_python_parse_args_size_hint_with_md_and_kwargs() -> None:
    """``MultiDict`` positional argument already yields the correct hint."""
    md: PyMultiDict[int] = PyMultiDict()
    arg = PyMultiDict([("a", 1), ("a", 2), ("b", 3)])
    kwargs = {"c": 4}

    it = md._parse_args(arg, kwargs)
    size_hint = next(it)
    entries = list(it)

    assert size_hint == len(entries) == len(arg) + len(kwargs)


# Code run between the items of an update() -- the argument's own iterator
# here -- finds the entries the call has doomed but not yet removed still
# whole, and what it changes is taken into account, not written over.
_READERS: dict[str, Callable[[MultiDict[object]], object]] = {
    "get": lambda d: d.get("a"),
    "getall": lambda d: d.getall("a"),
    "items": lambda d: list(d.items()),
    "len": len,
    "copy": lambda d: list(d.copy().items()),
    "popitem": lambda d: d.popitem(),
}


@pytest.mark.parametrize(
    ("reader", "seen"),
    [
        ("get", 1),
        ("getall", [1, 0]),
        ("items", [("a", 1), ("a", 0), ("b", 0)]),
        ("len", 3),
        ("copy", [("a", 1), ("a", 0), ("b", 0)]),
        ("popitem", ("b", 0)),
    ],
)
def test_update_between_items_reads(
    any_multidict_class: type[MultiDict[object]], reader: str, seen: object
) -> None:
    d = any_multidict_class([("a", 0), ("a", 0), ("b", 0)])
    got = []

    def items() -> Iterator[tuple[str, object]]:
        yield ("a", 1)
        got.append(_READERS[reader](d))
        yield ("a", 2)

    d.update(items())
    assert got == [seen]
    assert list(d.items())[:2] == [("a", 1), ("a", 2)]
    assert len(d) == len(list(d.items()))


_MUTATORS: dict[str, Callable[[MultiDict[object]], object]] = {
    "add": lambda d: d.add("x", "X"),
    "add_same": lambda d: d.add("a", "A"),
    "set_same": lambda d: d.__setitem__("a", "S"),
    "del_same": lambda d: d.__delitem__("a"),
    "clear": lambda d: d.clear(),
    "reinit": lambda d: d.__init__([("r", "R")]),  # type: ignore[misc]
    "reinit_md": lambda d: d.__init__(type(d)(r="R")),  # type: ignore[misc]
    "popitem": lambda d: d.popitem(),
    "grow": lambda d: d.extend((f"g{i}", i) for i in range(20)),
}


@pytest.mark.parametrize(
    ("mutator", "expected"),
    [
        ("add", [("a", 1), ("a", 2), ("b", 0), ("x", "X")]),
        ("add_same", [("a", 1), ("a", 2), ("b", 0)]),
        ("set_same", [("a", "S"), ("b", 0), ("a", 2)]),
        ("del_same", [("b", 0), ("a", 2)]),
        ("clear", [("a", 2)]),
        ("reinit", [("r", "R"), ("a", 2)]),
        ("reinit_md", [("r", "R"), ("a", 2)]),
        ("popitem", [("a", 1), ("a", 2)]),
        (
            "grow",
            [("a", 1), ("a", 2), ("b", 0)] + [(f"g{i}", i) for i in range(20)],
        ),
    ],
)
def test_update_between_items_mutates(
    any_multidict_class: type[MultiDict[object]],
    mutator: str,
    expected: list[tuple[str, object]],
) -> None:
    d = any_multidict_class([("a", 0), ("a", 0), ("b", 0)])

    def items() -> Iterator[tuple[str, object]]:
        yield ("a", 1)
        _MUTATORS[mutator](d)
        yield ("a", 2)

    d.update(items())
    assert list(d.items()) == expected
    assert len(d) == len(expected)


@pytest.mark.parametrize(
    ("mutator", "expected"),
    [
        ("add", [("a", 0), ("b", 1), ("x", "X"), ("b", 2)]),
        ("add_same", [("a", 0), ("b", 1), ("a", "A"), ("b", 2)]),
        ("clear", [("b", 2), ("a", 3)]),
        ("popitem", [("a", 0), ("b", 2)]),
        (
            "grow",
            [("a", 0), ("b", 1)] + [(f"g{i}", i) for i in range(20)] + [("b", 2)],
        ),
    ],
)
def test_merge_between_items_mutates(
    any_multidict_class: type[MultiDict[object]],
    mutator: str,
    expected: list[tuple[str, object]],
) -> None:
    d = any_multidict_class([("a", 0)])

    def items() -> Iterator[tuple[str, object]]:
        yield ("b", 1)
        _MUTATORS[mutator](d)
        yield ("b", 2)
        yield ("a", 3)

    d.merge(items())
    assert list(d.items()) == expected
    assert len(d) == len(expected)


def test_update_marks_outgrow_the_table(
    any_multidict_class: type[MultiDict[object]],
) -> None:
    # No length hint, so the table grows mid-call, past the room the
    # marks were built with; the first item's mark must survive that.
    d = any_multidict_class([("a", 0), ("a", 0)])
    items = [("a", 1)] + [(f"k{i}", i) for i in range(100)] + [("a", 2)]
    d.update(iter(items))
    assert list(d.items()) == [("a", 1), ("a", 2)] + items[1:-1]


@pytest.mark.parametrize("nested", ["update", "setitem", "merge", "read"])
def test_update_keeps_what_code_between_items_wrote(
    any_multidict_class: type[MultiDict[object]], nested: str
) -> None:
    # The outer call dooms the second "a"; code run between its items then
    # writes to that very entry. The write stands; a read, which can swap a
    # CIMultiDict's stored key for its istr, is no write.
    d = any_multidict_class([("a", 0), ("a", 0), ("b", 0)])
    writes: dict[str, Callable[[], object]] = {
        "update": lambda: d.update([("a", 2), ("a", 3)]),
        "setitem": lambda: d.__setitem__("a", 2),
        "merge": lambda: d.merge([("a", 2)]),
        "read": lambda: list(d.items()),
    }
    expected = {
        "update": [("a", 2), ("a", 3), ("b", 0)],
        "setitem": [("a", 2), ("b", 0)],
        "merge": [("a", 1), ("b", 0)],
        "read": [("a", 1), ("b", 0)],
    }

    def items() -> Iterator[tuple[str, object]]:
        yield ("a", 1)
        writes[nested]()

    d.update(items())
    assert list(d.items()) == expected[nested]
    assert len(d) == len(expected[nested])
