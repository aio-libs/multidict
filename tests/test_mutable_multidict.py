import string
import sys
from typing import Union

import pytest

from multidict import (
    CIMultiDict,
    CIMultiDictProxy,
    MultiDict,
    MultiDictProxy,
    istr,
)


class TestMutableMultiDict:
    def test_copy(
        self,
        md_class: type[MultiDict[str]],
    ) -> None:
        d1 = md_class(key="value", a="b")

        d2 = d1.copy()
        assert d1 == d2
        assert d1 is not d2

    def test__repr__(
        self,
        md_class: type[MultiDict[str]],
    ) -> None:
        d = md_class()
        assert str(d) == "<%s()>" % md_class.__name__

        d = md_class([("key", "one"), ("key", "two")])

        expected = (
            f"<{md_class.__name__}"
            "('key': 'one', 'key': 'two')>"
        )
        assert str(d) == expected

    def test_getall(
        self,
        md_class: type[MultiDict[str]],
    ) -> None:
        d = md_class([("key", "value1")], key="value2")
        assert len(d) == 2

        assert d.getall("key") == ["value1", "value2"]

        with pytest.raises(KeyError, match="some_key"):
            d.getall("some_key")

        default = object()
        assert d.getall("some_key", default) is default

    def test_add(
        self,
        md_class: type[MultiDict[str]],
    ) -> None:
        d = md_class()

        assert d == {}
        d["key"] = "one"
        assert d == {"key": "one"}
        assert d.getall("key") == ["one"]

        d["key"] = "two"
        assert d == {"key": "two"}
        assert d.getall("key") == ["two"]

        d.add("key", "one")
        assert 2 == len(d)
        assert d.getall("key") == ["two", "one"]

        d.add("foo", "bar")
        assert 3 == len(d)
        assert d.getall("foo") == ["bar"]

    def test_extend(
        self,
        md_class: type[MultiDict[Union[str, int]]],
    ) -> None:
        d = md_class()
        assert d == {}

        d.extend([("key", "one"), ("key", "two")], key=3, foo="bar")
        assert d != {"key": "one", "foo": "bar"}
        assert 4 == len(d)
        itms = d.items()
        # we can't guarantee order of kwargs
        assert ("key", "one") in itms
        assert ("key", "two") in itms
        assert ("key", 3) in itms
        assert ("foo", "bar") in itms

        other = md_class(bar="baz")
        assert other == {"bar": "baz"}

        d.extend(other)
        assert ("bar", "baz") in d.items()

        d.extend({"foo": "moo"})
        assert ("foo", "moo") in d.items()

        d.extend()
        assert 6 == len(d)

        with pytest.raises(TypeError):
            d.extend("foo", "bar")  # type: ignore[arg-type, call-arg]

    def test_extend_from_proxy(
        self,
        md_class: type[MultiDict[str]],
        md_proxy_class: type[MultiDictProxy[str]],
    ) -> None:
        d = md_class([("a", "a"), ("b", "b")])
        proxy = md_proxy_class(d)

        d2 = md_class()
        d2.extend(proxy)

        assert [("a", "a"), ("b", "b")] == list(d2.items())

    def test_clear(
        self,
        md_class: type[MultiDict[str]],
    ) -> None:
        d = md_class([("key", "one")], key="two", foo="bar")

        d.clear()
        assert d == {}
        assert list(d.items()) == []

    def test_del(
        self,
        md_class: type[MultiDict[str]],
    ) -> None:
        d = md_class([("key", "one"), ("key", "two")], foo="bar")
        assert list(d.keys()) == ["key", "key", "foo"]

        del d["key"]
        assert d == {"foo": "bar"}
        assert list(d.items()) == [("foo", "bar")]

        with pytest.raises(KeyError, match="key"):
            del d["key"]

    def test_set_default(
        self,
        md_class: type[MultiDict[str]],
    ) -> None:
        d = md_class([("key", "one"), ("key", "two")], foo="bar")
        assert "one" == d.setdefault("key", "three")
        assert "three" == d.setdefault("otherkey", "three")
        assert "otherkey" in d
        assert "three" == d["otherkey"]

    def test_popitem(
        self,
        md_class: type[MultiDict[str]],
    ) -> None:
        d = md_class()
        d.add("key", "val1")
        d.add("key", "val2")

        assert ("key", "val2") == d.popitem()
        assert [("key", "val1")] == list(d.items())

    def test_popitem_empty_multidict(
        self,
        md_class: type[MultiDict[str]],
    ) -> None:
        d = md_class()

        with pytest.raises(KeyError):
            d.popitem()

    def test_pop(
        self,
        md_class: type[MultiDict[str]],
    ) -> None:
        d = md_class()
        d.add("key", "val1")
        d.add("key", "val2")

        assert "val1" == d.pop("key")
        assert {"key": "val2"} == d

    def test_pop2(
        self,
        md_class: type[MultiDict[str]],
    ) -> None:
        d = md_class()
        d.add("key", "val1")
        d.add("key2", "val2")
        d.add("key", "val3")

        assert "val1" == d.pop("key")
        assert [("key2", "val2"), ("key", "val3")] == list(d.items())

    def test_pop_default(
        self,
        md_class: type[MultiDict[str]],
    ) -> None:
        d = md_class(other="val")

        assert "default" == d.pop("key", "default")
        assert "other" in d

    def test_pop_raises(
        self,
        md_class: type[MultiDict[str]],
    ) -> None:
        d = md_class(other="val")

        with pytest.raises(KeyError, match="key"):
            d.pop("key")

        assert "other" in d

    def test_replacement_order(
        self,
        md_class: type[MultiDict[str]],
    ) -> None:
        d = md_class()
        d.add("key1", "val1")
        d.add("key2", "val2")
        d.add("key1", "val3")
        d.add("key2", "val4")

        d["key1"] = "val"

        expected = [("key1", "val"), ("key2", "val2"), ("key2", "val4")]

        assert expected == list(d.items())

    def test_nonstr_key(
        self,
        md_class: type[MultiDict[str]],
    ) -> None:
        d = md_class()
        with pytest.raises(TypeError):
            d[1] = "val"  # type: ignore[index]

    def test_istr_key(
        self,
        md_class: type[MultiDict[str]],
        ci_str_class: type[str],
    ) -> None:
        d = md_class()
        d[ci_str_class("1")] = "val"
        assert type(list(d.keys())[0]) is ci_str_class

    def test_str_derived_key(
        self,
        md_class: type[MultiDict[str]],
    ) -> None:
        class A(str):
            pass

        d = md_class()
        d[A("1")] = "val"
        assert type(list(d.keys())[0]) is A

    def test_istr_key_add(
        self,
        md_class: type[MultiDict[str]],
        ci_str_class: type[str],
    ) -> None:
        d = md_class()
        d.add(ci_str_class("1"), "val")
        assert type(list(d.keys())[0]) is ci_str_class

    def test_str_derived_key_add(
        self,
        md_class: type[MultiDict[str]],
    ) -> None:
        class A(str):
            pass

        d = md_class()
        d.add(A("1"), "val")
        assert type(list(d.keys())[0]) is A

    def test_popall(
        self,
        md_class: type[MultiDict[str]],
    ) -> None:
        d = md_class()
        d.add("key1", "val1")
        d.add("key2", "val2")
        d.add("key1", "val3")
        ret = d.popall("key1")
        assert ["val1", "val3"] == ret
        assert {"key2": "val2"} == d

    def test_popall_default(
        self,
        md_class: type[MultiDict[str]],
    ) -> None:
        d = md_class()
        assert "val" == d.popall("key", "val")

    def test_popall_key_error(
        self,
        md_class: type[MultiDict[str]],
    ) -> None:
        d = md_class()
        with pytest.raises(KeyError, match="key"):
            d.popall("key")

    def test_large_multidict_resizing(
        self,
        md_class: type[MultiDict[int]],
    ) -> None:
        SIZE = 1024
        d = md_class()
        for i in range(SIZE):
            d["key" + str(i)] = i

        for i in range(SIZE - 1):
            del d["key" + str(i)]

        assert {"key" + str(SIZE - 1): SIZE - 1} == d

    def test_update(
        self,
        md_class: type[MultiDict[Union[str, int]]],
    ) -> None:
        d = md_class()
        assert d == {}

        d.update([("key", "one"), ("key", "two")], key=3, foo="bar")
        assert d != {"key": "one", "foo": "bar"}
        assert 4 == len(d)
        itms = d.items()
        # we can't guarantee order of kwargs
        assert ("key", "one") in itms
        assert ("key", "two") in itms
        assert ("key", 3) in itms
        assert ("foo", "bar") in itms

        other = md_class(bar="baz")
        assert other == {"bar": "baz"}

        d.update(other)
        assert ("bar", "baz") in d.items()

        d.update({"foo": "moo"})
        assert ("foo", "moo") in d.items()

        d.update()
        assert 5 == len(d)

        with pytest.raises(TypeError):
            d.update("foo", "bar")  # type: ignore[arg-type, call-arg]


class TestCIMutableMultiDict:
    def test_getall(
        self,
        ci_md_class: type[CIMultiDict[str]],
    ) -> None:
        d = ci_md_class([("KEY", "value1")], KEY="value2")

        assert d != {"KEY": "value1"}
        assert len(d) == 2

        assert d.getall("key") == ["value1", "value2"]

        with pytest.raises(KeyError, match="some_key"):
            d.getall("some_key")

    def test_ctor(
        self,
        ci_md_class: type[CIMultiDict[str]],
    ) -> None:
        d = ci_md_class(k1="v1")
        assert "v1" == d["K1"]
        assert ("k1", "v1") in d.items()

    def test_setitem(
        self,
        ci_md_class: type[CIMultiDict[str]],
    ) -> None:
        d = ci_md_class()
        d["k1"] = "v1"
        assert "v1" == d["K1"]
        assert ("k1", "v1") in d.items()

    def test_delitem(
        self,
        ci_md_class: type[CIMultiDict[str]],
    ) -> None:
        d = ci_md_class()
        d["k1"] = "v1"
        assert "K1" in d
        del d["k1"]
        assert "K1" not in d

    def test_copy(
        self,
        ci_md_class: type[CIMultiDict[str]],
    ) -> None:
        d1 = ci_md_class(key="KEY", a="b")

        d2 = d1.copy()
        assert d1 == d2
        assert d1.items() == d2.items()
        assert d1 is not d2

    def test__repr__(
        self,
        ci_md_class: type[CIMultiDict[str]],
    ) -> None:
        d = ci_md_class()
        assert str(d) == "<%s()>" % ci_md_class.__name__

        d = ci_md_class([("KEY", "one"), ("KEY", "two")])

        expected = (
            f"<{ci_md_class.__name__}"
            "('KEY': 'one', 'KEY': 'two')>"
        )
        assert str(d) == expected

    def test_add(
        self,
        ci_md_class: type[CIMultiDict[str]],
    ) -> None:
        d = ci_md_class()

        assert d == {}
        d["KEY"] = "one"
        assert ("KEY", "one") in d.items()
        assert d == ci_md_class({"Key": "one"})
        assert d.getall("key") == ["one"]

        d["KEY"] = "two"
        assert ("KEY", "two") in d.items()
        assert d == ci_md_class({"Key": "two"})
        assert d.getall("key") == ["two"]

        d.add("KEY", "one")
        assert ("KEY", "one") in d.items()
        assert 2 == len(d)
        assert d.getall("key") == ["two", "one"]

        d.add("FOO", "bar")
        assert ("FOO", "bar") in d.items()
        assert 3 == len(d)
        assert d.getall("foo") == ["bar"]

        d.add(key="test", value="test")
        assert ("test", "test") in d.items()
        assert 4 == len(d)
        assert d.getall("test") == ["test"]

    def test_extend(
        self,
        ci_md_class: type[CIMultiDict[Union[str, int]]],
    ) -> None:
        d = ci_md_class()
        assert d == {}

        d.extend([("KEY", "one"), ("key", "two")], key=3, foo="bar")
        assert 4 == len(d)
        itms = d.items()
        # we can't guarantee order of kwargs
        assert ("KEY", "one") in itms
        assert ("key", "two") in itms
        assert ("key", 3) in itms
        assert ("foo", "bar") in itms

        other = ci_md_class(Bar="baz")
        assert other == {"Bar": "baz"}

        d.extend(other)
        assert ("Bar", "baz") in d.items()
        assert "bar" in d

        d.extend({"Foo": "moo"})
        assert ("Foo", "moo") in d.items()
        assert "foo" in d

        d.extend()
        assert 6 == len(d)

        with pytest.raises(TypeError):
            d.extend("foo", "bar")  # type: ignore[arg-type, call-arg]

    def test_extend_from_proxy(
        self,
        ci_md_class: type[CIMultiDict[str]],
        ci_md_proxy_class: type[CIMultiDictProxy[str]],
    ) -> None:
        d = ci_md_class([("a", "a"), ("b", "b")])
        proxy = ci_md_proxy_class(d)

        d2 = ci_md_class()
        d2.extend(proxy)

        assert [("a", "a"), ("b", "b")] == list(d2.items())

    def test_clear(
        self,
        ci_md_class: type[CIMultiDict[str]],
    ) -> None:
        d = ci_md_class([("KEY", "one")], key="two", foo="bar")

        d.clear()
        assert d == {}
        assert list(d.items()) == []

    def test_del(
        self,
        ci_md_class: type[CIMultiDict[str]],
    ) -> None:
        d = ci_md_class(
            [("KEY", "one"), ("key", "two")],
            foo="bar",
        )

        del d["key"]
        assert d == {"foo": "bar"}
        assert list(d.items()) == [("foo", "bar")]

        with pytest.raises(KeyError, match="key"):
            del d["key"]

    def test_set_default(
        self,
        ci_md_class: type[CIMultiDict[str]],
    ) -> None:
        d = ci_md_class(
            [("KEY", "one"), ("key", "two")],
            foo="bar",
        )
        assert "one" == d.setdefault("key", "three")
        assert "three" == d.setdefault("otherkey", "three")
        assert "otherkey" in d
        assert ("otherkey", "three") in d.items()
        assert "three" == d["OTHERKEY"]

    def test_popitem(
        self,
        ci_md_class: type[CIMultiDict[str]],
    ) -> None:
        d = ci_md_class()
        d.add("KEY", "val1")
        d.add("key", "val2")

        pair = d.popitem()
        assert ("key", "val2") == pair
        assert isinstance(pair[0], str)
        assert [("KEY", "val1")] == list(d.items())

    def test_popitem_empty_multidict(
        self,
        ci_md_class: type[CIMultiDict[str]],
    ) -> None:
        d = ci_md_class()

        with pytest.raises(KeyError):
            d.popitem()

    def test_pop(
        self,
        ci_md_class: type[CIMultiDict[str]],
    ) -> None:
        d = ci_md_class()
        d.add("KEY", "val1")
        d.add("key", "val2")

        assert "val1" == d.pop("KEY")
        assert {"key": "val2"} == d

    def test_pop_lowercase(
        self,
        ci_md_class: type[CIMultiDict[str]],
    ) -> None:
        d = ci_md_class()
        d.add("KEY", "val1")
        d.add("key", "val2")

        assert "val1" == d.pop("key")
        assert {"key": "val2"} == d

    def test_pop_default(
        self,
        ci_md_class: type[CIMultiDict[str]],
    ) -> None:
        d = ci_md_class(OTHER="val")

        assert "default" == d.pop("key", "default")
        assert "other" in d

    def test_pop_raises(
        self,
        ci_md_class: type[CIMultiDict[str]],
    ) -> None:
        d = ci_md_class(OTHER="val")

        with pytest.raises(KeyError, match="KEY"):
            d.pop("KEY")

        assert "other" in d

    def test_extend_with_istr(
        self,
        ci_md_class: type[CIMultiDict[str]],
        ci_str_class: type[istr],
    ) -> None:
        us = ci_str_class("aBc")
        d = ci_md_class()

        d.extend([(us, "val")])
        assert [("aBc", "val")] == list(d.items())

    def test_copy_istr(
        self,
        ci_md_class: type[CIMultiDict[str]],
        ci_str_class: type[istr],
    ) -> None:
        d = ci_md_class({ci_str_class("Foo"): "bar"})
        d2 = d.copy()
        assert d == d2

    def test_eq(
        self,
        ci_md_class: type[CIMultiDict[str]],
    ) -> None:
        d1 = ci_md_class(Key="val")
        d2 = ci_md_class(KEY="val")

        assert d1 == d2

    @pytest.mark.skipif(
        sys.implementation.name == "pypy",
        reason="getsizeof() is not implemented on PyPy",
    )
    def test_sizeof(
        self,
        ci_md_class: type[CIMultiDict[str]],
    ) -> None:
        md = ci_md_class()
        s1 = sys.getsizeof(md)
        for i in string.ascii_lowercase:
            for j in string.ascii_uppercase:
                md[i + j] = i + j
        # multidict should be resized
        s2 = sys.getsizeof(md)
        assert s2 > s1

    @pytest.mark.skipif(
        sys.implementation.name == "pypy",
        reason="getsizeof() is not implemented on PyPy",
    )
    def test_min_sizeof(
        self,
        ci_md_class: type[CIMultiDict[str]],
    ) -> None:
        md = ci_md_class()
        assert sys.getsizeof(md) < 1024

    def test_issue_620_items(
        self,
        ci_md_class: type[CIMultiDict[str]],
    ) -> None:
        # https://github.com/aio-libs/multidict/issues/620
        d = ci_md_class({"a": "123, 456", "b": "789"})
        before_mutation_items = d.items()
        d["c"] = "000"
        # This causes an error on pypy.
        list(before_mutation_items)

    def test_issue_620_keys(
        self,
        ci_md_class: type[CIMultiDict[str]],
    ) -> None:
        # https://github.com/aio-libs/multidict/issues/620
        d = ci_md_class({"a": "123, 456", "b": "789"})
        before_mutation_keys = d.keys()
        d["c"] = "000"
        # This causes an error on pypy.
        list(before_mutation_keys)

    def test_issue_620_values(
        self,
        ci_md_class: type[CIMultiDict[str]],
    ) -> None:
        # https://github.com/aio-libs/multidict/issues/620
        d = ci_md_class({"a": "123, 456", "b": "789"})
        before_mutation_values = d.values()
        d["c"] = "000"
        # This causes an error on pypy.
        list(before_mutation_values)

    def test_keys_type(
        self,
        ci_md_class: type[CIMultiDict[str]],
        ci_str_class: type[istr],
    ) -> None:
        d = ci_md_class(
            [
                ("KEY", "one"),
            ]
        )
        d["k2"] = "2"
        d.extend(k3="3")

        for k in d:
            assert type(k) is ci_str_class

        for k in d.keys():
            assert type(k) is ci_str_class

        for k, v in d.items():
            assert type(k) is ci_str_class

        k, v = d.popitem()
        assert type(k) is ci_str_class
