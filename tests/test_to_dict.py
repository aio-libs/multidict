"""Test to_dict functionality for all multidict types."""

from __future__ import annotations

import pytest

from multidict import CIMultiDict, CIMultiDictProxy, MultiDict, MultiDictProxy


@pytest.mark.parametrize(
    ("items", "expected"),
    (
        pytest.param([("a", 1), ("b", 2)], {"a": [1], "b": [2]}, id="unique-keys"),
        pytest.param(
            [("a", 1), ("b", 2), ("a", 3)],
            {"a": [1, 3], "b": [2]},
            id="multi-values",
        ),
    ),
)
def test_to_dict(
    any_multidict_class: type[MultiDict[str]] | type[CIMultiDict[str]],
    items: list[tuple[str, int]],
    expected: dict[str, list[int]],
) -> None:
    """Test basic to_dict conversion with unique and duplicate keys."""
    d = any_multidict_class(items)
    assert d.to_dict() == expected


def test_to_dict_empty(
    any_multidict_class: type[MultiDict[str]] | type[CIMultiDict[str]],
) -> None:
    """Test conversion of an empty multidict."""
    d = any_multidict_class()
    assert d.to_dict() == {}


def test_to_dict_returns_new_dict(
    any_multidict_class: type[MultiDict[str]] | type[CIMultiDict[str]],
) -> None:
    """Test that each call returns a new dictionary instance."""
    d = any_multidict_class([("a", "1")])
    result1 = d.to_dict()
    result2 = d.to_dict()
    assert result1 == result2
    assert result1 is not result2


def test_to_dict_list_is_fresh(
    any_multidict_class: type[MultiDict[str]] | type[CIMultiDict[str]],
) -> None:
    """Test that value lists are independent between calls."""
    d = any_multidict_class([("a", "1")])
    result1 = d.to_dict()
    result2 = d.to_dict()
    assert result1["a"] is not result2["a"]


def test_to_dict_order_preservation(
    any_multidict_class: type[MultiDict[str]] | type[CIMultiDict[str]],
) -> None:
    """Test that value lists maintain insertion order."""
    d = any_multidict_class([("x", "3"), ("x", "1"), ("x", "2")])
    assert d.to_dict()["x"] == ["3", "1", "2"]


def test_to_dict_large_data(
    any_multidict_class: type[MultiDict[str]] | type[CIMultiDict[str]],
) -> None:
    """Test to_dict with a large number of entries."""
    items = [(f"key{i % 100}", str(i)) for i in range(10000)]
    d = any_multidict_class(items)
    result = d.to_dict()
    assert len(result) == 100
    assert all(len(v) == 100 for v in result.values())


def test_to_dict_mixed_value_types(
    any_multidict_class: type[MultiDict[str]] | type[CIMultiDict[str]],
) -> None:
    """Test to_dict with mixed value types."""
    d = any_multidict_class([("a", "1"), ("a", "two"), ("b", "3.14")])
    result = d.to_dict()
    assert result["a"] == ["1", "two"]
    assert result["b"] == ["3.14"]


def test_to_dict_case_insensitive_grouping(
    case_insensitive_multidict_class: type[CIMultiDict[str]],
) -> None:
    """Test that case variants are grouped under the same key."""
    d = case_insensitive_multidict_class([("A", "1"), ("a", "2"), ("B", "3")])
    result = d.to_dict()
    assert len(result) == 2
    key_a = next(k for k in result if k.lower() == "a")
    key_b = next(k for k in result if k.lower() == "b")
    assert result[key_a] == ["1", "2"]
    assert result[key_b] == ["3"]


def test_to_dict_proxy(
    any_multidict_class: type[MultiDict[str]] | type[CIMultiDict[str]],
    any_multidict_proxy_class: type[MultiDictProxy[str]]
    | type[CIMultiDictProxy[str]],
) -> None:
    """Test to_dict works through a proxy."""
    md = any_multidict_class([("a", "1"), ("b", "2"), ("a", "3")])
    proxy = any_multidict_proxy_class(md)
    assert proxy.to_dict() == {"a": ["1", "3"], "b": ["2"]}


def test_to_dict_proxy_mutation_isolation(
    any_multidict_class: type[MultiDict[str]] | type[CIMultiDict[str]],
    any_multidict_proxy_class: type[MultiDictProxy[str]]
    | type[CIMultiDictProxy[str]],
) -> None:
    """Test that modifying returned dict does not affect the proxy."""
    md = any_multidict_class([("a", "1")])
    proxy = any_multidict_proxy_class(md)
    result = proxy.to_dict()
    result["a"].append("999")
    assert proxy.getall("a") == ["1"]


def test_to_dict_ci_proxy_case_insensitive_grouping(
    case_insensitive_multidict_class: type[CIMultiDict[str]],
    case_insensitive_multidict_proxy_class: type[CIMultiDictProxy[str]],
) -> None:
    """Test case-insensitive grouping through a proxy."""
    md = case_insensitive_multidict_class([("A", "1"), ("a", "2"), ("B", "3")])
    proxy = case_insensitive_multidict_proxy_class(md)
    result = proxy.to_dict()
    assert len(result) == 2
    key_a = next(k for k in result if k.lower() == "a")
    key_b = next(k for k in result if k.lower() == "b")
    assert result[key_a] == ["1", "2"]
    assert result[key_b] == ["3"]
