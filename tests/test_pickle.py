import pickle

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


def test_pickle(cls):
    d = cls([('a', 1), ('a', 2)])
    pbytes = pickle.dumps(d)
    obj = pickle.loads(pbytes)
    assert d == obj


def test_pickle_proxy(proxy_classes):
    proxy_cls, dict_cls = proxy_classes
    d = dict_cls([('a', 1), ('a', 2)])
    proxy = proxy_cls(d)
    with pytest.raises(TypeError):
        pickle.dumps(proxy)
