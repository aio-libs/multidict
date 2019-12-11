import pytest


def test_proxies(_multidict):
    assert issubclass(_multidict.CIMultiDictProxy, _multidict.MultiDictProxy)


def test_dicts(_multidict):
    assert issubclass(_multidict.CIMultiDict, _multidict.MultiDict)


def test_proxy_not_inherited_from_dict(_multidict):
    assert not issubclass(_multidict.MultiDictProxy, _multidict.MultiDict)


def test_dict_not_inherited_from_proxy(_multidict):
    assert not issubclass(_multidict.MultiDict, _multidict.MultiDictProxy)


def test_multidict_proxy_copy_type(_multidict):
    d = _multidict.MultiDict(key="val")
    p = _multidict.MultiDictProxy(d)
    assert isinstance(p.copy(), _multidict.MultiDict)


def test_cimultidict_proxy_copy_type(_multidict):
    d = _multidict.CIMultiDict(key="val")
    p = _multidict.CIMultiDictProxy(d)
    assert isinstance(p.copy(), _multidict.CIMultiDict)


def test_create_multidict_proxy_from_nonmultidict(_multidict):
    with pytest.raises(TypeError):
        _multidict.MultiDictProxy({})


def test_create_multidict_proxy_from_cimultidict(_multidict):
    d = _multidict.CIMultiDict(key="val")
    p = _multidict.MultiDictProxy(d)
    assert p == d


def test_create_multidict_proxy_from_multidict_proxy_from_mdict(_multidict):
    d = _multidict.MultiDict(key="val")
    p = _multidict.MultiDictProxy(d)
    assert p == d
    p2 = _multidict.MultiDictProxy(p)
    assert p2 == p


def test_create_cimultidict_proxy_from_cimultidict_proxy_from_ci(_multidict):
    d = _multidict.CIMultiDict(key="val")
    p = _multidict.CIMultiDictProxy(d)
    assert p == d
    p2 = _multidict.CIMultiDictProxy(p)
    assert p2 == p


def test_create_cimultidict_proxy_from_nonmultidict(_multidict):
    with pytest.raises(
        TypeError,
        match=(
            "ctor requires CIMultiDict or CIMultiDictProxy instance, "
            "not <class 'dict'>"
        ),
    ):
        _multidict.CIMultiDictProxy({})


def test_create_ci_multidict_proxy_from_multidict(_multidict):
    d = _multidict.MultiDict(key="val")
    with pytest.raises(
        TypeError,
        match=(
            "ctor requires CIMultiDict or CIMultiDictProxy instance, "
            "not <class 'multidict._multidict.*.MultiDict'>"
        ),
    ):
        _multidict.CIMultiDictProxy(d)
