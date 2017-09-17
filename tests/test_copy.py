import copy
import pytest

from multidict._multidict import (MultiDict, CIMultiDict,
                                  MultiDictProxy, CIMultiDictProxy)
from multidict._multidict_py import (MultiDict as PyMultiDict,
                                     CIMultiDict as PyCIMultiDict,
                                     MultiDictProxy as PyMultiDictProxy,
                                     CIMultiDictProxy as PyCIMultiDictProxy)


@pytest.fixture(params=[MultiDict, PyMultiDict,
                        CIMultiDict, PyCIMultiDict],
                ids=['MultiDict', 'PyMultiDict',
                     'CIMultiDict', 'PyCIMultiDict'])
def cls(request):
    return request.param


@pytest.fixture(params=[(MultiDictProxy, MultiDict),
                        (PyMultiDictProxy, PyMultiDict),
                        (CIMultiDictProxy, CIMultiDict),
                        (PyCIMultiDictProxy, PyCIMultiDict)],
                ids=['MultiDictProxy', 'PyMultiDictProxy',
                     'CIMultiDictProxy', 'PyCIMultiDictProxy'])
def proxy_classes(request):
    return request.param


def test_copy(cls):
    d = cls()
    d['foo'] = 6
    d2 = d.copy()
    d2['foo'] = 7
    assert d['foo'] == 6
    assert d2['foo'] == 7


def test_copy_proxy(proxy_classes):
    proxy_cls, dict_cls = proxy_classes
    d = dict_cls()
    d['foo'] = 6
    p = proxy_cls(d)
    d2 = p.copy()
    d2['foo'] = 7
    assert d['foo'] == 6
    assert p['foo'] == 6
    assert d2['foo'] == 7


def test_copy_std_copy(cls):
    d = cls()
    d['foo'] = 6
    d2 = copy.copy(d)
    d2['foo'] = 7
    assert d['foo'] == 6
    assert d2['foo'] == 7


def test_ci_multidict_clone(cls):
    d = cls(foo=6)
    d2 = cls(d)
    d2['foo'] = 7
    assert d['foo'] == 6
    assert d2['foo'] == 7
