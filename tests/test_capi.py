import importlib
import os
import sys
import types

import pytest

import multidict

_testcapi = pytest.importorskip("multidict._testcapi")

_testcyapi: types.ModuleType | None
try:
    _testcyapi = importlib.import_module("multidict._testcyapi")
except ImportError:
    _testcyapi = None

if _testcyapi is None and os.environ.get(
    "MULTIDICT_TEST_REQUIRE_CYAPI"
):  # pragma: no cover
    raise RuntimeError(
        "multidict._testcyapi did not import, but MULTIDICT_TEST_REQUIRE_CYAPI "
        "is set: the Cython opt-in build was expected to succeed"
    )

pytestmark = pytest.mark.capi

MultiDictStr = multidict.MultiDict[str]
CIMultiDictStr = multidict.CIMultiDict[str]

_API_MODULES = [pytest.param(_testcapi, id="c")]
if _testcyapi is not None:
    _API_MODULES.append(pytest.param(_testcyapi, id="cython"))
else:
    _API_MODULES.append(
        pytest.param(
            None,
            id="cython",
            marks=pytest.mark.skip(
                reason="multidict._testcyapi not built (Cython not available at build time)"
            ),
        )
    )


@pytest.fixture(params=_API_MODULES)
def api(request: pytest.FixtureRequest) -> object:
    return request.param


@pytest.mark.parametrize(
    "name, cls",
    [
        ("md_type", multidict.MultiDict),
        ("cimd_type", multidict.CIMultiDict),
        ("mdproxy_type", multidict.MultiDictProxy),
        ("cimdproxy_type", multidict.CIMultiDictProxy),
    ],
    ids=["multidict", "cimultidict", "multidict_proxy", "cimultidict_proxy"],
)
def test_get_type(api: object, name: str, cls: object) -> None:
    getter = getattr(api, name)
    assert getter() is cls
    before = sys.getrefcount(cls)
    for _ in range(2000):
        getter()
    assert sys.getrefcount(cls) == before


def test_md_new(api: object) -> None:
    md = api.md_new(0)
    assert isinstance(md, multidict.MultiDict)
    assert not isinstance(md, multidict.CIMultiDict)
    assert len(md) == 0


def test_cimd_new(api: object) -> None:
    md = api.cimd_new(0)
    assert isinstance(md, multidict.CIMultiDict)
    assert len(md) == 0


def test_md_add_multidict(api: object) -> None:
    md: MultiDictStr = multidict.MultiDict()
    api.md_add(md, "key", "value")
    api.md_add(md, "key", "value2")
    assert list(md.items()) == [("key", "value"), ("key", "value2")]


def test_md_add_cimultidict(api: object) -> None:
    # There is no separate CIMultiDict_Add: MultiDict_Add works against a
    # CIMultiDict instance too, since it is a MultiDict subclass.
    md: CIMultiDictStr = multidict.CIMultiDict()
    api.md_add(md, "KEY", "value")
    assert list(md.items()) == [("KEY", "value")]
    assert md["key"] == "value"


@pytest.mark.parametrize(
    "md",
    [multidict.MultiDict(key="value"), multidict.CIMultiDict(key="value")],
    ids=["multidict", "cimultidict"],
)
def test_md_clear(api: object, md: object) -> None:
    # MultiDict_Clear also works against a CIMultiDict, same as MultiDict_Add.
    api.md_clear(md)
    assert len(md) == 0  # type: ignore[arg-type]


def test_mdproxy_new_from_multidict(api: object) -> None:
    md: MultiDictStr = multidict.MultiDict(key="value")
    proxy = api.mdproxy_new(md)
    assert isinstance(proxy, multidict.MultiDictProxy)
    assert not isinstance(proxy, multidict.CIMultiDictProxy)
    assert proxy["key"] == "value"
    md["key"] = "other"
    assert proxy["key"] == "other"


def test_mdproxy_new_from_cimultidict(api: object) -> None:
    # MultiDictProxy_New also accepts a CIMultiDict, same as the Python
    # level MultiDictProxy(CIMultiDict()) constructor call.
    md: CIMultiDictStr = multidict.CIMultiDict(KEY="value")
    proxy = api.mdproxy_new(md)
    assert isinstance(proxy, multidict.MultiDictProxy)
    assert proxy["key"] == "value"


def test_mdproxy_new_from_proxy(api: object) -> None:
    md: MultiDictStr = multidict.MultiDict(key="value")
    proxy = api.mdproxy_new(multidict.MultiDictProxy(md))
    assert isinstance(proxy, multidict.MultiDictProxy)
    assert proxy["key"] == "value"


def test_mdproxy_new_wrong_type(api: object) -> None:
    with pytest.raises(
        TypeError, match="requires a MultiDict or MultiDictProxy instance"
    ):
        api.mdproxy_new({})


def test_cimdproxy_new_from_cimultidict(api: object) -> None:
    md: CIMultiDictStr = multidict.CIMultiDict(KEY="value")
    proxy = api.cimdproxy_new(md)
    assert isinstance(proxy, multidict.CIMultiDictProxy)
    assert proxy["key"] == "value"
    md["key"] = "other"
    assert proxy["key"] == "other"


def test_cimdproxy_new_from_proxy(api: object) -> None:
    md: CIMultiDictStr = multidict.CIMultiDict(key="value")
    proxy = api.cimdproxy_new(multidict.CIMultiDictProxy(md))
    assert isinstance(proxy, multidict.CIMultiDictProxy)
    assert proxy["key"] == "value"


def test_cimdproxy_new_rejects_plain_multidict(api: object) -> None:
    # Unlike MultiDictProxy_New, CIMultiDictProxy_New does not accept a
    # plain (non-case-insensitive) MultiDict instance.
    md: MultiDictStr = multidict.MultiDict(key="value")
    with pytest.raises(
        TypeError, match="requires a CIMultiDict or CIMultiDictProxy instance"
    ):
        api.cimdproxy_new(md)


def test_cimdproxy_new_wrong_type(api: object) -> None:
    with pytest.raises(
        TypeError, match="requires a CIMultiDict or CIMultiDictProxy instance"
    ):
        api.cimdproxy_new({})


def test_md_getversion_multidict(api: object) -> None:
    md: multidict.MultiDict[object] = multidict.MultiDict()
    version = api.md_getversion(md)
    assert version == multidict.getversion(md)
    md["key"] = "value"
    assert api.md_getversion(md) != version
    assert api.md_getversion(md) == multidict.getversion(md)


def test_md_getversion_cimultidict(api: object) -> None:
    md: multidict.CIMultiDict[object] = multidict.CIMultiDict(key="value")
    assert api.md_getversion(md) == multidict.getversion(md)


def test_md_getversion_proxy(api: object) -> None:
    md: MultiDictStr = multidict.MultiDict(key="value")
    proxy = multidict.MultiDictProxy(md)
    assert api.md_getversion(proxy) == api.md_getversion(md)
    md["key2"] = "value2"
    assert api.md_getversion(proxy) == api.md_getversion(md)


def test_md_getversion_ciproxy(api: object) -> None:
    md: CIMultiDictStr = multidict.CIMultiDict(key="value")
    proxy = multidict.CIMultiDictProxy(md)
    assert api.md_getversion(proxy) == api.md_getversion(md)


def test_md_contains_multidict(api: object) -> None:
    md: MultiDictStr = multidict.MultiDict(key="value")
    assert api.md_contains(md, "key") is True
    assert api.md_contains(md, "missing") is False


def test_md_contains_cimultidict(api: object) -> None:
    md: CIMultiDictStr = multidict.CIMultiDict(KEY="value")
    assert api.md_contains(md, "key") is True


def test_md_contains_proxy(api: object) -> None:
    md: MultiDictStr = multidict.MultiDict(key="value")
    proxy = multidict.MultiDictProxy(md)
    assert api.md_contains(proxy, "key") is True
    assert api.md_contains(proxy, "missing") is False


def test_md_contains_ciproxy(api: object) -> None:
    md: CIMultiDictStr = multidict.CIMultiDict(KEY="value")
    proxy = multidict.CIMultiDictProxy(md)
    assert api.md_contains(proxy, "key") is True


def test_md_getitem_multidict(api: object) -> None:
    md: MultiDictStr = multidict.MultiDict([("key", "value1"), ("key", "value2")])
    assert api.md_getitem(md, "key") == (True, "value1")


def test_md_getitem_cimultidict(api: object) -> None:
    md: CIMultiDictStr = multidict.CIMultiDict(KEY="value")
    assert api.md_getitem(md, "key") == (True, "value")


def test_md_getitem_proxy(api: object) -> None:
    md: MultiDictStr = multidict.MultiDict([("key", "value1"), ("key", "value2")])
    proxy = multidict.MultiDictProxy(md)
    assert api.md_getitem(proxy, "key") == (True, "value1")


def test_md_getitem_ciproxy(api: object) -> None:
    md: CIMultiDictStr = multidict.CIMultiDict(KEY="value")
    proxy = multidict.CIMultiDictProxy(md)
    assert api.md_getitem(proxy, "key") == (True, "value")


def test_md_getitem_missing(api: object) -> None:
    # Unlike PyObject *self[key], this reports "not found" via the return
    # code rather than raising KeyError -- the PyDict_GetItemRef design.
    md: MultiDictStr = multidict.MultiDict()
    assert api.md_getitem(md, "missing") == (False, None)


def test_md_setitem_multidict_replaces_all(api: object) -> None:
    md: MultiDictStr = multidict.MultiDict([("key", "value1"), ("key", "value2")])
    api.md_setitem(md, "key", "new")
    assert list(md.items()) == [("key", "new")]


def test_md_setitem_multidict_adds_missing(api: object) -> None:
    md: MultiDictStr = multidict.MultiDict()
    api.md_setitem(md, "key", "value")
    assert list(md.items()) == [("key", "value")]


def test_md_setitem_cimultidict(api: object) -> None:
    md: CIMultiDictStr = multidict.CIMultiDict()
    api.md_setitem(md, "KEY", "value")
    assert md["key"] == "value"


def test_md_delitem_multidict_removes_all(api: object) -> None:
    md: MultiDictStr = multidict.MultiDict([("key", "value1"), ("key", "value2")])
    api.md_delitem(md, "key")
    assert "key" not in md


def test_md_delitem_cimultidict(api: object) -> None:
    md: CIMultiDictStr = multidict.CIMultiDict(KEY="value")
    api.md_delitem(md, "key")
    assert len(md) == 0


def test_md_delitem_missing(api: object) -> None:
    md: MultiDictStr = multidict.MultiDict()
    with pytest.raises(KeyError, match="missing"):
        api.md_delitem(md, "missing")


def test_md_pop_multidict(api: object) -> None:
    md: MultiDictStr = multidict.MultiDict([("key", "value1"), ("key", "value2")])
    assert api.md_pop(md, "key") == (True, "value1")
    assert list(md.items()) == [("key", "value2")]


def test_md_pop_cimultidict(api: object) -> None:
    md: CIMultiDictStr = multidict.CIMultiDict(KEY="value")
    assert api.md_pop(md, "key") == (True, "value")
    assert len(md) == 0


def test_md_pop_missing(api: object) -> None:
    # Same PyDict_GetItemRef-style design as md_getitem: "not found" is a
    # return code, not a KeyError.
    md: MultiDictStr = multidict.MultiDict()
    assert api.md_pop(md, "missing") == (False, None)


def test_md_setdefault_adds_missing(api: object) -> None:
    # PyDict_SetDefaultRef design: the bool reports whether the key was
    # already present (it was not, so `default` got inserted).
    md: MultiDictStr = multidict.MultiDict()
    assert api.md_setdefault(md, "key", "default") == (False, "default")
    assert list(md.items()) == [("key", "default")]


def test_md_setdefault_keeps_existing(api: object) -> None:
    md: MultiDictStr = multidict.MultiDict(key="value")
    assert api.md_setdefault(md, "key", "default") == (True, "value")
    assert list(md.items()) == [("key", "value")]


def test_md_setdefault_cimultidict(api: object) -> None:
    md: CIMultiDictStr = multidict.CIMultiDict()
    assert api.md_setdefault(md, "KEY", "v1") == (False, "v1")
    assert api.md_setdefault(md, "key", "v2") == (True, "v1")


def test_istr_type(api: object) -> None:
    istr_cls = multidict.istr
    assert api.istr_type() is istr_cls
    before = sys.getrefcount(istr_cls)
    for _ in range(2000):
        api.istr_type()
    assert sys.getrefcount(istr_cls) == before


def test_istr_from_unicode(api: object) -> None:
    s = api.istr_from_unicode("Header")
    assert isinstance(s, multidict.istr)
    assert s == "Header"
    assert s.lower() == "header"


def test_istr_from_unicode_passthrough(api: object) -> None:
    # Matches istr(existing_istr): the same object is returned, not a copy.
    s1 = multidict.istr("Header")
    s2 = api.istr_from_unicode(s1)
    assert s2 is s1


def test_istr_from_unicode_wrong_type(api: object) -> None:
    with pytest.raises(TypeError, match="should be a str instance"):
        api.istr_from_unicode(123)


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
def test_md_size(api: object, md: object, expected: int) -> None:
    assert api.md_size(md) == expected


def test_md_size_proxy_reflects_live_changes(api: object) -> None:
    md: MultiDictStr = multidict.MultiDict(key="value")
    proxy = multidict.MultiDictProxy(md)
    assert api.md_size(proxy) == 1
    md["key2"] = "value2"
    assert api.md_size(proxy) == 2


# The visitor is handed (identity, hash, key, value); for a CIMultiDict
# the identity is the lower-cased key, so it differs from the key itself,
# and so does its hash.
def _md_for_foreach_all() -> list[tuple[object, list[tuple[str, int, str, str]]]]:
    md_plain: MultiDictStr = multidict.MultiDict([("a", "1"), ("b", "2"), ("a", "3")])
    md_ci: CIMultiDictStr = multidict.CIMultiDict(KEY="value")
    md_for_proxy: MultiDictStr = multidict.MultiDict([("a", "1"), ("a", "2")])
    md_for_ciproxy: CIMultiDictStr = multidict.CIMultiDict(key="value")
    return [
        (
            md_plain,
            [
                ("a", hash("a"), "a", "1"),
                ("b", hash("b"), "b", "2"),
                ("a", hash("a"), "a", "3"),
            ],
        ),
        (md_ci, [("key", hash("key"), "KEY", "value")]),
        (
            multidict.MultiDictProxy(md_for_proxy),
            [("a", hash("a"), "a", "1"), ("a", hash("a"), "a", "2")],
        ),
        (
            multidict.CIMultiDictProxy(md_for_ciproxy),
            [("key", hash("key"), "key", "value")],
        ),
    ]


@pytest.mark.parametrize(
    "container, expected",
    _md_for_foreach_all(),
    ids=["multidict", "cimultidict", "proxy", "ciproxy"],
)
def test_md_foreach_all(
    api: object, container: object, expected: list[tuple[str, int, str, str]]
) -> None:
    assert api.md_foreach(container, None, -1) == expected


def test_md_foreach_all_empty(api: object) -> None:
    md: MultiDictStr = multidict.MultiDict()
    assert api.md_foreach(md, None, -1) == []


def test_md_foreach_all_early_stop(api: object) -> None:
    md: MultiDictStr = multidict.MultiDict([("a", "1"), ("b", "2"), ("c", "3")])
    assert api.md_foreach(md, None, 1) == [("a", hash("a"), "a", "1")]


def test_md_foreach_key_multidict(api: object) -> None:
    md: MultiDictStr = multidict.MultiDict([("a", "1"), ("b", "2"), ("a", "3")])
    assert api.md_foreach(md, "a", -1) == [
        ("a", hash("a"), "a", "1"),
        ("a", hash("a"), "a", "3"),
    ]
    assert [v for *_, v in api.md_foreach(md, "a", -1)] == md.getall("a")


def test_md_foreach_key_cimultidict(api: object) -> None:
    md: CIMultiDictStr = multidict.CIMultiDict()
    md.add("KEY", "v1")
    md.add("key", "v2")
    # The identity is the lower-cased lookup key, the same for both entries
    # and distinct from the "KEY" one of them was added under; the hash is
    # that identity's, not the "Key" the walk was asked for.
    assert api.md_foreach(md, "Key", -1) == [
        ("key", hash("key"), "KEY", "v1"),
        ("key", hash("key"), "key", "v2"),
    ]
    assert [v for *_, v in api.md_foreach(md, "Key", -1)] == md.getall("key")


def test_md_foreach_key_missing(api: object) -> None:
    md: MultiDictStr = multidict.MultiDict()
    assert api.md_foreach(md, "missing", -1) == []


def test_md_foreach_key_early_stop(api: object) -> None:
    md: MultiDictStr = multidict.MultiDict([("a", "1"), ("a", "2"), ("a", "3")])
    assert api.md_foreach(md, "a", 1) == [("a", hash("a"), "a", "1")]


@pytest.mark.parametrize("key", [None, "a"], ids=["all", "key"])
def test_md_foreach_mutating_visitor(api: object, key: str | None) -> None:
    md: MultiDictStr = multidict.MultiDict([("a", "1"), ("b", "2")])
    with pytest.raises(RuntimeError, match="changed during iteration"):
        api.md_foreach_mutates(md, key, "c")


def test_md_foreach_raises(api: object) -> None:
    md: MultiDictStr = multidict.MultiDict([("a", "1"), ("b", "2")])
    with pytest.raises(RuntimeError, match="boom from visitor"):
        api.md_foreach_raises(md)


@pytest.mark.skipif(
    _testcyapi is None,
    reason="multidict._testcyapi not built (Cython not available at build time)",
)
def test_md_foreach_cy_all() -> None:
    assert _testcyapi is not None
    md: MultiDictStr = multidict.MultiDict([("a", "1"), ("b", "2"), ("a", "3")])
    assert _testcyapi.md_foreach_cy(md, None, -1) == [
        ("a", hash("a"), "a", "1"),
        ("b", hash("b"), "b", "2"),
        ("a", hash("a"), "a", "3"),
    ]


@pytest.mark.skipif(
    _testcyapi is None,
    reason="multidict._testcyapi not built (Cython not available at build time)",
)
def test_md_foreach_cy_key() -> None:
    assert _testcyapi is not None
    md: MultiDictStr = multidict.MultiDict([("a", "1"), ("b", "2"), ("a", "3")])
    assert _testcyapi.md_foreach_cy(md, "a", -1) == [
        ("a", hash("a"), "a", "1"),
        ("a", hash("a"), "a", "3"),
    ]


@pytest.mark.skipif(
    _testcyapi is None,
    reason="multidict._testcyapi not built (Cython not available at build time)",
)
def test_md_foreach_cy_all_cimultidict() -> None:
    assert _testcyapi is not None
    md: CIMultiDictStr = multidict.CIMultiDict(KEY="value")
    assert _testcyapi.md_foreach_cy(md, None, -1) == [
        ("key", hash("key"), "KEY", "value")
    ]


@pytest.mark.skipif(
    _testcyapi is None,
    reason="multidict._testcyapi not built (Cython not available at build time)",
)
def test_md_foreach_cy_key_cimultidict() -> None:
    assert _testcyapi is not None
    md: CIMultiDictStr = multidict.CIMultiDict()
    md.add("KEY", "v1")
    md.add("key", "v2")
    assert _testcyapi.md_foreach_cy(md, "Key", -1) == [
        ("key", hash("key"), "KEY", "v1"),
        ("key", hash("key"), "key", "v2"),
    ]


@pytest.mark.skipif(
    _testcyapi is None,
    reason="multidict._testcyapi not built (Cython not available at build time)",
)
def test_md_foreach_cy_early_stop() -> None:
    assert _testcyapi is not None
    md: MultiDictStr = multidict.MultiDict([("a", "1"), ("b", "2"), ("c", "3")])
    assert _testcyapi.md_foreach_cy(md, None, 1) == [("a", hash("a"), "a", "1")]


@pytest.mark.skipif(
    _testcyapi is None,
    reason="multidict._testcyapi not built (Cython not available at build time)",
)
def test_md_foreach_cy_raises() -> None:
    assert _testcyapi is not None
    md: MultiDictStr = multidict.MultiDict([("a", "1"), ("b", "2")])
    with pytest.raises(RuntimeError, match="boom from cy visitor"):
        _testcyapi.md_foreach_cy_raises(md)


@pytest.mark.parametrize(
    "name, args",
    [
        ("md_add", ("key", "value")),
        ("md_clear", ()),
        ("md_setitem", ("key", "value")),
        ("md_delitem", ("key",)),
        ("md_pop", ("key",)),
        ("md_setdefault", ("key", "default")),
    ],
    ids=[
        "add",
        "clear",
        "setitem",
        "delitem",
        "pop",
        "setdefault",
    ],
)
def test_md_wrong_type(api: object, name: str, args: tuple[object, ...]) -> None:
    with pytest.raises(TypeError, match="should be a MultiDict instance"):
        getattr(api, name)({}, *args)


@pytest.mark.parametrize(
    "name, args",
    [
        ("md_add", ("key", "value")),
        ("md_clear", ()),
        ("md_setitem", ("key", "value")),
        ("md_delitem", ("key",)),
        ("md_pop", ("key",)),
        ("md_setdefault", ("key", "default")),
    ],
    ids=[
        "add",
        "clear",
        "setitem",
        "delitem",
        "pop",
        "setdefault",
    ],
)
def test_md_mutator_rejects_proxy(
    api: object, name: str, args: tuple[object, ...]
) -> None:
    # Proxies expose no mutating methods at the Python level either, so
    # there is nothing to mutate through -- unlike the readers, which
    # accept a MultiDict, CIMultiDict, MultiDictProxy or CIMultiDictProxy
    # alike (see test_any_multidict_wrong_type below).
    md: MultiDictStr = multidict.MultiDict()
    proxy = multidict.MultiDictProxy(md)
    with pytest.raises(TypeError, match="should be a MultiDict instance"):
        getattr(api, name)(proxy, *args)


@pytest.mark.parametrize(
    "name, args",
    [
        ("md_getversion", ()),
        ("md_size", ()),
        ("md_contains", ("key",)),
        ("md_getitem", ("key",)),
        ("md_foreach", (None, -1)),
        ("md_foreach", ("key", -1)),
    ],
    ids=["getversion", "size", "contains", "getitem", "foreach_all", "foreach_key"],
)
def test_any_multidict_wrong_type(
    api: object, name: str, args: tuple[object, ...]
) -> None:
    with pytest.raises(
        TypeError,
        match="should be a MultiDict, CIMultiDict, MultiDictProxy or "
        "CIMultiDictProxy instance",
    ):
        getattr(api, name)({}, *args)


def test_check_api_version_accepts_current_and_newer() -> None:
    # Not exercised through the `api` fixture: this checks the raw C
    # struct-versioning guard in multidict_capi.h directly, which has no
    # Cython-side counterpart to mirror.
    _testcapi.check_api_version(1)
    _testcapi.check_api_version(2)


def test_check_api_version_rejects_older() -> None:
    with pytest.raises(RuntimeError, match="C API version mismatch"):
        _testcapi.check_api_version(0)
