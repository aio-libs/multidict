import pytest

import multidict

_testcapi = pytest.importorskip("multidict._testcapi")

pytestmark = pytest.mark.capi

MultiDictStr = multidict.MultiDict[str]
CIMultiDictStr = multidict.CIMultiDict[str]


@pytest.mark.parametrize(
    "func, cls",
    [
        (_testcapi.md_type, multidict.MultiDict),
        (_testcapi.cimd_type, multidict.CIMultiDict),
        (_testcapi.mdproxy_type, multidict.MultiDictProxy),
        (_testcapi.cimdproxy_type, multidict.CIMultiDictProxy),
    ],
    ids=["multidict", "cimultidict", "multidict_proxy", "cimultidict_proxy"],
)
def test_get_type(func: object, cls: object) -> None:
    assert func() is cls  # type: ignore[operator]


def test_md_new() -> None:
    md = _testcapi.md_new(0)
    assert isinstance(md, multidict.MultiDict)
    assert not isinstance(md, multidict.CIMultiDict)
    assert len(md) == 0


def test_cimd_new() -> None:
    md = _testcapi.cimd_new(0)
    assert isinstance(md, multidict.CIMultiDict)
    assert len(md) == 0


def test_md_add_multidict() -> None:
    md: MultiDictStr = multidict.MultiDict()
    _testcapi.md_add(md, "key", "value")
    _testcapi.md_add(md, "key", "value2")
    assert list(md.items()) == [("key", "value"), ("key", "value2")]


def test_md_add_cimultidict() -> None:
    # There is no separate CIMultiDict_Add: MultiDict_Add works against a
    # CIMultiDict instance too, since it is a MultiDict subclass.
    md: CIMultiDictStr = multidict.CIMultiDict()
    _testcapi.md_add(md, "KEY", "value")
    assert list(md.items()) == [("KEY", "value")]
    assert md["key"] == "value"


@pytest.mark.parametrize(
    "md",
    [multidict.MultiDict(key="value"), multidict.CIMultiDict(key="value")],
    ids=["multidict", "cimultidict"],
)
def test_md_clear(md: object) -> None:
    # MultiDict_Clear also works against a CIMultiDict, same as MultiDict_Add.
    _testcapi.md_clear(md)
    assert len(md) == 0  # type: ignore[arg-type]


def test_mdproxy_new_from_multidict() -> None:
    md: MultiDictStr = multidict.MultiDict(key="value")
    proxy = _testcapi.mdproxy_new(md)
    assert isinstance(proxy, multidict.MultiDictProxy)
    assert not isinstance(proxy, multidict.CIMultiDictProxy)
    assert proxy["key"] == "value"
    md["key"] = "other"
    assert proxy["key"] == "other"


def test_mdproxy_new_from_cimultidict() -> None:
    # MultiDictProxy_New also accepts a CIMultiDict, same as the Python
    # level MultiDictProxy(CIMultiDict()) constructor call.
    md: CIMultiDictStr = multidict.CIMultiDict(KEY="value")
    proxy = _testcapi.mdproxy_new(md)
    assert isinstance(proxy, multidict.MultiDictProxy)
    assert proxy["key"] == "value"


def test_mdproxy_new_from_proxy() -> None:
    md: MultiDictStr = multidict.MultiDict(key="value")
    proxy = _testcapi.mdproxy_new(multidict.MultiDictProxy(md))
    assert isinstance(proxy, multidict.MultiDictProxy)
    assert proxy["key"] == "value"


def test_mdproxy_new_wrong_type() -> None:
    with pytest.raises(
        TypeError, match="requires a MultiDict or MultiDictProxy instance"
    ):
        _testcapi.mdproxy_new({})


def test_cimdproxy_new_from_cimultidict() -> None:
    md: CIMultiDictStr = multidict.CIMultiDict(KEY="value")
    proxy = _testcapi.cimdproxy_new(md)
    assert isinstance(proxy, multidict.CIMultiDictProxy)
    assert proxy["key"] == "value"
    md["key"] = "other"
    assert proxy["key"] == "other"


def test_cimdproxy_new_from_proxy() -> None:
    md: CIMultiDictStr = multidict.CIMultiDict(key="value")
    proxy = _testcapi.cimdproxy_new(multidict.CIMultiDictProxy(md))
    assert isinstance(proxy, multidict.CIMultiDictProxy)
    assert proxy["key"] == "value"


def test_cimdproxy_new_rejects_plain_multidict() -> None:
    # Unlike MultiDictProxy_New, CIMultiDictProxy_New does not accept a
    # plain (non-case-insensitive) MultiDict instance.
    md: MultiDictStr = multidict.MultiDict(key="value")
    with pytest.raises(
        TypeError, match="requires a CIMultiDict or CIMultiDictProxy instance"
    ):
        _testcapi.cimdproxy_new(md)


def test_cimdproxy_new_wrong_type() -> None:
    with pytest.raises(
        TypeError, match="requires a CIMultiDict or CIMultiDictProxy instance"
    ):
        _testcapi.cimdproxy_new({})


def test_md_getversion_multidict() -> None:
    md: multidict.MultiDict[object] = multidict.MultiDict()
    version = _testcapi.md_getversion(md)
    assert version == multidict.getversion(md)
    md["key"] = "value"
    assert _testcapi.md_getversion(md) != version
    assert _testcapi.md_getversion(md) == multidict.getversion(md)


def test_md_getversion_cimultidict() -> None:
    md: multidict.CIMultiDict[object] = multidict.CIMultiDict(key="value")
    assert _testcapi.md_getversion(md) == multidict.getversion(md)


def test_md_getversion_proxy() -> None:
    md: MultiDictStr = multidict.MultiDict(key="value")
    proxy = multidict.MultiDictProxy(md)
    assert _testcapi.md_getversion(proxy) == _testcapi.md_getversion(md)
    md["key2"] = "value2"
    assert _testcapi.md_getversion(proxy) == _testcapi.md_getversion(md)


def test_md_getversion_ciproxy() -> None:
    md: CIMultiDictStr = multidict.CIMultiDict(key="value")
    proxy = multidict.CIMultiDictProxy(md)
    assert _testcapi.md_getversion(proxy) == _testcapi.md_getversion(md)


def test_md_contains_multidict() -> None:
    md: MultiDictStr = multidict.MultiDict(key="value")
    assert _testcapi.md_contains(md, "key") is True
    assert _testcapi.md_contains(md, "missing") is False


def test_md_contains_cimultidict() -> None:
    md: CIMultiDictStr = multidict.CIMultiDict(KEY="value")
    assert _testcapi.md_contains(md, "key") is True


def test_md_getitem_multidict() -> None:
    md: MultiDictStr = multidict.MultiDict([("key", "value1"), ("key", "value2")])
    assert _testcapi.md_getitem(md, "key") == (True, "value1")


def test_md_getitem_cimultidict() -> None:
    md: CIMultiDictStr = multidict.CIMultiDict(KEY="value")
    assert _testcapi.md_getitem(md, "key") == (True, "value")


def test_md_getitem_missing() -> None:
    # Unlike PyObject *self[key], this reports "not found" via the return
    # code rather than raising KeyError -- the PyDict_GetItemRef design.
    md: MultiDictStr = multidict.MultiDict()
    assert _testcapi.md_getitem(md, "missing") == (False, None)


def test_md_setitem_multidict_replaces_all() -> None:
    md: MultiDictStr = multidict.MultiDict([("key", "value1"), ("key", "value2")])
    _testcapi.md_setitem(md, "key", "new")
    assert list(md.items()) == [("key", "new")]


def test_md_setitem_multidict_adds_missing() -> None:
    md: MultiDictStr = multidict.MultiDict()
    _testcapi.md_setitem(md, "key", "value")
    assert list(md.items()) == [("key", "value")]


def test_md_setitem_cimultidict() -> None:
    md: CIMultiDictStr = multidict.CIMultiDict()
    _testcapi.md_setitem(md, "KEY", "value")
    assert md["key"] == "value"


def test_md_delitem_multidict_removes_all() -> None:
    md: MultiDictStr = multidict.MultiDict([("key", "value1"), ("key", "value2")])
    _testcapi.md_delitem(md, "key")
    assert "key" not in md


def test_md_delitem_cimultidict() -> None:
    md: CIMultiDictStr = multidict.CIMultiDict(KEY="value")
    _testcapi.md_delitem(md, "key")
    assert len(md) == 0


def test_md_delitem_missing() -> None:
    md: MultiDictStr = multidict.MultiDict()
    with pytest.raises(KeyError, match="missing"):
        _testcapi.md_delitem(md, "missing")


def test_md_pop_multidict() -> None:
    md: MultiDictStr = multidict.MultiDict([("key", "value1"), ("key", "value2")])
    assert _testcapi.md_pop(md, "key") == (True, "value1")
    assert list(md.items()) == [("key", "value2")]


def test_md_pop_cimultidict() -> None:
    md: CIMultiDictStr = multidict.CIMultiDict(KEY="value")
    assert _testcapi.md_pop(md, "key") == (True, "value")
    assert len(md) == 0


def test_md_pop_missing() -> None:
    # Same PyDict_GetItemRef-style design as md_getitem: "not found" is a
    # return code, not a KeyError.
    md: MultiDictStr = multidict.MultiDict()
    assert _testcapi.md_pop(md, "missing") == (False, None)


def test_md_setdefault_adds_missing() -> None:
    # PyDict_SetDefaultRef design: the bool reports whether the key was
    # already present (it was not, so `default` got inserted).
    md: MultiDictStr = multidict.MultiDict()
    assert _testcapi.md_setdefault(md, "key", "default") == (False, "default")
    assert list(md.items()) == [("key", "default")]


def test_md_setdefault_keeps_existing() -> None:
    md: MultiDictStr = multidict.MultiDict(key="value")
    assert _testcapi.md_setdefault(md, "key", "default") == (True, "value")
    assert list(md.items()) == [("key", "value")]


def test_md_setdefault_cimultidict() -> None:
    md: CIMultiDictStr = multidict.CIMultiDict()
    assert _testcapi.md_setdefault(md, "KEY", "v1") == (False, "v1")
    assert _testcapi.md_setdefault(md, "key", "v2") == (True, "v1")


def test_istr_type() -> None:
    assert _testcapi.istr_type() is multidict.istr


def test_istr_from_unicode() -> None:
    s = _testcapi.istr_from_unicode("Header")
    assert isinstance(s, multidict.istr)
    assert s == "Header"
    assert s.lower() == "header"


def test_istr_from_unicode_passthrough() -> None:
    # Matches istr(existing_istr): the same object is returned, not a copy.
    s1 = multidict.istr("Header")
    s2 = _testcapi.istr_from_unicode(s1)
    assert s2 is s1


def test_istr_from_unicode_wrong_type() -> None:
    with pytest.raises(TypeError, match="should be a str instance"):
        _testcapi.istr_from_unicode(123)


@pytest.mark.parametrize(
    "md, expected",
    [
        (multidict.MultiDict([("key", "value1"), ("key", "value2")]), 2),
        (multidict.MultiDict(), 0),
        (multidict.CIMultiDict(KEY="value"), 1),
        (multidict.MultiDictProxy(multidict.MultiDict(key="value")), 1),
        (multidict.CIMultiDictProxy(multidict.CIMultiDict(key="value")), 1),
    ],
    ids=["multidict", "empty", "cimultidict", "proxy", "ciproxy"],
)
def test_md_size(md: object, expected: int) -> None:
    assert _testcapi.md_size(md) == expected


def test_md_size_proxy_reflects_live_changes() -> None:
    md: MultiDictStr = multidict.MultiDict(key="value")
    proxy = multidict.MultiDictProxy(md)
    assert _testcapi.md_size(proxy) == 1
    md["key2"] = "value2"
    assert _testcapi.md_size(proxy) == 2


def _md_for_foreach_all() -> list[tuple[object, list[tuple[str, str]]]]:
    md_plain: MultiDictStr = multidict.MultiDict([("a", "1"), ("b", "2"), ("a", "3")])
    md_ci: CIMultiDictStr = multidict.CIMultiDict(KEY="value")
    md_for_proxy: MultiDictStr = multidict.MultiDict([("a", "1"), ("a", "2")])
    md_for_ciproxy: CIMultiDictStr = multidict.CIMultiDict(key="value")
    return [
        (md_plain, list(md_plain.items())),
        (md_ci, list(md_ci.items())),
        (multidict.MultiDictProxy(md_for_proxy), list(md_for_proxy.items())),
        (multidict.CIMultiDictProxy(md_for_ciproxy), list(md_for_ciproxy.items())),
    ]


@pytest.mark.parametrize(
    "container, expected",
    _md_for_foreach_all(),
    ids=["multidict", "cimultidict", "proxy", "ciproxy"],
)
def test_md_foreach_all(container: object, expected: list[tuple[str, str]]) -> None:
    assert _testcapi.md_foreach(container, None, -1) == expected


def test_md_foreach_all_empty() -> None:
    md: MultiDictStr = multidict.MultiDict()
    assert _testcapi.md_foreach(md, None, -1) == []


def test_md_foreach_all_early_stop() -> None:
    md: MultiDictStr = multidict.MultiDict([("a", "1"), ("b", "2"), ("c", "3")])
    assert _testcapi.md_foreach(md, None, 1) == [("a", "1")]


def test_md_foreach_key_multidict() -> None:
    md: MultiDictStr = multidict.MultiDict([("a", "1"), ("b", "2"), ("a", "3")])
    assert _testcapi.md_foreach(md, "a", -1) == [("a", "1"), ("a", "3")]
    assert [v for _, v in _testcapi.md_foreach(md, "a", -1)] == md.getall("a")


def test_md_foreach_key_cimultidict() -> None:
    md: CIMultiDictStr = multidict.CIMultiDict()
    md.add("KEY", "v1")
    md.add("key", "v2")
    assert [v for _, v in _testcapi.md_foreach(md, "Key", -1)] == md.getall("key")


def test_md_foreach_key_missing() -> None:
    md: MultiDictStr = multidict.MultiDict()
    assert _testcapi.md_foreach(md, "missing", -1) == []


def test_md_foreach_key_early_stop() -> None:
    md: MultiDictStr = multidict.MultiDict([("a", "1"), ("a", "2"), ("a", "3")])
    assert _testcapi.md_foreach(md, "a", 1) == [("a", "1")]


@pytest.mark.parametrize(
    "func, args",
    [
        (_testcapi.md_add, ("key", "value")),
        (_testcapi.md_clear, ()),
        (_testcapi.md_contains, ("key",)),
        (_testcapi.md_getitem, ("key",)),
        (_testcapi.md_setitem, ("key", "value")),
        (_testcapi.md_delitem, ("key",)),
        (_testcapi.md_pop, ("key",)),
        (_testcapi.md_setdefault, ("key", "default")),
    ],
    ids=[
        "add",
        "clear",
        "contains",
        "getitem",
        "setitem",
        "delitem",
        "pop",
        "setdefault",
    ],
)
def test_md_wrong_type(func: object, args: tuple[object, ...]) -> None:
    with pytest.raises(TypeError, match="should be a MultiDict instance"):
        func({}, *args)  # type: ignore[operator]


@pytest.mark.parametrize(
    "func, args",
    [
        (_testcapi.md_getversion, ()),
        (_testcapi.md_size, ()),
        (_testcapi.md_foreach, (None, -1)),
        (_testcapi.md_foreach, ("key", -1)),
    ],
    ids=["getversion", "size", "foreach_all", "foreach_key"],
)
def test_any_multidict_wrong_type(func: object, args: tuple[object, ...]) -> None:
    with pytest.raises(
        TypeError,
        match="should be a MultiDict, CIMultiDict, MultiDictProxy or "
        "CIMultiDictProxy instance",
    ):
        func({}, *args)  # type: ignore[operator]
