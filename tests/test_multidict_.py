import pytest

from multidict import MultiDict, CIMultiDict, MultiDictProxy, CIMultiDictProxy

MUTABLE_DICTS = [MultiDict, CIMultiDict]
PROXY_DICTS = [MultiDictProxy, CIMultiDictProxy]


@pytest.fixture(params=MUTABLE_DICTS)
def md_simple(request):
    return request.param([('key1', 'value1')])

@pytest.fixture(params=MUTABLE_DICTS)
def md_w_multivalue_per_key(request):
    return request.param([('key1', 'one'), ('key2', 'two'), ('key1', 3)])


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

    def test_cannot_create_from_unaccepted(self, cls):
        with pytest.raises(TypeError):
            cls([(1, 2, 3)])


@pytest.mark.parametrize("cls", MUTABLE_DICTS+PROXY_DICTS)
class TestContents:

    @pytest.fixture(autouse=True)
    def _autoassign_md_to_d(self, md_w_multivalue_per_key):
        self.d = md_w_multivalue_per_key

    def test_getting_items(self, cls):
        assert self.d.getone('key1') == 'one'
        assert self.d.get('key1') == 'one'
        assert self.d['key1'] == 'one'

        with pytest.raises(KeyError, match='key0'):
            self.d['key0']
        with pytest.raises(KeyError, match='key0'):
            self.d.getone('key0')

        assert self.d.getone('key0', 'default') == 'default'

    def test__iter__(self, cls):
        assert list(self.d) == ['key1', 'key2', 'key1']

    def test_keys__contains(self, cls):
        assert list(self.d.keys()) == ['key1', 'key2', 'key1']

        assert 'key1' in self.d.keys()
        assert 'key2' in self.d.keys()

        assert 'foo' not in self.d.keys()

    def test_values__contains(self, cls):
        assert list(self.d.values()) == ['one', 'two', 3]

        assert 'one' in self.d.values()
        assert 'two' in self.d.values()
        assert 3 in self.d.values()

        assert 'foo' not in self.d.values()

    def test_items__contains(self, cls):
        assert list(self.d.items()) == [('key1', 'one'), ('key2', 'two'), ('key1', 3)]

        assert ('key1', 'one') in self.d.items()
        assert ('key2', 'two') in self.d.items()
        assert ('key1', 3) in self.d.items()

        assert ('foo', 'bar') not in self.d.items()


@pytest.mark.parametrize("cls", MUTABLE_DICTS+PROXY_DICTS)
class TestComparisons:

    @pytest.fixture(autouse=True)
    def _autoassign_md_simple_to_d(self, md_simple):
        self.d = md_simple

    def test_keys_is_set_less(self, cls):
        assert self.d.keys() < {'key1', 'key2'}

    def test_keys_is_set_less_equal(self, cls):
        assert self.d.keys() <= {'key1'}

    def test_keys_is_set_equal(self, cls):
        assert self.d.keys() == {'key1'}
