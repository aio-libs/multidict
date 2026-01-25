"""Test to_dict functionality for all multidict types."""
import pytest


class BaseToDictTests:
    """Base tests for to_dict() method, inherited by all multidict type tests."""

    def test_to_dict_simple(self, cls):
        """Test basic conversion with unique keys."""
        d = cls([("a", 1), ("b", 2)])
        result = d.to_dict()
        assert result == {"a": [1], "b": [2]}

    def test_to_dict_multi_values(self, cls):
        """Test grouping multiple values under the same key."""
        d = cls([("a", 1), ("b", 2), ("a", 3)])
        result = d.to_dict()
        assert result == {"a": [1, 3], "b": [2]}

    def test_to_dict_empty(self, cls):
        """Test conversion of an empty multidict."""
        d = cls()
        result = d.to_dict()
        assert result == {}

    def test_to_dict_returns_new_dict(self, cls):
        """Test that each call returns a new dictionary instance."""
        d = cls([("a", 1)])
        result1 = d.to_dict()
        result2 = d.to_dict()
        assert result1 == result2
        assert result1 is not result2

    def test_to_dict_list_is_fresh(self, cls):
        """Test that value lists are independent between calls."""
        d = cls([("a", 1)])
        result1 = d.to_dict()
        result2 = d.to_dict()
        assert result1["a"] is not result2["a"]

    def test_to_dict_order_preservation(self, cls):
        """Test that value lists maintain insertion order."""
        d = cls([("x", 3), ("x", 1), ("x", 2)])
        result = d.to_dict()
        assert result["x"] == [3, 1, 2]

    def test_to_dict_large_data(self, cls):
        """Test to_dict with a large number of entries for performance."""
        items = [(f"key{i % 100}", i) for i in range(10000)]
        d = cls(items)
        result = d.to_dict()
        assert len(result) == 100
        assert all(len(v) == 100 for v in result.values())

    def test_to_dict_mixed_value_types(self, cls):
        """Test to_dict with mixed value types (str, int) to verify generic _V."""
        d = cls([("a", 1), ("a", "two"), ("b", 3.14)])
        result = d.to_dict()
        assert result["a"] == [1, "two"]
        assert result["b"] == [3.14]


class TestMultiDictToDict(BaseToDictTests):
    """Tests for MultiDict.to_dict()."""

    @pytest.fixture
    def cls(self, multidict_module):
        return multidict_module.MultiDict


class TestCIMultiDictToDict(BaseToDictTests):
    """Tests for CIMultiDict.to_dict()."""

    @pytest.fixture
    def cls(self, multidict_module):
        return multidict_module.CIMultiDict

    def test_to_dict_case_insensitive_grouping(self, cls):
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
    def cls(self, multidict_module):
        def make_proxy(*args, **kwargs):
            md = multidict_module.MultiDict(*args, **kwargs)
            return multidict_module.MultiDictProxy(md)
        return make_proxy

    def test_to_dict_proxy_mutation_isolation(self, cls, multidict_module):
        """Test that modifying returned dict does not affect the proxy."""
        md = multidict_module.MultiDict([("a", 1)])
        proxy = multidict_module.MultiDictProxy(md)
        result = proxy.to_dict()
        result["a"].append(999)
        assert proxy.getall("a") == [1]


class TestCIMultiDictProxyToDict(BaseToDictTests):
    """Tests for CIMultiDictProxy.to_dict()."""

    @pytest.fixture
    def cls(self, multidict_module):
        def make_proxy(*args, **kwargs):
            md = multidict_module.CIMultiDict(*args, **kwargs)
            return multidict_module.CIMultiDictProxy(md)
        return make_proxy

    def test_to_dict_case_insensitive_grouping(self, cls):
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

    def test_to_dict_proxy_mutation_isolation(self, cls, multidict_module):
        """Test that modifying returned dict does not affect the proxy."""
        md = multidict_module.CIMultiDict([("a", 1)])
        proxy = multidict_module.CIMultiDictProxy(md)
        result = proxy.to_dict()
        result["a"].append(999)
        assert proxy.getall("a") == [1]

