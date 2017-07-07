import pytest

from multidict._multidict import MultiDict
from multidict._multidict_py import MultiDict as PyMultiDict


@pytest.fixture(params=[MultiDict, PyMultiDict],
                ids=['MultiDict', 'PyMultiDict'])
def cls(request):
    return request.param


def test_guard_items(cls):
    md = cls({'a': 'b'})
    it = iter(md.items())
    md['a'] = 'c'
    with pytest.raises(RuntimeError):
        next(it)


def test_guard_keys(cls):
    md = cls({'a': 'b'})
    it = iter(md.keys())
    md['a'] = 'c'
    with pytest.raises(RuntimeError):
        next(it)


def test_guard_values(cls):
    md = cls({'a': 'b'})
    it = iter(md.values())
    md['a'] = 'c'
    with pytest.raises(RuntimeError):
        next(it)
