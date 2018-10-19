import pytest

from multidict import MultiDict, CIMultiDict, MultiDictProxy, CIMultiDictProxy

MUTABLE_DICTS = [MultiDict, CIMultiDict]
ALL_DICTS = [MultiDict, CIMultiDict, MultiDictProxy, CIMultiDictProxy]


def create_any_instance(request, val):
    inst = {
        MultiDict: MultiDict(val),
        CIMultiDict: CIMultiDict(val),
        MultiDictProxy: MultiDictProxy(MultiDict(val)),
        CIMultiDictProxy: CIMultiDictProxy(CIMultiDict(val)),
    }[request.param]
    return inst

@pytest.fixture(params=ALL_DICTS)
def md_simple(request):
    val = [('key1', 'value1')]
    return create_any_instance(request, val)

@pytest.fixture(params=ALL_DICTS)
def md_twokeys(request):
    val = [('key1', 'value1'), ('key2', 'value2')]
    return create_any_instance(request, val)

@pytest.fixture(params=ALL_DICTS)
def md_multivalue(request):
    val = [('key1', 'one'), ('key2', 'two'), ('key1', 3)]
    return create_any_instance(request, val)


@pytest.mark.parametrize("cls", MUTABLE_DICTS)
class TestInstantiation:

    def test_instantiate__empty(self, cls):
        d = cls()
        assert d == {}
        assert len(d) == 0
        assert list(d.keys()) == []
        assert list(d.values()) == []
        assert list(d.items()) == []

        assert cls() != list()

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

    def test_cannot_create_from_unaccepted(self, cls):
        with pytest.raises(TypeError):
            cls([(1, 2, 3)])

        with pytest.raises(TypeError, match=r'(2 given)'):
            cls(('key1', 'value1'), ('key2', 'value2'))


class TestContents:

    def test_getting_items(self, md_multivalue):
        assert md_multivalue.getone('key1') == 'one'
        assert md_multivalue.getone('key1') == 'one'
        assert md_multivalue.get('key1') == 'one'
        assert md_multivalue['key1'] == 'one'

        with pytest.raises(KeyError, match='key99'):
            md_multivalue['key99']
        with pytest.raises(KeyError, match='key99'):
            md_multivalue.getone('key99')

        assert md_multivalue.getone('key99', 'default') == 'default'

    def test__iter__(self, md_multivalue):
        assert list(md_multivalue) == ['key1', 'key2', 'key1']

    def test_keys__contains(self, md_multivalue):
        assert list(md_multivalue.keys()) == ['key1', 'key2', 'key1']

        assert 'key1' in md_multivalue.keys()
        assert 'key2' in md_multivalue.keys()

        assert 'foo' not in md_multivalue.keys()

    def test_values__contains(self, md_multivalue):
        assert list(md_multivalue.values()) == ['one', 'two', 3]

        assert 'one' in md_multivalue.values()
        assert 'two' in md_multivalue.values()
        assert 3 in md_multivalue.values()

        assert 'foo' not in md_multivalue.values()

    def test_items__contains(self, md_multivalue):
        assert list(md_multivalue.items()) == [('key1', 'one'), ('key2', 'two'), ('key1', 3)]

        assert ('key1', 'one') in md_multivalue.items()
        assert ('key2', 'two') in md_multivalue.items()
        assert ('key1', 3) in md_multivalue.items()

        assert ('foo', 'bar') not in md_multivalue.items()


class TestComparisons:

    def test_keys_is_set_less(self, md_simple):
        assert md_simple.keys() < {'key1', 'key2'}

    def test_keys_is_set_less_equal(self, md_simple):
        assert md_simple.keys() <= {'key1'}

    def test_keys_is_set_equal(self, md_simple):
        assert md_simple.keys() == {'key1'}

    def test_keys_is_set_greater(self, md_simple):
        assert {'key1', 'key2'} > md_simple.keys()

    def test_keys_is_set_greater_equal(self, md_simple):
        assert {'key1'} >= md_simple.keys()

    def test_keys_is_set_not_equal(self, md_simple):
        assert md_simple.keys() != {'key2'}

    def test_eq(self, md_simple):
        assert {'key1': 'value1'} == md_simple

    @pytest.mark.parametrize("cls", MUTABLE_DICTS)
    def test_eq2(self, cls, md_simple):
        another_md = cls([('key2', 'value1')])
        assert md_simple != another_md

    @pytest.mark.parametrize("cls", MUTABLE_DICTS)
    def test_eq3(self, cls, md_simple):
        empty_md = cls()
        assert md_simple != empty_md

    @pytest.mark.parametrize("cls", MUTABLE_DICTS)
    def test_eq_other_mapping_contains_more_keys(self, cls):
        d1 = cls(foo='bar')
        d2 = dict(foo='bar', bar='baz')
        assert d1 != d2

    def test_ne(self, md_simple):
        assert md_simple != {'key1': 'another_value'}

    def test_and(self, md_simple):
        assert {'key1'} == md_simple.keys() & {'key1', 'key2'}

    def test_and2(self, md_simple):
        assert {'key1'} == {'key1', 'key2'} & md_simple.keys()

    def test_or(self, md_simple):
        assert {'key1', 'key2'} == md_simple.keys() | {'key2'}

    def test_or2(self, md_simple):
        assert {'key1', 'key2'} == {'key2'} | md_simple.keys()

    def test_sub(self, md_twokeys):
        assert {'key1'} == md_twokeys.keys() - {'key2'}

    def test_sub2(self, md_twokeys):
        assert {'key3'} == {'key1', 'key2', 'key3'} - md_twokeys.keys()

    def test_xor(self, md_twokeys):
        assert {'key1', 'key3'} == md_twokeys.keys() ^ {'key2', 'key3'}

    def test_xor2(self, md_twokeys):
        assert {'key1', 'key3'} == {'key2', 'key3'} ^ md_twokeys.keys()

    @pytest.mark.parametrize('_set, expected', [
        ({'key2'}, True),
        ({'key1'}, False)
    ])
    def test_isdisjoint(self, md_simple, _set, expected):
        assert md_simple.keys().isdisjoint(_set) == expected
