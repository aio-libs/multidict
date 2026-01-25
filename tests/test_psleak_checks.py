import pytest
import psleak
from multidict import _multidict


class MultiDictLeakTests(psleak.MemoryLeakTestCase):
    tolerance = 1024*100       # Allow 100KB
    times = 1000000        # More iterations


@pytest.mark.c_extension
class TestMultiDictLeaks(MultiDictLeakTests):
    def test_multidict_create(self):
        def worker():
            _multidict.MultiDict()
        self.execute(worker)

    def test_multidict_create_with_args(self):
        def worker():
            _multidict.MultiDict({"a": 1, "b": 2})
        self.execute(worker)

    def test_multidict_add(self):
        def worker():
            d = _multidict.MultiDict()
            d.add("key", "value")
        self.execute(worker)

    def test_multidict_pop(self):
        def worker():
            d = _multidict.MultiDict({"a": 1})
            d.pop("a")
        self.execute(worker)

    def test_multidict_pop_missing_with_default(self):
        def worker():
            d = _multidict.MultiDict()
            d.pop("missing", None)
        self.execute(worker)

    def test_multidict_pop_missing_keyerror(self):
        def worker():
            d = _multidict.MultiDict()
            try:
                d.pop("missing")
            except KeyError:
                pass
        self.execute(worker)

    def test_multidict_popall(self):
        def worker():
            d = _multidict.MultiDict([("a", 1), ("a", 2)])
            d.popall("a")
        self.execute(worker)

    def test_multidict_update_dict(self):
        def worker():
            d = _multidict.MultiDict()
            d.update({"a": 1, "b": 2})
        self.execute(worker)

    def test_multidict_extend_tuple(self):
        def worker():
            d = _multidict.MultiDict()
            d.extend([("a", 1), ("b", 2)])
        self.execute(worker)

    def test_multidict_getall(self):
        def worker():
            d = _multidict.MultiDict([("a", 1), ("a", 2)])
            d.getall("a")
        self.execute(worker)

    def test_multidict_keys(self):
        def worker():
            d = _multidict.MultiDict({"a": 1, "b": 2})
            list(d.keys())
        self.execute(worker)

    def test_multidict_values(self):
        def worker():
            d = _multidict.MultiDict({"a": 1, "b": 2})
            list(d.values())
        self.execute(worker)

    def test_multidict_items(self):
        def worker():
            d = _multidict.MultiDict({"a": 1, "b": 2})
            list(d.items())
        self.execute(worker)

    def test_multidict_clear(self):
        def worker():
            d = _multidict.MultiDict({f"key{i}": i for i in range(100)})
            d.clear()
        self.execute(worker)

    def test_multidict_copy(self):
        def worker():
            d = _multidict.MultiDict({"key": "value"})
            d.copy()
        self.execute(worker)

    def test_multidict_getone(self):
        def worker():
            d = _multidict.MultiDict([("key", "val1"), ("key", "val2")])
            d.getone("key")
        self.execute(worker)

    def test_multidict_get(self):
        def worker():
            d = _multidict.MultiDict({"key": "value"})
            d.get("key")
            d.get("missing")
            d.get("missing", "default")
        self.execute(worker)

    def test_multidict_popitem(self):
        def worker():
            d = _multidict.MultiDict({"a": 1, "b": 2})
            d.popitem()
        self.execute(worker)

    def test_multidict_setdefault(self):
        def worker():
            d = _multidict.MultiDict()
            d.setdefault("key", "default")
            d.setdefault("key", "other")
        self.execute(worker)

    def test_multidict_setitem(self):
        def worker():
            d = _multidict.MultiDict({"key": "old"})
            d["key"] = "new"
        self.execute(worker)

    def test_multidict_delitem(self):
        def worker():
            d = _multidict.MultiDict({"key": "value"})
            del d["key"]
        self.execute(worker)

    def test_multidict_stress(self):
        def worker():
            d = _multidict.MultiDict()
            for i in range(100):
                d.add(f"key{i}", f"value{i}")
            d.clear()
        self.execute(worker)

    def test_multidict_multi_value_add(self):
        def worker():
            d = _multidict.MultiDict()
            for i in range(10):
                d.add("same_key", f"value{i}")
        self.execute(worker)


@pytest.mark.c_extension
class TestCIMultiDictLeaks(MultiDictLeakTests):
    def test_cimultidict_create(self):
        def worker():
            _multidict.CIMultiDict()
        self.execute(worker)

    def test_cimultidict_create_with_args(self):
        def worker():
            _multidict.CIMultiDict({"a": 1, "b": 2})
        self.execute(worker)

    def test_cimultidict_add(self):
        def worker():
            d = _multidict.CIMultiDict()
            d.add("key", "value")
        self.execute(worker)

    def test_cimultidict_pop(self):
        def worker():
            d = _multidict.CIMultiDict({"a": 1})
            d.pop("a")
        self.execute(worker)

    def test_cimultidict_pop_missing_with_default(self):
        def worker():
            d = _multidict.CIMultiDict()
            d.pop("missing", object())
        self.execute(worker)

    def test_cimultidict_pop_missing_keyerror(self):
        def worker():
            d = _multidict.CIMultiDict()
            try:
                d.pop("missing")
            except KeyError:
                pass
        self.execute(worker)

    def test_cimultidict_popall(self):
        def worker():
            d = _multidict.CIMultiDict([("a", 1), ("a", 2)])
            d.popall("a")
        self.execute(worker)

    def test_cimultidict_clear(self):
        def worker():
            d = _multidict.CIMultiDict({f"key{i}": i for i in range(100)})
            d.clear()
        self.execute(worker)

    def test_cimultidict_copy(self):
        def worker():
            d = _multidict.CIMultiDict({"key": "value"})
            d.copy()
        self.execute(worker)

    def test_cimultidict_getone(self):
        def worker():
            d = _multidict.CIMultiDict([("key", "val1"), ("key", "val2")])
            d.getone("key")
        self.execute(worker)

    def test_cimultidict_get(self):
        def worker():
            d = _multidict.CIMultiDict({"key": "value"})
            d.get("key")
            d.get("missing", "default")
        self.execute(worker)

    def test_cimultidict_setitem(self):
        def worker():
            d = _multidict.CIMultiDict({"key": "old"})
            d["key"] = "new"
        self.execute(worker)

    def test_cimultidict_delitem(self):
        def worker():
            d = _multidict.CIMultiDict({"key": "value"})
            del d["key"]
        self.execute(worker)

    def test_cimultidict_case_insensitive_add_pop(self):
        def worker():
            d = _multidict.CIMultiDict()
            d.add("Key", "value1")
            d.add("KEY", "value2")
            d.pop("key")
        self.execute(worker)

    def test_cimultidict_update(self):
        def worker():
            d = _multidict.CIMultiDict()
            d.update({"a": 1, "b": 2})
        self.execute(worker)

    def test_cimultidict_extend(self):
        def worker():
            d = _multidict.CIMultiDict()
            d.extend([("a", 1), ("b", 2)])
        self.execute(worker)

    def test_cimultidict_getall(self):
        def worker():
            d = _multidict.CIMultiDict([("a", 1), ("a", 2)])
            d.getall("a")
        self.execute(worker)

    def test_cimultidict_keys(self):
        def worker():
            d = _multidict.CIMultiDict({"a": 1, "b": 2})
            list(d.keys())
        self.execute(worker)

    def test_cimultidict_values(self):
        def worker():
            d = _multidict.CIMultiDict({"a": 1, "b": 2})
            list(d.values())
        self.execute(worker)

    def test_cimultidict_items(self):
        def worker():
            d = _multidict.CIMultiDict({"a": 1, "b": 2})
            list(d.items())
        self.execute(worker)


@pytest.mark.c_extension
class TestMultiDictProxyLeaks(MultiDictLeakTests):
    def test_proxy_create(self):
        d = _multidict.MultiDict({"a": 1})
        def worker():
            _multidict.MultiDictProxy(d)
        self.execute(worker)

    def test_proxy_access(self):
        d = _multidict.MultiDict({"a": 1})
        p = _multidict.MultiDictProxy(d)
        def worker():
            _ = p["a"]
        self.execute(worker)

    def test_proxy_getall(self):
        d = _multidict.MultiDict([("a", 1), ("a", 2)])
        p = _multidict.MultiDictProxy(d)
        def worker():
            p.getall("a")
        self.execute(worker)

    def test_proxy_getone(self):
        d = _multidict.MultiDict([("key", "val1"), ("key", "val2")])
        p = _multidict.MultiDictProxy(d)
        def worker():
            p.getone("key")
        self.execute(worker)

    def test_proxy_get(self):
        d = _multidict.MultiDict({"key": "value"})
        p = _multidict.MultiDictProxy(d)
        def worker():
            p.get("key")
            p.get("missing", "default")
        self.execute(worker)

    def test_proxy_keys(self):
        d = _multidict.MultiDict({"a": 1, "b": 2})
        p = _multidict.MultiDictProxy(d)
        def worker():
            list(p.keys())
        self.execute(worker)

    def test_proxy_values(self):
        d = _multidict.MultiDict({"a": 1, "b": 2})
        p = _multidict.MultiDictProxy(d)
        def worker():
            list(p.values())
        self.execute(worker)

    def test_proxy_items(self):
        d = _multidict.MultiDict({"a": 1, "b": 2})
        p = _multidict.MultiDictProxy(d)
        def worker():
            list(p.items())
        self.execute(worker)

    def test_proxy_copy(self):
        d = _multidict.MultiDict({"key": "value"})
        p = _multidict.MultiDictProxy(d)
        def worker():
            p.copy()
        self.execute(worker)

    def test_proxy_after_modify(self):
        d = _multidict.MultiDict()
        p = _multidict.MultiDictProxy(d)
        def worker():
            d.add("key", "value")
            _ = p["key"]
            d.clear()
        self.execute(worker)


@pytest.mark.c_extension
class TestCIMultiDictProxyLeaks(MultiDictLeakTests):
    def test_proxy_create(self):
        d = _multidict.CIMultiDict({"a": 1})
        def worker():
            _multidict.CIMultiDictProxy(d)
        self.execute(worker)

    def test_proxy_access(self):
        d = _multidict.CIMultiDict({"a": 1})
        p = _multidict.CIMultiDictProxy(d)
        def worker():
            _ = p["a"]
        self.execute(worker)

    def test_proxy_getone(self):
        d = _multidict.CIMultiDict([("key", "val1"), ("key", "val2")])
        p = _multidict.CIMultiDictProxy(d)
        def worker():
            p.getone("key")
        self.execute(worker)

    def test_proxy_get(self):
        d = _multidict.CIMultiDict({"key": "value"})
        p = _multidict.CIMultiDictProxy(d)
        def worker():
            p.get("KEY")
            p.get("missing", "default")
        self.execute(worker)

    def test_proxy_keys(self):
        d = _multidict.CIMultiDict({"a": 1, "b": 2})
        p = _multidict.CIMultiDictProxy(d)
        def worker():
            list(p.keys())
        self.execute(worker)

    def test_proxy_values(self):
        d = _multidict.CIMultiDict({"a": 1, "b": 2})
        p = _multidict.CIMultiDictProxy(d)
        def worker():
            list(p.values())
        self.execute(worker)

    def test_proxy_items(self):
        d = _multidict.CIMultiDict({"a": 1, "b": 2})
        p = _multidict.CIMultiDictProxy(d)
        def worker():
            list(p.items())
        self.execute(worker)

    def test_proxy_copy(self):
        d = _multidict.CIMultiDict({"key": "value"})
        p = _multidict.CIMultiDictProxy(d)
        def worker():
            p.copy()
        self.execute(worker)


@pytest.mark.c_extension
class TestIstrLeaks(MultiDictLeakTests):
    def test_istr_create(self):
        def worker():
            _multidict.istr("Hello")
        self.execute(worker)

    def test_istr_comparison(self):
        def worker():
            s1 = _multidict.istr("Hello")
            s2 = _multidict.istr("hello")
            _ = s1 == s2
        self.execute(worker)

    def test_istr_lower(self):
        def worker():
            s = _multidict.istr("Hello")
            s.lower()
        self.execute(worker)

    def test_istr_upper(self):
        def worker():
            s = _multidict.istr("Hello")
            s.upper()
        self.execute(worker)

    def test_istr_in_dict(self):
        def worker():
            d = {_multidict.istr("key"): "value"}
            _ = d[_multidict.istr("key")]
        self.execute(worker)

    def test_istr_concatenation(self):
        def worker():
            s1 = _multidict.istr("Hello")
            s2 = _multidict.istr("World")
            _ = s1 + " " + s2
        self.execute(worker)

    def test_istr_hash(self):
        def worker():
            s = _multidict.istr("Hello")
            hash(s)
        self.execute(worker)
