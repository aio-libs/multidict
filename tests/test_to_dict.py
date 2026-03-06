"""Test to_dict functionality for all multidict types."""

from collections.abc import Iterable
from typing import Protocol, Type

import pytest

from multidict import (
    CIMultiDict,
    CIMultiDictProxy,
    MultiDict,
    MultiDictProxy,
    MultiMapping,
)


class MultidictModule(Protocol):
    MultiDict: Type[MultiDict[object]]
    CIMultiDict: Type[CIMultiDict[object]]
    MultiDictProxy: Type[MultiDictProxy[object]]
    CIMultiDictProxy: Type[CIMultiDictProxy[object]]


class DictFactory(Protocol):
    def __call__(
        self, arg: Iterable[tuple[str, object]] | None = None
    ) -> MultiMapping[object]:
        raise NotImplementedError


class BaseToDictTests:
    """Base tests for to_dict() method, inherited by all multidict type tests."""

    def test_to_dict_simple(self, cls: DictFactory) -> None:
        """Test basic conversion with unique keys."""
        d = cls([("a", 1), ("b", 2)])
        result = d.to_dict()
        assert result == {"a": [1], "b": [2]}

    def test_to_dict_multi_values(self, cls: DictFactory) -> None:
        """Test grouping multiple values under the same key."""
        d = cls([("a", 1), ("b", 2), ("a", 3)])
        result = d.to_dict()
        assert result == {"a": [1, 3], "b": [2]}

    def test_to_dict_empty(self, cls: DictFactory) -> None:
        """Test conversion of an empty multidict."""
        d = cls()
        result = d.to_dict()
        assert result == {}

    def test_to_dict_returns_new_dict(self, cls: DictFactory) -> None:
        """Test that each call returns a new dictionary instance."""
        d = cls([("a", 1)])
        result1 = d.to_dict()
        result2 = d.to_dict()
        assert result1 == result2
        assert result1 is not result2

    def test_to_dict_list_is_fresh(self, cls: DictFactory) -> None:
        """Test that value lists are independent between calls."""
        d = cls([("a", 1)])
        result1 = d.to_dict()
        result2 = d.to_dict()
        assert result1["a"] is not result2["a"]

    def test_to_dict_order_preservation(self, cls: DictFactory) -> None:
        """Test that value lists maintain insertion order."""
        d = cls([("x", 3), ("x", 1), ("x", 2)])
        result = d.to_dict()
        assert result["x"] == [3, 1, 2]

    def test_to_dict_large_data(self, cls: DictFactory) -> None:
        """Test to_dict with a large number of entries for performance."""
        items = [(f"key{i % 100}", i) for i in range(10000)]
        d = cls(items)
        result = d.to_dict()
        assert len(result) == 100
        assert all(len(v) == 100 for v in result.values())

    def test_to_dict_mixed_value_types(self, cls: DictFactory) -> None:
        """Test to_dict with mixed value types (str, int) to verify generic _V."""
        d = cls([("a", 1), ("a", "two"), ("b", 3.14)])
        result = d.to_dict()
        assert result["a"] == [1, "two"]
        assert result["b"] == [3.14]


class TestMultiDictToDict(BaseToDictTests):
    """Tests for MultiDict.to_dict()."""

    @pytest.fixture
    def cls(self, multidict_module: MultidictModule) -> Type[MultiDict[object]]:
        return multidict_module.MultiDict


class TestCIMultiDictToDict(BaseToDictTests):
    """Tests for CIMultiDict.to_dict()."""

    @pytest.fixture
    def cls(self, multidict_module: MultidictModule) -> Type[CIMultiDict[object]]:
        return multidict_module.CIMultiDict

    def test_to_dict_case_insensitive_grouping(self, cls: DictFactory) -> None:
        """Test that case variants are grouped under the same key."""
        d = cls([("A", 1), ("a", 2), ("B", 3)])
        result = d.to_dict()
        assert len(result) == 2
        assert "A" in result or "a" in result
        assert "B" in result or "b" in result
        key_a = "A" if "A" in result else "a"
        key_b = "B" if "B" in result else "b"
        assert result[key_a] == [1, 2]
        assert result[key_b] == [3]


class TestMultiDictProxyToDict(BaseToDictTests):
    """Tests for MultiDictProxy.to_dict()."""

    @pytest.fixture
    def cls(self, multidict_module: MultidictModule) -> DictFactory:
        def make_proxy(
            arg: Iterable[tuple[str, object]] | None = None,
        ) -> MultiMapping[object]:
            md: MultiDict[object] = (
                multidict_module.MultiDict(arg) if arg else multidict_module.MultiDict()
            )
            return multidict_module.MultiDictProxy(md)

        return make_proxy

    def test_to_dict_proxy_mutation_isolation(
        self, cls: DictFactory, multidict_module: MultidictModule
    ) -> None:
        """Test that modifying returned dict does not affect the proxy."""
        md: MultiDict[object] = multidict_module.MultiDict([("a", 1)])
        proxy: MultiMapping[object] = multidict_module.MultiDictProxy(md)
        result = proxy.to_dict()
        result["a"].append(999)
        assert proxy.getall("a") == [1]


class TestCIMultiDictProxyToDict(BaseToDictTests):
    """Tests for CIMultiDictProxy.to_dict()."""

    @pytest.fixture
    def cls(self, multidict_module: MultidictModule) -> DictFactory:
        def make_proxy(
            arg: Iterable[tuple[str, object]] | None = None,
        ) -> MultiMapping[object]:
            md: CIMultiDict[object] = (
                multidict_module.CIMultiDict(arg)
                if arg
                else multidict_module.CIMultiDict()
            )
            return multidict_module.CIMultiDictProxy(md)

        return make_proxy

    def test_to_dict_case_insensitive_grouping(self, cls: DictFactory) -> None:
        """Test that case variants are grouped under the same key."""
        d = cls([("A", 1), ("a", 2), ("B", 3)])
        result = d.to_dict()
        assert len(result) == 2
        assert "A" in result or "a" in result
        assert "B" in result or "b" in result
        key_a = "A" if "A" in result else "a"
        key_b = "B" if "B" in result else "b"
        assert result[key_a] == [1, 2]
        assert result[key_b] == [3]

    def test_to_dict_proxy_mutation_isolation(
        self, cls: DictFactory, multidict_module: MultidictModule
    ) -> None:
        """Test that modifying returned dict does not affect the proxy."""
        md: CIMultiDict[object] = multidict_module.CIMultiDict([("a", 1)])
        proxy: MultiMapping[object] = multidict_module.CIMultiDictProxy(md)
        result = proxy.to_dict()
        result["a"].append(999)
        assert proxy.getall("a") == [1]
