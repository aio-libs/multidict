import sys
import pytest

import multidict

from multidict._compat import USE_CYTHON

if USE_CYTHON:
    from multidict._multidict import (
        MultiDictProxy,
        MultiDict,
        CIMultiDictProxy,
        CIMultiDict,
        istr
    )

from multidict._multidict_py import (  # noqa: E402
    MultiDictProxy as _MultiDictProxy,
    MultiDict as _MultiDict,
    CIMultiDictProxy as _CIMultiDictProxy,
    CIMultiDict as _CIMultiDict,
    istr as _istr
)


class _Root:

    cls = None

    proxy_cls = None

    istr_cls = None

    key_cls = None

    def test_exposed_names(self):
        name = self.cls.__name__
        while name.startswith('_'):
            name = name[1:]
        assert name in multidict.__all__


class _BaseTest(_Root):

    def test_instantiate__empty(self):
        d = self.make_dict()
        assert d == {}
        assert len(d) == 0
        assert list(d.keys()) == []
        assert list(d.values()) == []
        assert list(d.values()) == []
        assert list(d.items()) == []
        assert list(d.items()) == []

        assert self.make_dict() != list()
        with pytest.raises(TypeError) as excinfo:
            self.make_dict(('key1', 'value1'), ('key2', 'value2'))
        assert '(2 given)' in str(excinfo.value)

    def test_instantiate__from_arg0(self):
        d = self.make_dict([('key', 'value1')])

        assert d == {'key': 'value1'}
        assert len(d) == 1
        assert list(d.keys()) == ['key']
        assert list(d.values()) == ['value1']
        assert list(d.items()) == [('key', 'value1')]

    def test_instantiate__from_arg0_dict(self):
        d = self.make_dict({'key': 'value1'})

        assert d == {'key': 'value1'}
        assert len(d) == 1
        assert list(d.keys()) == ['key']
        assert list(d.values()) == ['value1']
        assert list(d.items()) == [('key', 'value1')]

    def test_instantiate__with_kwargs(self):
        d = self.make_dict([('key', 'value1')], key2='value2')

        assert d == {'key': 'value1', 'key2': 'value2'}
        assert len(d) == 2
        assert sorted(d.keys()) == ['key', 'key2']
        assert sorted(d.values()) == ['value1', 'value2']
        assert sorted(d.items()) == [('key', 'value1'),
                                     ('key2', 'value2')]

    def test_instantiate__from_generator(self):
        d = self.make_dict((str(i), i) for i in range(2))

        assert d == {'0': 0, '1': 1}
        assert len(d) == 2
        assert sorted(d.keys()) == ['0', '1']
        assert sorted(d.values()) == [0, 1]
        assert sorted(d.items()) == [('0', 0), ('1', 1)]

    def test_getone(self):
        d = self.make_dict([('key', 'value1')], key='value2')
        assert d.getone('key') == 'value1'
        assert d.get('key') == 'value1'
        assert d['key'] == 'value1'

        with pytest.raises(KeyError) as excinfo:
            d['key2']
        assert 'key2' in str(excinfo.value)
        with pytest.raises(KeyError) as exc2info:
            d.getone('key2')
        assert 'key2' in str(exc2info.value)

        assert 'default' == d.getone('key2', 'default')

    def test__iter__(self):
        d = self.make_dict([('key', 'one'), ('key2', 'two'), ('key', 3)])
        assert ['key', 'key2', 'key'] == list(d)

    def test__iter__types(self):
        d = self.make_dict([('key', 'one'), ('key2', 'two'), ('key', 3)])
        for i in d:
            assert type(i) is self.key_cls, (type(i), self.key_cls)

    def test_keys__contains(self):
        d = self.make_dict([('key', 'one'), ('key2', 'two'), ('key', 3)])
        assert list(d.keys()) == ['key', 'key2', 'key']

        assert 'key' in d.keys()
        assert 'key2' in d.keys()

        assert 'foo' not in d.keys()

    def test_values__contains(self):
        d = self.make_dict([('key', 'one'), ('key', 'two'), ('key', 3)])
        assert list(d.values()) == ['one', 'two', 3]

        assert 'one' in d.values()
        assert 'two' in d.values()
        assert 3 in d.values()

        assert 'foo' not in d.values()

    def test_items__contains(self):
        d = self.make_dict([('key', 'one'), ('key', 'two'), ('key', 3)])
        expected = [('key', 'one'), ('key', 'two'), ('key', 3)]
        assert list(d.items()) == expected
        assert list(d.items()) == expected # was the repetition intentional?

        assert ('key', 'one') in d.items()
        assert ('key', 'two') in d.items()
        assert ('key', 3) in d.items()

        assert ('foo', 'bar') not in d.items()

    def test_cannot_create_from_unaccepted(self):
        with pytest.raises(TypeError):
            self.make_dict([(1, 2, 3)])

    def test_keys_is_set_less(self):
        d = self.make_dict([('key', 'value1')])

        assert d.keys() < {'key', 'key2'}

    def test_keys_is_set_less_equal(self):
        d = self.make_dict([('key', 'value1')])

        assert d.keys() <= {'key'}

    def test_keys_is_set_equal(self):
        d = self.make_dict([('key', 'value1')])

        assert d.keys() == {'key'}

    def test_keys_is_set_greater(self):
        d = self.make_dict([('key', 'value1')])

        assert {'key', 'key2'} > d.keys()

    def test_keys_is_set_greater_equal(self):
        d = self.make_dict([('key', 'value1')])

        assert {'key'} >= d.keys()

    def test_keys_is_set_not_equal(self):
        d = self.make_dict([('key', 'value1')])

        assert d.keys() != {'key2'}

    def test_eq(self):
        d = self.make_dict([('key', 'value1')])
        assert {'key': 'value1'} == d

    def test_eq2(self):
        d1 = self.make_dict([('key', 'value1')])
        d2 = self.make_dict([('key2', 'value1')])
        assert d1 != d2

    def test_eq3(self):
        d1 = self.make_dict([('key', 'value1')])
        d2 = self.make_dict()
        assert d1 != d2

    def test_eq_other_mapping_contains_more_keys(self):
        d1 = self.make_dict(foo='bar')
        d2 = dict(foo='bar', bar='baz')
        assert d1 != d2

    def test_ne(self):
        d = self.make_dict([('key', 'value1')])
        assert d != {'key': 'another_value'}

    def test_and(self):
        d = self.make_dict([('key', 'value1')])
        assert {'key'} == d.keys() & {'key', 'key2'}

    def test_and2(self):
        d = self.make_dict([('key', 'value1')])
        assert {'key'} == {'key', 'key2'} & d.keys()

    def test_or(self):
        d = self.make_dict([('key', 'value1')])
        assert {'key', 'key2'} == d.keys() | {'key2'}

    def test_or2(self):
        d = self.make_dict([('key', 'value1')])
        assert {'key', 'key2'} == {'key2'} | d.keys()

    def test_sub(self):
        d = self.make_dict([('key', 'value1'), ('key2', 'value2')])
        assert {'key'} == d.keys() - {'key2'}

    def test_sub2(self):
        d = self.make_dict([('key', 'value1'), ('key2', 'value2')])
        assert {'key3'} == {'key', 'key2', 'key3'} - d.keys()

    def test_xor(self):
        d = self.make_dict([('key', 'value1'), ('key2', 'value2')])
        assert {'key', 'key3'} == d.keys() ^ {'key2', 'key3'}

    def test_xor2(self):
        d = self.make_dict([('key', 'value1'), ('key2', 'value2')])
        assert {'key', 'key3'} == {'key2', 'key3'} ^ d.keys()

    def test_isdisjoint(self):
        d = self.make_dict([('key', 'value1')])
        assert d.keys().isdisjoint({'key2'})

    def test_isdisjoint2(self):
        d = self.make_dict([('key', 'value1')])
        assert not d.keys().isdisjoint({'key'})

    def test_repr_issue_410(self):
        d = self.make_dict()
        try:
            raise Exception
            self.fail("Sould never happen")  # pragma: no cover
        except Exception as e:
            repr(d)
            assert sys.exc_info()[1] is e

    def test_or_issue_410(self):
        d = self.make_dict([('key', 'value')])
        try:
            raise Exception
            self.fail("Sould never happen")  # pragma: no cover
        except Exception as e:
            d.keys() | {'other'}
            assert sys.exc_info()[1] is e

    def test_and_issue_410(self):
        d = self.make_dict([('key', 'value')])
        try:
            raise Exception
            self.fail("Sould never happen")  # pragma: no cover
        except Exception as e:
            d.keys() & {'other'}
            assert sys.exc_info()[1] is e

    def test_sub_issue_410(self):
        d = self.make_dict([('key', 'value')])
        try:
            raise Exception
            self.fail("Sould never happen")  # pragma: no cover
        except Exception as e:
            d.keys() - {'other'}
            assert sys.exc_info()[1] is e

    def test_xor_issue_410(self):
        d = self.make_dict([('key', 'value')])
        try:
            raise Exception
            self.fail("Sould never happen")  # pragma: no cover
        except Exception as e:
            d.keys() ^ {'other'}
            assert sys.exc_info()[1] is e


class _MultiDictTests(_BaseTest):

    def test__repr__(self):
        d = self.make_dict()
        cls = self.proxy_cls if self.proxy_cls is not None else self.cls

        assert str(d) == "<%s()>" % cls.__name__
        d = self.make_dict([('key', 'one'), ('key', 'two')])
        expected = "<%s('key': 'one', 'key': 'two')>" % cls.__name__
        assert str(d) == expected

    def test_getall(self):
        d = self.make_dict([('key', 'value1')], key='value2')

        assert d != {'key': 'value1'}
        assert len(d) == 2

        assert d.getall('key') == ['value1', 'value2']

        with pytest.raises(KeyError) as excinfo:
            d.getall('some_key')
        assert 'some_key' in str(excinfo.value)

        default = object()
        assert d.getall('some_key', default) is default

    def test_preserve_stable_ordering(self):
        d = self.make_dict([('a', 1), ('b', '2'), ('a', 3)])
        s = '&'.join('{}={}'.format(k, v) for k, v in d.items())

        assert 'a=1&b=2&a=3' == s

    def test_get(self):
        d = self.make_dict([('a', 1), ('a', 2)])
        assert 1 == d['a']

    def test_items__repr__(self):
        d = self.make_dict([('key', 'value1')], key='value2')
        expected = "_ItemsView('key': 'value1', 'key': 'value2')"
        assert repr(d.items()) == expected

    def test_keys__repr__(self):
        d = self.make_dict([('key', 'value1')], key='value2')
        assert repr(d.keys()) == "_KeysView('key', 'key')"

    def test_values__repr__(self):
        d = self.make_dict([('key', 'value1')], key='value2')
        assert repr(d.values()) == "_ValuesView('value1', 'value2')"


class _CIMultiDictTests(_Root):

    def test_basics(self):
        d = self.make_dict([('KEY', 'value1')], KEY='value2')
        assert d.getone('key') == 'value1'
        assert d.get('key') == 'value1'
        assert d.get('key2', 'val') == 'val'
        assert d['key'] == 'value1'
        assert 'key' in d

        with pytest.raises(KeyError):
            d['key2']
        with pytest.raises(KeyError):
            d.getone('key2')

    def test_getall(self):
        d = self.make_dict([('KEY', 'value1')], KEY='value2')

        assert not d == {'KEY': 'value1'}
        assert len(d) == 2

        assert d.getall('key') == ['value1', 'value2']

        with pytest.raises(KeyError) as excinfo:
            d.getall('some_key')
        assert "some_key" in str(excinfo.value)

    def test_get(self):
        d = self.make_dict([('A', 1), ('a', 2)])
        assert 1 == d['a']

    def test__repr__(self):
        d = self.make_dict([('KEY', 'value1')], key='value2')
        cls = type(d)

        expected = "<%s('KEY': 'value1', 'key': 'value2')>" % cls.__name__
        assert str(d) == expected

    def test_items__repr__(self):
        d = self.make_dict([('KEY', 'value1')], key='value2')
        expected = "_ItemsView('KEY': 'value1', 'key': 'value2')"
        assert repr(d.items()) == expected

    def test_keys__repr__(self):
        d = self.make_dict([('KEY', 'value1')], key='value2')
        assert repr(d.keys()) == "_KeysView('KEY', 'key')"

    def test_values__repr__(self):
        d = self.make_dict([('KEY', 'value1')], key='value2')
        assert repr(d.values()) == "_ValuesView('value1', 'value2')"


class _NonProxyCIMultiDict(_CIMultiDictTests):

    def test_extend_with_istr(self):
        us = self.istr_cls('a')
        d = self.make_dict()

        d.extend([(us, 'val')])
        assert [('A', 'val')] == list(d.items())


class _TestProxy(_MultiDictTests):

    def make_dict(self, *args, **kwargs):
        dct = self.cls(*args, **kwargs)
        return self.proxy_cls(dct)

    def test_copy(self):
        d1 = self.cls(key='value', a='b')
        p1 = self.proxy_cls(d1)

        d2 = p1.copy()
        assert d1 == d2
        assert d1 is not d2


class _TestCIProxy(_CIMultiDictTests):

    def make_dict(self, *args, **kwargs):
        dct = self.cls(*args, **kwargs)
        return self.proxy_cls(dct)

    def test_copy(self):
        d1 = self.cls(key='value', a='b')
        p1 = self.proxy_cls(d1)

        d2 = p1.copy()
        assert d1 == d2
        assert d1 is not d2


class _BaseMutableMultiDictTests(_BaseTest):

    def test_copy(self):
        d1 = self.make_dict(key='value', a='b')

        d2 = d1.copy()
        assert d1 == d2
        assert d1 is not d2

    def make_dict(self, *args, **kwargs):
        return self.cls(*args, **kwargs)

    def test__repr__(self):
        d = self.make_dict()
        assert str(d) == "<%s()>" % self.cls.__name__

        d = self.make_dict([('key', 'one'), ('key', 'two')])

        expected = "<%s('key': 'one', 'key': 'two')>" % self.cls.__name__
        assert str(d) == expected

    def test_getall(self):
        d = self.make_dict([('key', 'value1')], key='value2')
        assert len(d) == 2

        assert d.getall('key') == ['value1', 'value2']

        with pytest.raises(KeyError) as excinfo:
            d.getall('some_key')
        assert 'some_key' in str(excinfo.value)

        default = object()
        assert d.getall('some_key', default) is default

    def test_add(self):
        d = self.make_dict()

        assert d == {}
        d['key'] = 'one'
        assert d == {'key': 'one'}
        assert d.getall('key') == ['one']

        d['key'] = 'two'
        assert d == {'key': 'two'}
        assert d.getall('key') == ['two']

        d.add('key', 'one')
        assert 2 == len(d)
        assert d.getall('key') == ['two', 'one']

        d.add('foo', 'bar')
        assert 3 == len(d)
        assert d.getall('foo') == ['bar']

    def test_extend(self):
        d = self.make_dict()
        assert d == {}

        d.extend([('key', 'one'), ('key', 'two')], key=3, foo='bar')
        assert d != {'key': 'one', 'foo': 'bar'}
        assert 4 == len(d)
        itms = d.items()
        # we can't guarantee order of kwargs
        assert ('key', 'one') in itms
        assert ('key', 'two') in itms
        assert ('key', 3) in itms
        assert ('foo', 'bar') in itms

        other = self.make_dict(bar='baz')
        assert other == {'bar': 'baz'}

        d.extend(other)
        assert ('bar', 'baz') in d.items()

        d.extend({'foo': 'moo'})
        assert ('foo', 'moo') in d.items()

        d.extend()
        assert 6 == len(d)

        with pytest.raises(TypeError):
            d.extend('foo', 'bar')

    def test_extend_from_proxy(self):
        d = self.make_dict([('a', 'a'), ('b', 'b')])
        proxy = self.proxy_cls(d)

        d2 = self.make_dict()
        d2.extend(proxy)

        assert [('a', 'a'), ('b', 'b')] == list(d2.items())

    def test_clear(self):
        d = self.make_dict([('key', 'one')], key='two', foo='bar')

        d.clear()
        assert d == {}
        assert list(d.items()) == []

    def test_del(self):
        d = self.make_dict([('key', 'one'), ('key', 'two')], foo='bar')

        del d['key']
        assert d == {'foo': 'bar'}
        assert list(d.items()) == [('foo', 'bar')]

        with pytest.raises(KeyError):
            del d['key']

    def test_set_default(self):
        d = self.make_dict([('key', 'one'), ('key', 'two')], foo='bar')
        assert 'one' == d.setdefault('key', 'three')
        assert 'three' == d.setdefault('otherkey', 'three')
        assert 'otherkey' in d
        assert 'three' == d['otherkey']

    def test_popitem(self):
        d = self.make_dict()
        d.add('key', 'val1')
        d.add('key', 'val2')

        assert ('key', 'val1') == d.popitem()
        assert [('key', 'val2')] == list(d.items())

    def test_popitem_empty_multidict(self):
        d = self.make_dict()

        with pytest.raises(KeyError):
            d.popitem()

    def test_pop(self):
        d = self.make_dict()
        d.add('key', 'val1')
        d.add('key', 'val2')

        assert 'val1' == d.pop('key')
        assert {'key': 'val2'} == d

    def test_pop2(self):
        d = self.make_dict()
        d.add('key', 'val1')
        d.add('key2', 'val2')
        d.add('key', 'val3')

        assert 'val1' == d.pop('key')
        assert [('key2', 'val2'), ('key', 'val3')] == list(d.items())

    def test_pop_default(self):
        d = self.make_dict(other='val')

        assert 'default' == d.pop('key', 'default')
        assert 'other' in d

    def test_pop_raises(self):
        d = self.make_dict(other='val')

        with pytest.raises(KeyError):
            d.pop('key')

        assert 'other' in d

    def test_update(self):
        d = self.make_dict()
        d.add('key', 'val1')
        d.add('key', 'val2')
        d.add('key2', 'val3')

        d.update(key='val')

        assert [('key', 'val'), ('key2', 'val3')] == list(d.items())

    def test_replacement_order(self):
        d = self.make_dict()
        d.add('key1', 'val1')
        d.add('key2', 'val2')
        d.add('key1', 'val3')
        d.add('key2', 'val4')

        d['key1'] = 'val'

        expected = [('key2', 'val2'),
                    ('key1', 'val'),
                    ('key2', 'val4')]
        assert expected == list(d.items())

    def test_nonstr_key(self):
        d = self.make_dict()
        with pytest.raises(TypeError):
            d[1] = 'val'

    if USE_CYTHON:
        def test_istr_key(self):
            d = self.make_dict()
            d[istr('1')] = 'val'
            assert type(list(d.keys())[0]) is str

    def test_str_derived_key(self):
        class A(str):
            pass
        d = self.make_dict()
        d[A('1')] = 'val'
        assert type(list(d.keys())[0]) is str

    def test_popall(self):
        d = self.make_dict()
        d.add('key1', 'val1')
        d.add('key2', 'val2')
        d.add('key1', 'val3')
        ret = d.popall('key1')
        assert ['val1', 'val3'] == ret
        assert {'key2': 'val2'} == d

    def test_popall_default(self):
        d = self.make_dict()
        assert 'val' == d.popall('key', 'val')

    def test_popall_key_error(self):
        d = self.make_dict()
        with pytest.raises(KeyError):
            d.popall('key')


class _CIMutableMultiDictTests(_Root):

    def make_dict(self, *args, **kwargs):
        return self.cls(*args, **kwargs)

    def test_getall(self):
        d = self.make_dict([('KEY', 'value1')], KEY='value2')

        assert d != {'KEY': 'value1'}
        assert len(d) == 2

        assert d.getall('key') == ['value1', 'value2']

        with pytest.raises(KeyError) as excinfo:
            d.getall('some_key')
        assert 'some_key' in str(excinfo.value)

    def test_ctor(self):
        d = self.make_dict(k1='v1')
        assert 'v1' == d['K1']

    def test_setitem(self):
        d = self.make_dict()
        d['k1'] = 'v1'
        assert 'v1' == d['K1']

    def test_delitem(self):
        d = self.make_dict()
        d['k1'] = 'v1'
        assert 'K1' in d
        del d['k1']
        assert 'K1' not in d

    def test_copy(self):
        d1 = self.make_dict(key='KEY', a='b')

        d2 = d1.copy()
        assert d1 == d2
        assert d1 is not d2

    def test__repr__(self):
        d = self.make_dict()
        assert str(d) == "<%s()>" % self.cls.__name__

        d = self.make_dict([('KEY', 'one'), ('KEY', 'two')])

        expected = "<%s('KEY': 'one', 'KEY': 'two')>" % self.cls.__name__
        assert str(d) == expected

    def test_add(self):
        d = self.make_dict()

        assert d == {}
        d['KEY'] = 'one'
        assert d == self.make_dict({'Key': 'one'})
        assert d.getall('key') == ['one']

        d['KEY'] = 'two'
        assert d == self.make_dict({'Key': 'two'})
        assert d.getall('key') == ['two']

        d.add('KEY', 'one')
        assert 2 == len(d)
        assert d.getall('key') == ['two', 'one']

        d.add('FOO', 'bar')
        assert 3 == len(d)
        assert d.getall('foo') == ['bar']

    def test_extend(self):
        d = self.make_dict()
        assert d == {}

        d.extend([('KEY', 'one'), ('key', 'two')], key=3, foo='bar')
        assert 4 == len(d)
        itms = d.items()
        # we can't guarantee order of kwargs
        assert ('KEY', 'one') in itms
        assert ('key', 'two') in itms
        assert ('key', 3) in itms
        assert ('foo', 'bar') in itms

        other = self.make_dict(Bar='baz')
        assert other == {'Bar': 'baz'}

        d.extend(other)
        assert ('Bar', 'baz') in d.items()
        assert 'bar' in d

        d.extend({'Foo': 'moo'})
        assert ('Foo', 'moo') in d.items()
        assert 'foo' in d

        d.extend()
        assert 6 == len(d)

        with pytest.raises(TypeError):
            d.extend('foo', 'bar')

    def test_extend_from_proxy(self):
        d = self.make_dict([('a', 'a'), ('b', 'b')])
        proxy = self.proxy_cls(d)

        d2 = self.make_dict()
        d2.extend(proxy)

        assert [('a', 'a'), ('b', 'b')] == list(d2.items())

    def test_clear(self):
        d = self.make_dict([('KEY', 'one')], key='two', foo='bar')

        d.clear()
        assert d == {}
        assert list(d.items()) == []

    def test_del(self):
        d = self.make_dict([('KEY', 'one'), ('key', 'two')], foo='bar')

        del d['key']
        assert d == {'foo': 'bar'}
        assert list(d.items()) == [('foo', 'bar')]

        with pytest.raises(KeyError):
            del d['key']

    def test_set_default(self):
        d = self.make_dict([('KEY', 'one'), ('key', 'two')], foo='bar')
        assert 'one' == d.setdefault('key', 'three')
        assert 'three' == d.setdefault('otherkey', 'three')
        assert 'otherkey' in d
        assert 'three' == d['OTHERKEY']

    def test_popitem(self):
        d = self.make_dict()
        d.add('KEY', 'val1')
        d.add('key', 'val2')

        pair = d.popitem()
        assert ('KEY', 'val1') == pair
        assert isinstance(pair[0], str)
        assert [('key', 'val2')] == list(d.items())

    def test_popitem_empty_multidict(self):
        d = self.make_dict()

        with pytest.raises(KeyError):
            d.popitem()

    def test_pop(self):
        d = self.make_dict()
        d.add('KEY', 'val1')
        d.add('key', 'val2')

        assert 'val1' == d.pop('KEY')
        assert {'key': 'val2'} == d

    def test_pop_lowercase(self):
        d = self.make_dict()
        d.add('KEY', 'val1')
        d.add('key', 'val2')

        assert 'val1' == d.pop('key')
        assert {'key': 'val2'} == d

    def test_pop_default(self):
        d = self.make_dict(OTHER='val')

        assert 'default' == d.pop('key', 'default')
        assert 'other' in d

    def test_pop_raises(self):
        d = self.make_dict(OTHER='val')

        with pytest.raises(KeyError):
            d.pop('KEY')

        assert 'other' in d

    def test_update(self):
        d = self.make_dict()
        d.add('KEY', 'val1')
        d.add('key', 'val2')
        d.add('key2', 'val3')

        d.update(Key='val')

        assert [('Key', 'val'), ('key2', 'val3')] == list(d.items())

    if USE_CYTHON:
        def test_update_istr(self):
            d = self.make_dict()
            d.add(istr('KEY'), 'val1')
            d.add('key', 'val2')
            d.add('key2', 'val3')

            d.update({istr('key'): 'val'})

            assert [('Key', 'val'), ('key2', 'val3')] == list(d.items())

        def test_copy_istr(self):
            d = self.make_dict({istr('Foo'): 'bar'})
            d2 = d.copy()
            assert d == d2

    def test_eq(self):
        d1 = self.make_dict(Key='val')
        d2 = self.make_dict(KEY='val')

        assert d1 == d2


class TestPyMultiDictProxy(_TestProxy):

    cls = _MultiDict
    proxy_cls = _MultiDictProxy
    key_cls = str


class TestPyCIMultiDictProxy(_TestCIProxy):

    cls = _CIMultiDict
    proxy_cls = _CIMultiDictProxy
    key_cls = istr_cls = _istr


class TestPyMutableMultiDict(_BaseMutableMultiDictTests):

    cls = _MultiDict
    proxy_cls = _MultiDictProxy
    key_cls = str


class TestPyCIMutableMultiDict(_CIMutableMultiDictTests, _NonProxyCIMultiDict):

    cls = _CIMultiDict
    istr_cls = _istr
    proxy_cls = _CIMultiDictProxy
    key_cls = istr_cls


if USE_CYTHON:
    class TestMultiDictProxy(_TestProxy):

        cls = MultiDict
        proxy_cls = MultiDictProxy
        key_cls = str


if USE_CYTHON:
    class TestCIMultiDictProxy(_TestCIProxy):

        cls = CIMultiDict
        proxy_cls = CIMultiDictProxy
        key_cls = istr


if USE_CYTHON:
    class TestMutableMultiDict(_BaseMutableMultiDictTests):

        cls = MultiDict
        proxy_cls = MultiDictProxy
        key_cls = str


if USE_CYTHON:
    class TestCIMutableMultiDict(_CIMutableMultiDictTests,
                                 _NonProxyCIMultiDict):

        cls = CIMultiDict
        istr_cls = istr
        proxy_cls = CIMultiDictProxy
        key_cls = istr_cls


class TypesMixin:

    proxy = ciproxy = mdict = cimdict = None

    def test_proxies(self):
        assert issubclass(self.ciproxy, self.proxy)

    def test_dicts(self):
        assert issubclass(self.cimdict, self.mdict)

    def test_proxy_not_inherited_from_dict(self):
        assert not issubclass(self.proxy, self.mdict)

    def test_dict_not_inherited_from_proxy(self):
        assert not issubclass(self.mdict, self.proxy)

    def test_create_multidict_proxy_from_nonmultidict(self):
        with pytest.raises(TypeError):
            self.proxy({})

    def test_create_multidict_proxy_from_cimultidict(self):
        d = self.cimdict(key='val')
        p = self.proxy(d)
        assert p == d

    def test_create_multidict_proxy_from_multidict_proxy_from_mdict(self):
        d = self.mdict(key='val')
        p = self.proxy(d)
        assert p == d
        p2 = self.proxy(p)
        assert p2 == p

    def test_create_cimultidict_proxy_from_cimultidict_proxy_from_ci(self):
        d = self.cimdict(key='val')
        p = self.ciproxy(d)
        assert p == d
        p2 = self.ciproxy(p)
        assert p2 == p

    def test_create_cimultidict_proxy_from_nonmultidict(self):
        with pytest.raises(TypeError):
            self.ciproxy({})

    def test_create_ci_multidict_proxy_from_multidict(self):
        d = self.mdict(key='val')
        with pytest.raises(TypeError):
            self.ciproxy(d)


class TestPyTypes(TypesMixin):

    proxy = _MultiDictProxy
    ciproxy = _CIMultiDictProxy
    mdict = _MultiDict
    cimdict = _CIMultiDict


if USE_CYTHON:
    class TestTypes(TypesMixin):

        proxy = MultiDictProxy
        ciproxy = CIMultiDictProxy
        mdict = MultiDict
        cimdict = CIMultiDict
