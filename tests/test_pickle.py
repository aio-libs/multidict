import pickle

from pathlib import Path

import pytest

from multidict._multidict import (MultiDict, CIMultiDict,
                                  MultiDictProxy, CIMultiDictProxy)
from multidict._multidict_py import (MultiDict as PyMultiDict,
                                     CIMultiDict as PyCIMultiDict,
                                     MultiDictProxy as PyMultiDictProxy,
                                     CIMultiDictProxy as PyCIMultiDictProxy)

here = Path(__file__).resolve().parent


@pytest.fixture(params=['MultiDict', 'PyMultiDict',
                        'CIMultiDict', 'PyCIMultiDict'])
def cls_name(request):
    return request.param


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


def test_pickle(cls, pickle_protocol):
    d = cls([('a', 1), ('a', 2)])
    pbytes = pickle.dumps(d, pickle_protocol)
    obj = pickle.loads(pbytes)
    assert d == obj
    assert isinstance(obj, cls)


def test_pickle_proxy(proxy_classes):
    proxy_cls, dict_cls = proxy_classes
    d = dict_cls([('a', 1), ('a', 2)])
    proxy = proxy_cls(d)
    with pytest.raises(TypeError):
        pickle.dumps(proxy)


def test_load_from_file(pickle_protocol, cls_name):
    cls = globals()[cls_name]
    d = cls([('a', 1), ('a', 2)])
    fname = '{}.pickle.{}'.format(cls_name.lower(), pickle_protocol)
    p = here / fname
    print(p)
    with p.open('rb') as f:
        obj = pickle.load(f)
    assert d == obj
    assert isinstance(obj, cls)
