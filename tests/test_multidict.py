import operator
import sys
from functools import reduce

import pytest

import multidict


def chained_callable(module, callables):
    """
    Returns callable that will get and call all given objects in module in
    exact order. If `names` is a single object's name function will return
    object itself.

    Will treat `names` of type `str` as a list of single element.
    """
    callables = (callables,) if isinstance(callables, str) else callables
    _callable, *rest = (getattr(module, name) for name in callables)

    def chained_call(*args, **kwargs):
        return reduce(lambda res, c: c(res), rest, _callable(*args, **kwargs))

    return chained_call if len(rest) > 0 else _callable


@pytest.fixture(scope='function')
def cls(request, _multidict):
    return chained_callable(_multidict, request.param)


dict_cls = proxy_cls = cls


@pytest.mark.parametrize('cls', ['MultiDict', 'CIMultiDict'], indirect=True)
def test_exposed_names(cls):
    name = cls.__name__

    while name.startswith('_'):
        name = name[1:]

    assert name in multidict.__all__


@pytest.mark.parametrize('cls, key_cls', [
    ('MultiDict', str),
    (('MultiDict', 'MultiDictProxy'), str)
], indirect=['cls'])
def test__iter__types(cls, key_cls):
    d = cls([('key', 'one'), ('key2', 'two'), ('key', 3)])
    for i in d:
        assert type(i) is key_cls, (type(i), key_cls)


@pytest.mark.parametrize('dict_cls, proxy_cls', [
    ('MultiDict', 'MultiDictProxy'),
    ('CIMultiDict', 'CIMultiDictProxy'),
], indirect=True)
def test_proxy_copy(dict_cls, proxy_cls):
    d1 = dict_cls(key='value', a='b')
    p1 = proxy_cls(d1)

    d2 = p1.copy()
    assert d1 == d2
    assert d1 is not d2


class BaseMultiDictTest:

    def test_instantiate__empty(self, cls):
        d = cls()
        assert d == {}
        assert len(d) == 0
        assert list(d.keys()) == []
        assert list(d.values()) == []
        assert list(d.items()) == []

        assert cls() != list()
        with pytest.raises(TypeError, match='\(2 given\)'):
            cls(('key1', 'value1'), ('key2', 'value2'))

    @pytest.mark.parametrize('arg0', [
        [('key', 'value1')],
        {'key': 'value1'}
    ])
    def test_instantiate__from_arg0(self, cls, arg0):
        d = cls(arg0)

        assert d == {'key': 'value1'}
        assert len(d) == 1
        assert list(d.keys()) == ['key']
        assert list(d.values()) == ['value1']
        assert list(d.items()) == [('key', 'value1')]

    def test_instantiate__with_kwargs(self, cls):
        d = cls([('key', 'value1')], key2='value2')

        assert d == {'key': 'value1', 'key2': 'value2'}
        assert len(d) == 2
        assert sorted(d.keys()) == ['key', 'key2']
        assert sorted(d.values()) == ['value1', 'value2']
        assert sorted(d.items()) == [('key', 'value1'),
                                     ('key2', 'value2')]

    def test_instantiate__from_generator(self, cls):
        d = cls((str(i), i) for i in range(2))

        assert d == {'0': 0, '1': 1}
        assert len(d) == 2
        assert sorted(d.keys()) == ['0', '1']
        assert sorted(d.values()) == [0, 1]
        assert sorted(d.items()) == [('0', 0), ('1', 1)]

    def test_getone(self, cls):
        d = cls([('key', 'value1')], key='value2')

        assert d.getone('key') == 'value1'
        assert d.get('key') == 'value1'
        assert d['key'] == 'value1'

        with pytest.raises(KeyError, match='key2'):
            d['key2']
        with pytest.raises(KeyError, match='key2'):
            d.getone('key2')

        assert d.getone('key2', 'default') == 'default'

    def test__iter__(self, cls, ):
        d = cls([('key', 'one'), ('key2', 'two'), ('key', 3)])
        assert list(d) == ['key', 'key2', 'key']

    def test_keys__contains(self, cls):
        d = cls([('key', 'one'), ('key2', 'two'), ('key', 3)])

        assert list(d.keys()) == ['key', 'key2', 'key']

        assert 'key' in d.keys()
        assert 'key2' in d.keys()

        assert 'foo' not in d.keys()

    def test_values__contains(self, cls):
        d = cls([('key', 'one'), ('key', 'two'), ('key', 3)])

        assert list(d.values()) == ['one', 'two', 3]

        assert 'one' in d.values()
        assert 'two' in d.values()
        assert 3 in d.values()

        assert 'foo' not in d.values()

    def test_items__contains(self, cls):
        d = cls([('key', 'one'), ('key', 'two'), ('key', 3)])

        assert list(d.items()) == [('key', 'one'), ('key', 'two'), ('key', 3)]

        assert ('key', 'one') in d.items()
        assert ('key', 'two') in d.items()
        assert ('key', 3) in d.items()

        assert ('foo', 'bar') not in d.items()

    def test_cannot_create_from_unaccepted(self, cls):
        with pytest.raises(TypeError):
            cls([(1, 2, 3)])

    def test_keys_is_set_less(self, cls):
        d = cls([('key', 'value1')])

        assert d.keys() < {'key', 'key2'}

    def test_keys_is_set_less_equal(self, cls):
        d = cls([('key', 'value1')])

        assert d.keys() <= {'key'}

    def test_keys_is_set_equal(self, cls):
        d = cls([('key', 'value1')])

        assert d.keys() == {'key'}

    def test_keys_is_set_greater(self, cls):
        d = cls([('key', 'value1')])

        assert {'key', 'key2'} > d.keys()

    def test_keys_is_set_greater_equal(self, cls):
        d = cls([('key', 'value1')])

        assert {'key'} >= d.keys()

    def test_keys_is_set_not_equal(self, cls):
        d = cls([('key', 'value1')])

        assert d.keys() != {'key2'}

    def test_eq(self, cls):
        d = cls([('key', 'value1')])

        assert {'key': 'value1'} == d

    def test_eq2(self, cls):
        d1 = cls([('key', 'value1')])
        d2 = cls([('key2', 'value1')])

        assert d1 != d2

    def test_eq3(self, cls):
        d1 = cls([('key', 'value1')])
        d2 = cls()

        assert d1 != d2

    def test_eq_other_mapping_contains_more_keys(self, cls):
        d1 = cls(foo='bar')
        d2 = dict(foo='bar', bar='baz')

        assert d1 != d2

    def test_ne(self, cls):
        d = cls([('key', 'value1')])

        assert d != {'key': 'another_value'}

    def test_and(self, cls):
        d = cls([('key', 'value1')])

        assert {'key'} == d.keys() & {'key', 'key2'}

    def test_and2(self, cls):
        d = cls([('key', 'value1')])

        assert {'key'} == {'key', 'key2'} & d.keys()

    def test_or(self, cls):
        d = cls([('key', 'value1')])

        assert {'key', 'key2'} == d.keys() | {'key2'}

    def test_or2(self, cls):
        d = cls([('key', 'value1')])

        assert {'key', 'key2'} == {'key2'} | d.keys()

    def test_sub(self, cls):
        d = cls([('key', 'value1'), ('key2', 'value2')])

        assert {'key'} == d.keys() - {'key2'}

    def test_sub2(self, cls):
        d = cls([('key', 'value1'), ('key2', 'value2')])

        assert {'key3'} == {'key', 'key2', 'key3'} - d.keys()

    def test_xor(self, cls):
        d = cls([('key', 'value1'), ('key2', 'value2')])

        assert {'key', 'key3'} == d.keys() ^ {'key2', 'key3'}

    def test_xor2(self, cls):
        d = cls([('key', 'value1'), ('key2', 'value2')])

        assert {'key', 'key3'} == {'key2', 'key3'} ^ d.keys()

    @pytest.mark.parametrize('_set, expected', [
        ({'key2'}, True),
        ({'key'}, False)
    ])
    def test_isdisjoint(self, cls, _set, expected):
        d = cls([('key', 'value1')])

        assert d.keys().isdisjoint(_set) == expected

    def test_repr_issue_410(self, cls):
        d = cls()

        try:
            raise Exception
            pytest.fail("Should never happen")  # pragma: no cover
        except Exception as e:
            repr(d)

            assert sys.exc_info()[1] == e

    @pytest.mark.parametrize('op', [
        operator.or_, operator.and_, operator.sub, operator.xor
    ])
    @pytest.mark.parametrize('other', [{'other'}])
    def test_op_issue_410(self, cls, op, other):
        d = cls([('key', 'value')])

        try:
            raise Exception
            pytest.fail("Should never happen")  # pragma: no cover
        except Exception as e:
            op(d.keys(), other)

            assert sys.exc_info()[1] == e


class TestMultiDict(BaseMultiDictTest):

    @pytest.fixture(params=[
        'MultiDict',
        ('MultiDict', 'MultiDictProxy'),
    ])
    def cls(self, request, _multidict):
        return chained_callable(_multidict, request.param)

    def test__repr__(self, cls):
        d = cls()
        _cls = type(d)

        assert str(d) == '<%s()>' % _cls.__name__

        d = cls([('key', 'one'), ('key', 'two')])

        assert str(d) == "<%s('key': 'one', 'key': 'two')>" % _cls.__name__

    def test_getall(self, cls):
        d = cls([('key', 'value1')], key='value2')

        assert d != {'key': 'value1'}
        assert len(d) == 2

        assert d.getall('key') == ['value1', 'value2']

        with pytest.raises(KeyError, match='some_key'):
            d.getall('some_key')

        default = object()
        assert d.getall('some_key', default) is default

    def test_preserve_stable_ordering(self, cls):
        d = cls([('a', 1), ('b', '2'), ('a', 3)])
        s = '&'.join('{}={}'.format(k, v) for k, v in d.items())

        assert s == 'a=1&b=2&a=3'

    def test_get(self, cls):
        d = cls([('a', 1), ('a', 2)])
        assert d['a'] == 1

    def test_items__repr__(self, cls):
        d = cls([('key', 'value1')], key='value2')
        expected = "_ItemsView('key': 'value1', 'key': 'value2')"
        assert repr(d.items()) == expected

    def test_keys__repr__(self, cls):
        d = cls([('key', 'value1')], key='value2')
        assert repr(d.keys()) == "_KeysView('key', 'key')"

    def test_values__repr__(self, cls):
        d = cls([('key', 'value1')], key='value2')
        assert repr(d.values()) == "_ValuesView('value1', 'value2')"


class TestCIMultiDict(BaseMultiDictTest):

    @pytest.fixture(params=[
        'CIMultiDict',
        ('CIMultiDict', 'CIMultiDictProxy'),
    ])
    def cls(self, request, _multidict):
        return chained_callable(_multidict, request.param)

    def test_basics(self, cls):
        d = cls([('KEY', 'value1')], KEY='value2')

        assert d.getone('key') == 'value1'
        assert d.get('key') == 'value1'
        assert d.get('key2', 'val') == 'val'
        assert d['key'] == 'value1'
        assert 'key' in d

        with pytest.raises(KeyError, match='key2'):
            d['key2']
        with pytest.raises(KeyError, match='key2'):
            d.getone('key2')

    def test_getall(self, cls):
        d = cls([('KEY', 'value1')], KEY='value2')

        assert not d == {'KEY': 'value1'}
        assert len(d) == 2

        assert d.getall('key') == ['value1', 'value2']

        with pytest.raises(KeyError, match='some_key'):
            d.getall('some_key')

    def test_get(self, cls):
        d = cls([('A', 1), ('a', 2)])
        assert 1 == d['a']

    def test__repr__(self, cls):
        d = cls([('KEY', 'value1')], key='value2')
        _cls = type(d)

        expected = "<%s('KEY': 'value1', 'key': 'value2')>" % _cls.__name__
        assert str(d) == expected

    def test_items__repr__(self, cls):
        d = cls([('KEY', 'value1')], key='value2')
        expected = "_ItemsView('KEY': 'value1', 'key': 'value2')"
        assert repr(d.items()) == expected

    def test_keys__repr__(self, cls):
        d = cls([('KEY', 'value1')], key='value2')
        assert repr(d.keys()) == "_KeysView('KEY', 'key')"

    def test_values__repr__(self, cls):
        d = cls([('KEY', 'value1')], key='value2')
        assert repr(d.values()) == "_ValuesView('value1', 'value2')"
