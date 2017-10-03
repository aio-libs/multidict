import copy

import pytest

from multidict._compat import USE_CYTHON

if USE_CYTHON:
    from multidict._multidict import (MultiDict, CIMultiDict,
                                      MultiDictProxy, CIMultiDictProxy)

from multidict._multidict_py import (MultiDict as PyMultiDict,  # noqa: E402
                                     CIMultiDict as PyCIMultiDict,
                                     MultiDictProxy as PyMultiDictProxy,
                                     CIMultiDictProxy as PyCIMultiDictProxy)


@pytest.fixture(
    params=(
        [
            MultiDict,
            CIMultiDict,
        ]
        if USE_CYTHON else
        []
    ) +
    [
        PyMultiDict,
        PyCIMultiDict
    ],
    ids=(
        [
            'MultiDict',
            'CIMultiDict',
        ]
        if USE_CYTHON else
        []
    ) +
    [
        'PyMultiDict',
        'PyCIMultiDict'
    ]
)
def cls(request):
    return request.param


@pytest.fixture(
    params=(
        [
            (MultiDictProxy, MultiDict),
            (CIMultiDictProxy, CIMultiDict),
        ]
        if USE_CYTHON else
        []
    ) +
    [
        (PyMultiDictProxy, PyMultiDict),
        (PyCIMultiDictProxy, PyCIMultiDict),
    ],
    ids=(
        [
            'MultiDictProxy',
            'CIMultiDictProxy',
        ]
        if USE_CYTHON else
        []
    ) +
    [
        'PyMultiDictProxy',
        'PyCIMultiDictProxy'
    ]
)
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
