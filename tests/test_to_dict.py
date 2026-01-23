"""Test to_dict functionality for all multidict types."""
import pytest


class BaseToDictTests:

    def test_to_dict_simple(self, cls):
        d = cls([("a", 1), ("b", 2)])
        result = d.to_dict()
        assert result == {"a": [1], "b": [2]}

    def test_to_dict_multi_values(self, cls):
        d = cls([("a", 1), ("b", 2), ("a", 3)])
        result = d.to_dict()
        assert result == {"a": [1, 3], "b": [2]}

    def test_to_dict_empty(self, cls):
        d = cls()
        result = d.to_dict()
        assert result == {}

    def test_to_dict_returns_new_dict(self, cls):
        d = cls([("a", 1)])
        result1 = d.to_dict()
        result2 = d.to_dict()
        assert result1 == result2
        assert result1 is not result2

    def test_to_dict_list_is_fresh(self, cls):
        d = cls([("a", 1)])
        result1 = d.to_dict()
        result2 = d.to_dict()
        assert result1["a"] is not result2["a"]


class TestMultiDictToDict(BaseToDictTests):

    @pytest.fixture
    def cls(self, multidict_module):
        return multidict_module.MultiDict


class TestCIMultiDictToDict(BaseToDictTests):

    @pytest.fixture
    def cls(self, multidict_module):
        return multidict_module.CIMultiDict

    def test_to_dict_case_insensitive_grouping(self, cls):
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

    @pytest.fixture
    def cls(self, multidict_module):
        def make_proxy(*args, **kwargs):
            md = multidict_module.MultiDict(*args, **kwargs)
            return multidict_module.MultiDictProxy(md)
        return make_proxy


class TestCIMultiDictProxyToDict(BaseToDictTests):

    @pytest.fixture
    def cls(self, multidict_module):
        def make_proxy(*args, **kwargs):
            md = multidict_module.CIMultiDict(*args, **kwargs)
            return multidict_module.CIMultiDictProxy(md)
        return make_proxy

    def test_to_dict_case_insensitive_grouping(self, cls):
        d = cls([("A", 1), ("a", 2), ("B", 3)])
        result = d.to_dict()
        assert len(result) == 2
