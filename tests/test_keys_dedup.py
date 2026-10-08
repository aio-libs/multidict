"""keys() and iter() yield each key once, under its first-seen spelling."""

import pytest

from multidict import CIMultiDict, MultiDict, MultiDictProxy

_MD_Classes = type[MultiDict[str]] | type[CIMultiDict[str]]


def test_iter_keys_and_len(any_multidict_class: _MD_Classes) -> None:
    d = any_multidict_class([("a", "1"), ("b", "2"), ("a", "3"), ("a", "4")])
    assert list(d) == ["a", "b"]
    assert list(d.keys()) == ["a", "b"]
    assert list(reversed(d.keys())) == ["b", "a"]  # type: ignore[call-overload]
    assert len(d.keys()) == 2
    assert repr(d.keys()) == "<_KeysView('a', 'b')>"


def test_items_values_len_unchanged(any_multidict_class: _MD_Classes) -> None:
    d = any_multidict_class([("a", "1"), ("b", "2"), ("a", "3")])
    assert len(d) == 3
    assert list(d.items()) == [("a", "1"), ("b", "2"), ("a", "3")]
    assert list(d.values()) == ["1", "2", "3"]
    assert len(d.items()) == len(d.values()) == 3


def test_empty(any_multidict_class: _MD_Classes) -> None:
    d = any_multidict_class()
    assert list(d) == []
    assert len(d.keys()) == 0
    assert list(reversed(d.keys())) == []  # type: ignore[call-overload]


def test_ci_first_spelling(
    case_insensitive_multidict_class: type[CIMultiDict[str]],
) -> None:
    d = case_insensitive_multidict_class(
        [("X-Foo", "1"), ("x-foo", "2"), ("B", "3"), ("X-FOO", "4")]
    )
    assert list(d) == ["X-Foo", "B"]
    assert list(reversed(d.keys())) == ["B", "X-Foo"]  # type: ignore[call-overload]
    assert len(d.keys()) == 2
    assert repr(d.keys()) == "<_KeysView('X-Foo', 'B')>"
    assert dict(d) == {"X-Foo": "1", "B": "3"}
    assert {k: d.getall(k) for k in d} == {"X-Foo": ["1", "2", "4"], "B": ["3"]}


def test_ci_istr_keys(
    case_insensitive_multidict_class: type[CIMultiDict[str]],
    case_insensitive_str_class: type[str],
) -> None:
    d = case_insensitive_multidict_class(
        [(case_insensitive_str_class("Foo"), "1"), ("FOO", "2")]
    )
    assert list(d) == ["Foo"]


def test_next_spelling_after_removing_first(
    case_insensitive_multidict_class: type[CIMultiDict[str]],
) -> None:
    d = case_insensitive_multidict_class([("X-Foo", "1"), ("B", "2"), ("x-foo", "3")])
    assert d.popone("X-FOO") == "1"
    assert list(d) == ["B", "x-foo"]


def test_readded_key_keeps_earlier_spelling(
    case_insensitive_multidict_class: type[CIMultiDict[str]],
) -> None:
    d = case_insensitive_multidict_class([("A", "1"), ("b", "2")])
    d.add("a", "3")
    del d["b"]
    d.add("B", "4")
    assert list(d) == ["A", "B"]


def test_replaced_first_entry_spelling(
    case_insensitive_multidict_class: type[CIMultiDict[str]],
) -> None:
    d = case_insensitive_multidict_class([("X-Foo", "1"), ("x-foo", "2")])
    d["X-FOO"] = "3"
    assert list(d) == ["X-FOO"]


def test_many_duplicates_across_resizes(any_multidict_class: _MD_Classes) -> None:
    d = any_multidict_class()
    for i in range(1000):
        d.add(f"k{i % 37}", str(i))
    assert list(d) == [f"k{i}" for i in range(37)]
    assert len(d.keys()) == 37
    for i in range(0, 37, 2):
        d.popone(f"k{i}")
    # the second entry of every even key moved up, past all the odd keys
    assert list(d) == [f"k{i}" for i in range(1, 37, 2)] + [
        f"k{i}" for i in range(0, 37, 2)
    ]
    assert list(reversed(d.keys())) == list(reversed(list(d)))  # type: ignore[call-overload]


def test_after_deletes_and_rebuild(any_multidict_class: _MD_Classes) -> None:
    d = any_multidict_class((f"k{i}", "x") for i in range(100))
    for i in range(100):
        d.add(f"k{i}", "y")
    for i in range(0, 100, 3):
        del d[f"k{i}"]
    d.update({f"k{i}": "z" for i in range(0, 100, 3)})
    expected = [f"k{i}" for i in range(100) if i % 3] + [
        f"k{i}" for i in range(0, 100, 3)
    ]
    assert list(d) == expected
    assert len(d.keys()) == 100


def test_proxy(
    any_multidict_class: _MD_Classes,
    any_multidict_proxy_class: type[MultiDictProxy[str]],
) -> None:
    p = any_multidict_proxy_class(any_multidict_class([("a", "1"), ("a", "2")]))
    assert list(p) == ["a"]
    assert list(p.keys()) == ["a"]
    assert len(p.keys()) == 1


def test_length_hint_is_upper_bound(any_multidict_class: _MD_Classes) -> None:
    d = any_multidict_class([("a", "1"), ("a", "2")])
    it = iter(d.keys())
    assert it.__length_hint__() >= 1  # type: ignore[attr-defined]


def test_mutation_while_iterating_raises(any_multidict_class: _MD_Classes) -> None:
    d = any_multidict_class([("a", "1"), ("a", "2"), ("b", "3")])
    it = iter(d)
    assert next(it) == "a"
    d.add("c", "4")
    with pytest.raises(RuntimeError):
        next(it)


def test_ci_set_operations(
    case_insensitive_multidict_class: type[CIMultiDict[str]],
) -> None:
    d = case_insensitive_multidict_class([("X-Foo", "1"), ("x-foo", "2"), ("B", "3")])
    k = d.keys()
    assert k - {"x-foo"} == {"B"}
    assert k & {"x-foo"} == {"X-Foo"}
    assert k ^ {"x-foo"} == {"B"}
    assert k | {"X-FOO"} == {"X-Foo", "B"}
    assert {"z"} | k == {"z", "X-Foo", "B"}
    assert {"x-foo", "z"} - k == {"z"}
    assert {"x-foo", "z"} & k == {"x-foo"}
    assert {"x-foo"} ^ k == {"B"}
    assert k == {"X-Foo", "B"}
    assert k != {"X-Foo", "x-foo", "B"}
    assert k <= {"X-Foo", "B", "z"}
    assert k < {"X-Foo", "B", "z"}
    assert k >= {"X-FOO"}
    assert k > {"x-foo"}
    assert not k.isdisjoint({"X-FOO"})


def test_cs_set_operations(
    case_sensitive_multidict_class: type[MultiDict[str]],
) -> None:
    d = case_sensitive_multidict_class([("a", "1"), ("a", "2"), ("b", "3")])
    k = d.keys()
    assert k == {"a", "b"}
    assert k - {"a"} == {"b"}
    assert k ^ {"a", "c"} == {"b", "c"}
    assert {"c"} | k == {"a", "b", "c"}


def test_update_appending_duplicates(any_multidict_class: _MD_Classes) -> None:
    d = any_multidict_class([("a", "1"), ("b", "2")])
    d.update([("a", "3"), ("a", "4"), ("b", "5"), ("b", "6")])
    assert list(d.items()) == [("a", "3"), ("b", "5"), ("a", "4"), ("b", "6")]
    assert list(d) == ["a", "b"]
    assert len(d.keys()) == 2


def test_merge_and_extend(any_multidict_class: _MD_Classes) -> None:
    d = any_multidict_class([("a", "1")])
    d.extend(any_multidict_class([("a", "2"), ("b", "3"), ("b", "4")]))
    d.merge([("c", "5"), ("c", "6")])
    assert list(d) == ["a", "b", "c"]


def test_copies_keep_dedup(any_multidict_class: _MD_Classes) -> None:
    src = any_multidict_class([("a", "1"), ("b", "2"), ("a", "3")])
    copies = [src.copy(), any_multidict_class(src), any_multidict_class()]
    copies[2].__init__(src)  # type: ignore[misc]
    for d in copies:
        assert list(d) == ["a", "b"]
        assert len(d.keys()) == 2


def test_cross_type_copy(
    case_sensitive_multidict_class: type[MultiDict[str]],
    case_insensitive_multidict_class: type[CIMultiDict[str]],
) -> None:
    cs = case_sensitive_multidict_class([("A", "1"), ("a", "2"), ("A", "3")])
    assert list(cs) == ["A", "a"]
    ci = case_insensitive_multidict_class(cs)
    assert list(ci) == ["A"]
    assert list(case_sensitive_multidict_class(ci)) == ["A", "a"]


def test_deleting_duplicates_then_rebuilding(any_multidict_class: _MD_Classes) -> None:
    d = any_multidict_class([("a", "1"), ("a", "2"), ("b", "3")])
    d.popone("a")
    d.extend((f"k{i}", "x") for i in range(50))
    assert list(d)[:2] == ["a", "b"]
    assert len(d.keys()) == 52


def test_removed_slots_then_duplicate(any_multidict_class: _MD_Classes) -> None:
    d = any_multidict_class((f"k{i}", str(i)) for i in range(20))
    for i in range(0, 20, 2):
        del d[f"k{i}"]
    # a repeat added after the deletions must still sort after its first
    # entry
    for i in range(0, 20, 2):
        d[f"n{i}"] = "a"
        d.setdefault(f"s{i}", "b")
        d.add(f"n{i}", "c")
        d.add(f"s{i}", "d")
    for i in range(0, 20, 2):
        assert d.getall(f"n{i}") == ["a", "c"]
        assert d.getall(f"s{i}") == ["b", "d"]
        assert d[f"n{i}"] == "a"
    assert len(d.keys()) == len(set(d.keys())) == 30


def test_duplicates_removed_then_rebuilt(any_multidict_class: _MD_Classes) -> None:
    d = any_multidict_class([("a", "1"), ("a", "2"), ("b", "3")])
    d["a"] = "4"
    d.extend((f"k{i}", "x") for i in range(100))  # rebuilds the table
    assert list(d)[:2] == ["a", "b"]
    d.add("b", "5")
    assert list(d)[:2] == ["a", "b"]
    assert len(d.keys()) == 102
    assert d.getall("b") == ["3", "5"]


def test_getall_popall_to_dict(any_multidict_class: _MD_Classes) -> None:
    d = any_multidict_class([("a", "1"), ("b", "2")])
    assert d.getall("a") == ["1"]
    assert d.getall("z", None) is None
    assert d.to_dict() == {"a": ["1"], "b": ["2"]}
    assert d.popall("a") == ["1"]
    d.add("b", "3")
    assert d.getall("b") == ["2", "3"]
    assert d.to_dict() == {"b": ["2", "3"]}
    assert d.popall("b") == ["2", "3"]
    assert not d


def test_extend_from_multidict_with_duplicates(
    any_multidict_class: _MD_Classes,
) -> None:
    src = any_multidict_class([("a", "1"), ("b", "2"), ("a", "3")])
    for d in (any_multidict_class(), any_multidict_class([("c", "0")])):
        d.extend(src)
        assert list(d)[-2:] == ["a", "b"]
        d.extend(src)
        assert d.getall("a") == ["1", "3", "1", "3"]
    clean = any_multidict_class([("a", "1"), ("b", "2")])
    d = any_multidict_class()
    d.extend(clean)
    d.add("a", "x")
    assert list(d) == ["a", "b"]
    assert d.getall("a") == ["1", "x"]


def test_extend_self(any_multidict_class: _MD_Classes) -> None:
    d = any_multidict_class([("a", "1"), ("b", "2")])
    d.extend(d)
    assert list(d) == ["a", "b"]
    assert d.getall("a") == ["1", "1"]


def test_empty_multidict_from_dict_and_kwargs(any_multidict_class: _MD_Classes) -> None:
    d = any_multidict_class({"a": "1", "b": "2"})
    d.add("a", "3")
    assert list(d) == ["a", "b"]
    d = any_multidict_class(a="1", b="2")
    d.add("b", "3")
    assert list(d) == ["a", "b"]
    assert d.getall("b") == ["2", "3"]


def test_ci_from_dict_and_kwargs_with_spellings(
    case_insensitive_multidict_class: type[CIMultiDict[str]],
) -> None:
    d = case_insensitive_multidict_class({"A": "1", "a": "2"})
    assert list(d) == ["A"]
    d = case_insensitive_multidict_class(A="1", a="2")
    assert list(d) == ["A"]
    d = case_insensitive_multidict_class()
    d.extend(case_insensitive_multidict_class([("A", "1")]))
    d.extend({"a": "2"})
    assert list(d) == ["A"]
    assert d.getall("a") == ["1", "2"]


def test_update_and_merge_in_one_batch(any_multidict_class: _MD_Classes) -> None:
    d = any_multidict_class([("a", "1")])
    d.merge([("m", "1"), ("m", "2")])
    assert d.getall("m") == ["1", "2"]
    assert list(d) == ["a", "m"]
    d = any_multidict_class((f"k{i}", "x") for i in range(10))
    for i in range(0, 10, 2):
        del d[f"k{i}"]
    d.update([("u", "1"), ("u", "2"), ("k1", "y")])
    assert d.getall("u") == ["1", "2"]
    assert d["k1"] == "y"
    assert list(d).count("u") == 1


def test_merge_keeps_present_keys(
    any_multidict_class: _MD_Classes, any_multidict_class_name: str
) -> None:
    d = any_multidict_class([("a", "1"), ("b", "2")])
    d.merge(any_multidict_class([("a", "x"), ("b", "y"), ("c", "3")]))
    assert list(d.items()) == [("a", "1"), ("b", "2"), ("c", "3")]
    d.merge({"A": "z"})
    assert len(d) == (3 if any_multidict_class_name == "CIMultiDict" else 4)


def test_update_repeating_a_key_in_one_batch(any_multidict_class: _MD_Classes) -> None:
    d = any_multidict_class([("a", "1")])
    d.update([("a", "2"), ("a", "3"), ("a", "4")])
    assert d.getall("a") == ["2", "3", "4"]
    assert list(d) == ["a"]
    assert len(d.keys()) == 1


class _OtherHashStr(str):
    def __hash__(self) -> int:
        return 12345


@pytest.mark.parametrize("subclass_first", [False, True])
def test_from_dict_and_kwargs_with_an_equal_str_subclass_key(
    any_multidict_class: _MD_Classes, subclass_first: bool
) -> None:
    pairs = [("a", "1"), (_OtherHashStr("a"), "2")]
    if subclass_first:
        pairs.reverse()
    src = dict(pairs)
    assert len(src) == 2
    extended = any_multidict_class()
    extended.extend(src)
    for d in (any_multidict_class(src), any_multidict_class(**src), extended):
        assert [str(k) for k in d] == ["a"]
        assert len(d.keys()) == 1
        assert d.getall("a") == [v for _, v in pairs]
