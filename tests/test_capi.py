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

if _testcyapi is not None:
    _CYTHON_PARAM = pytest.param(_testcyapi, id="cython")
else:
    _CYTHON_PARAM = pytest.param(
        None,
        id="cython",
        marks=pytest.mark.skip(
            reason="multidict._testcyapi not built (Cython not available at build time)"
        ),
    )

_API_MODULES = [pytest.param(_testcapi, id="c"), _CYTHON_PARAM]


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


def test_md_setdefault_omitted_default_inserts_none(api: object) -> None:
    # No default passes NULL through the C API, which reads it as None.
    md: multidict.MultiDict[str | None] = multidict.MultiDict()
    assert api.md_setdefault(md, "key") == (False, None)
    assert list(md.items()) == [("key", None)]


def test_md_setdefault_omitted_default_keeps_existing(api: object) -> None:
    md: MultiDictStr = multidict.MultiDict(key="value")
    assert api.md_setdefault(md, "key") == (True, "value")
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


def test_a_callback_that_unwatches_gets_no_further_events(api: object) -> None:
    # Delivery reads the watch bits afresh for each event, so a client
    # that unwatches mid-burst can free its user_data right after.
    log: list[object] = []
    watcher_id = api.md_add_unwatching_watcher(log)
    md: MultiDictStr = multidict.MultiDict([("key", "one"), ("key", "two")])
    api.md_watch(watcher_id, md, None)
    md["key"] = "three"  # BATCH_BEGIN, REPLACED, DELETED, BATCH_END
    api.md_clear_watcher(watcher_id)
    api.watch_release_refs()
    assert log == [BATCH_BEGIN]
    assert list(md.items()) == [("key", "three")]


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


# --------------------------- watchers ---------------------------

ADDED = _testcapi.MultiDict_EVENT_ADDED
REPLACED = _testcapi.MultiDict_EVENT_REPLACED
DELETED = _testcapi.MultiDict_EVENT_DELETED
CLEARED = _testcapi.MultiDict_EVENT_CLEARED
CLONED = _testcapi.MultiDict_EVENT_CLONED
DEALLOCATED = _testcapi.MultiDict_EVENT_DEALLOCATED
BATCH_BEGIN = _testcapi.MultiDict_EVENT_BATCH_BEGIN
BATCH_END = _testcapi.MultiDict_EVENT_BATCH_END
MAX_WATCHERS = _testcapi.MULTIDICT_MAX_WATCHERS

# Each recorded event is (event, self, user_data, identity, hash, key,
# value, old_value); `self` is the object except on DEALLOCATED, where it
# is its address. See record_event() in _testcapi.c.
Event = tuple[object, ...]


class Watcher:
    """One registered watcher plus the log its events land in."""

    def __init__(self, api: object, watcher_id: int, log: list[Event]) -> None:
        self.api = api
        self.id = watcher_id
        self.log = log

    def watch(self, md: object, user_data: object) -> None:
        self.api.md_watch(self.id, md, user_data)

    def unwatch(self, md: object) -> None:
        self.api.md_unwatch(self.id, md)

    def drain(self) -> list[Event]:
        """Every event so far, emptying the log.

        Draining also drops the log's own references to the watched
        multidict, which each recorded event holds; a test that wants a
        DEALLOCATED event has to drain before dropping its own reference.
        """
        events = list(self.log)
        self.log.clear()
        return events

    def kinds(self) -> list[object]:
        return [event[0] for event in self.drain()]


@pytest.fixture
def watcher(api: object) -> object:
    log: list[Event] = []
    watcher_id = api.md_add_watcher(log)
    yield Watcher(api, watcher_id, log)
    api.md_clear_watcher(watcher_id)
    api.watch_release_refs()


@pytest.fixture
def other_watcher(api: object) -> object:
    log: list[Event] = []
    watcher_id = api.md_add_watcher(log)
    yield Watcher(api, watcher_id, log)
    api.md_clear_watcher(watcher_id)
    api.watch_release_refs()


def test_add_returns_the_added_pair(watcher: Watcher) -> None:
    md: MultiDictStr = multidict.MultiDict()
    watcher.watch(md, "ctx")
    md.add("key", "value")
    assert watcher.drain() == [
        (ADDED, md, "ctx", "key", hash("key"), "key", "value", None)
    ]


def test_user_data_is_per_watched_multidict(watcher: Watcher) -> None:
    # The reason this API exists: one registered callback, many watched
    # multidicts, each carrying the context of whatever owns it.
    first: CIMultiDictStr = multidict.CIMultiDict()
    second: CIMultiDictStr = multidict.CIMultiDict()
    watcher.watch(first, "response-1")
    watcher.watch(second, "response-2")
    first.add("Content-Length", "10")
    second.add("Content-Length", "20")
    assert [(event[2], event[6]) for event in watcher.drain()] == [
        ("response-1", "10"),
        ("response-2", "20"),
    ]


def test_watcher_data_is_shared_by_every_watched_multidict(watcher: Watcher) -> None:
    # `watcher_data` is the log itself: one object, fixed at registration.
    first: MultiDictStr = multidict.MultiDict()
    second: MultiDictStr = multidict.MultiDict()
    watcher.watch(first, "a")
    watcher.watch(second, "b")
    first.add("k", "1")
    second.add("k", "2")
    assert len(watcher.log) == 2


def test_two_watchers_can_watch_one_multidict(
    watcher: Watcher, other_watcher: Watcher
) -> None:
    # The bit mask holds one slot per registered watcher, so a multidict
    # can carry all of them at once, each with its own user_data.
    md: MultiDictStr = multidict.MultiDict()
    watcher.watch(md, "first")
    other_watcher.watch(md, "second")
    md.add("key", "value")
    assert [event[2] for event in watcher.drain()] == ["first"]
    assert [event[2] for event in other_watcher.drain()] == ["second"]


def test_unwatching_one_watcher_leaves_the_other(
    watcher: Watcher, other_watcher: Watcher
) -> None:
    md: MultiDictStr = multidict.MultiDict()
    watcher.watch(md, "first")
    other_watcher.watch(md, "second")
    watcher.unwatch(md)
    md.add("key", "value")
    assert watcher.drain() == []
    assert [event[2] for event in other_watcher.drain()] == ["second"]


def test_rewatching_overwrites_user_data(watcher: Watcher) -> None:
    md: MultiDictStr = multidict.MultiDict()
    watcher.watch(md, "first")
    watcher.watch(md, "second")
    md.add("k", "v")
    assert [event[2] for event in watcher.drain()] == ["second"]


def test_identity_of_a_case_insensitive_key(watcher: Watcher) -> None:
    md: CIMultiDictStr = multidict.CIMultiDict()
    watcher.watch(md, None)
    md.add("Content-Length", "10")
    (event,) = watcher.drain()
    assert (event[3], event[5]) == ("content-length", "Content-Length")


def test_identity_of_a_case_sensitive_key(watcher: Watcher) -> None:
    md: MultiDictStr = multidict.MultiDict()
    watcher.watch(md, None)
    md.add("Content-Length", "10")
    (event,) = watcher.drain()
    assert (event[3], event[5]) == ("Content-Length", "Content-Length")


def test_hash_is_the_identity_hash(watcher: Watcher) -> None:
    # The hash multidict looked the entry up by, so on a CIMultiDict it is
    # the hash of the lowercased identity, not of the key as written.
    md: CIMultiDictStr = multidict.CIMultiDict()
    watcher.watch(md, None)
    md.add("Content-Length", "10")
    (event,) = watcher.drain()
    assert (event[4], event[5]) == (hash("content-length"), "Content-Length")


def test_an_event_without_a_key_reports_no_hash(watcher: Watcher) -> None:
    md: MultiDictStr = multidict.MultiDict([("key", "value")])
    watcher.watch(md, None)
    md.clear()
    (event,) = watcher.drain()
    assert (event[0], event[3], event[4]) == (CLEARED, None, -1)


def test_setitem_on_a_new_key_adds(watcher: Watcher) -> None:
    md: MultiDictStr = multidict.MultiDict()
    watcher.watch(md, None)
    md["key"] = "value"
    assert watcher.kinds() == [BATCH_BEGIN, ADDED, BATCH_END]


def test_setitem_replaces_the_first_and_deletes_the_rest(watcher: Watcher) -> None:
    md: MultiDictStr = multidict.MultiDict([("key", "one"), ("key", "two")])
    watcher.watch(md, None)
    md["key"] = "three"
    begin, replaced, deleted, end = watcher.drain()
    assert (begin[0], end[0]) == (BATCH_BEGIN, BATCH_END)
    assert (replaced[0], replaced[6], replaced[7]) == (REPLACED, "three", "one")
    assert (deleted[0], deleted[6], deleted[7]) == (DELETED, "two", None)


def test_delitem_removes_every_match(watcher: Watcher) -> None:
    md: MultiDictStr = multidict.MultiDict([("key", "one"), ("key", "two")])
    watcher.watch(md, None)
    del md["key"]
    assert watcher.kinds() == [BATCH_BEGIN, DELETED, DELETED, BATCH_END]


def test_delitem_of_a_missing_key_records_nothing(watcher: Watcher) -> None:
    md: MultiDictStr = multidict.MultiDict()
    watcher.watch(md, None)
    with pytest.raises(KeyError):
        del md["key"]
    assert watcher.drain() == []


def test_extend_batches_one_added_per_pair(watcher: Watcher) -> None:
    md: MultiDictStr = multidict.MultiDict()
    watcher.watch(md, None)
    md.extend([("a", "1"), ("b", "2")])
    assert watcher.kinds() == [BATCH_BEGIN, ADDED, ADDED, BATCH_END]


def test_update_replaces_the_first_match(watcher: Watcher) -> None:
    md: MultiDictStr = multidict.MultiDict([("a", "1"), ("a", "2")])
    watcher.watch(md, None)
    md.update([("a", "9")])
    assert watcher.kinds() == [BATCH_BEGIN, REPLACED, DELETED, BATCH_END]


def test_merge_only_adds_absent_keys(watcher: Watcher) -> None:
    md: MultiDictStr = multidict.MultiDict([("a", "1")])
    watcher.watch(md, None)
    md.merge([("a", "ignored"), ("b", "2")])
    assert watcher.kinds() == [BATCH_BEGIN, ADDED, BATCH_END]


def test_popall_batches_one_deleted_per_value(watcher: Watcher) -> None:
    md: MultiDictStr = multidict.MultiDict([("a", "1"), ("a", "2")])
    watcher.watch(md, None)
    assert md.popall("a") == ["1", "2"]
    assert watcher.kinds() == [BATCH_BEGIN, DELETED, DELETED, BATCH_END]


def test_popone_deletes(watcher: Watcher) -> None:
    md: MultiDictStr = multidict.MultiDict([("a", "1")])
    watcher.watch(md, None)
    assert md.popone("a") == "1"
    assert watcher.kinds() == [DELETED]


def test_popitem_deletes(watcher: Watcher) -> None:
    md: MultiDictStr = multidict.MultiDict([("a", "1")])
    watcher.watch(md, None)
    assert md.popitem() == ("a", "1")
    assert watcher.kinds() == [DELETED]


def test_setdefault_adds_only_when_absent(watcher: Watcher) -> None:
    md: MultiDictStr = multidict.MultiDict()
    watcher.watch(md, None)
    assert md.setdefault("a", "1") == "1"
    assert md.setdefault("a", "ignored") == "1"
    assert watcher.kinds() == [ADDED]


def test_clear_records_one_event_not_one_per_pair(watcher: Watcher) -> None:
    md: MultiDictStr = multidict.MultiDict([("a", "1"), ("b", "2")])
    watcher.watch(md, None)
    md.clear()
    assert watcher.kinds() == [CLEARED]


def test_clear_of_an_empty_multidict_records_nothing(watcher: Watcher) -> None:
    md: MultiDictStr = multidict.MultiDict()
    watcher.watch(md, None)
    md.clear()
    assert watcher.drain() == []


def test_init_again_clears_then_adds(watcher: Watcher) -> None:
    md: MultiDictStr = multidict.MultiDict([("a", "1")])
    watcher.watch(md, None)
    md.__init__([("b", "2")])  # type: ignore[misc]
    assert watcher.kinds() == [BATCH_BEGIN, CLEARED, ADDED, BATCH_END]


def test_init_from_the_same_type_clones(watcher: Watcher) -> None:
    md: MultiDictStr = multidict.MultiDict()
    watcher.watch(md, None)
    md.__init__(multidict.MultiDict([("a", "1")]))  # type: ignore[misc]
    assert watcher.kinds() == [CLONED]


def test_copy_records_nothing(watcher: Watcher) -> None:
    # The copy is a brand-new object, so it cannot be watched, and the
    # source is not modified.
    md: MultiDictStr = multidict.MultiDict([("a", "1")])
    watcher.watch(md, None)
    assert md.copy() == md
    assert watcher.drain() == []


def test_reading_records_nothing(watcher: Watcher) -> None:
    md: MultiDictStr = multidict.MultiDict([("a", "1")])
    watcher.watch(md, None)
    assert md["a"] == "1"
    assert list(md.items()) == [("a", "1")]
    assert "a" in md
    assert len(md) == 1
    assert watcher.drain() == []


def test_watch_through_a_proxy_watches_the_underlying_multidict(
    watcher: Watcher,
) -> None:
    md: MultiDictStr = multidict.MultiDict()
    watcher.watch(multidict.MultiDictProxy(md), "ctx")
    md.add("a", "1")
    assert [event[2] for event in watcher.drain()] == ["ctx"]


def test_unwatch_through_another_proxy_removes_the_same_watch(
    watcher: Watcher,
) -> None:
    md: MultiDictStr = multidict.MultiDict()
    watcher.watch(multidict.MultiDictProxy(md), "ctx")
    watcher.unwatch(multidict.MultiDictProxy(md))
    md.add("a", "1")
    assert watcher.drain() == []


def test_unwatch_stops_events(watcher: Watcher) -> None:
    md: MultiDictStr = multidict.MultiDict()
    watcher.watch(md, None)
    watcher.unwatch(md)
    md.add("a", "1")
    assert watcher.drain() == []


def test_unwatch_of_an_unwatched_multidict_is_a_noop(watcher: Watcher) -> None:
    md: MultiDictStr = multidict.MultiDict()
    watcher.unwatch(md)
    md.add("a", "1")
    assert watcher.drain() == []


def test_clear_watcher_stops_events(api: object, watcher: Watcher) -> None:
    # The bit stays on the multidict; it resolves to the now-empty slot
    # and is skipped, exactly like CPython's PyDict_ClearWatcher().
    md: MultiDictStr = multidict.MultiDict()
    watcher.watch(md, None)
    api.md_clear_watcher(watcher.id)
    md.add("a", "1")
    assert watcher.drain() == []
    # re-registering so the fixture's own clear has a live slot to clear
    watcher.id = api.md_add_watcher(watcher.log)


def test_dealloc_reports_the_address_not_the_object(watcher: Watcher) -> None:
    md: MultiDictStr = multidict.MultiDict()
    watcher.watch(md, "ctx")
    address = id(md)
    # each recorded event holds a reference to `md`, so drain first
    watcher.drain()
    del md
    assert watcher.drain() == [
        (DEALLOCATED, address, "ctx", None, -1, None, None, None)
    ]


def test_a_callback_may_read_the_multidict_it_watches(api: object) -> None:
    # Delivery happens after the operation, with no lock held, so this is
    # safe -- unlike a MultiDict_ForEach visitor.
    log: list[Event] = []
    watcher_id = api.md_add_watcher(log)
    md: MultiDictStr = multidict.MultiDict()
    api.md_watch(watcher_id, md, md)
    md.add("a", "1")
    seen = [api.md_size(event[2]) for event in log]
    api.md_clear_watcher(watcher_id)
    api.watch_release_refs()
    assert seen == [1]


def test_add_watcher_rejects_a_null_callback(api: object) -> None:
    with pytest.raises(ValueError, match="callback must not be NULL"):
        api.md_add_null_watcher()


def test_add_watcher_runs_out_of_slots(api: object) -> None:
    log: list[Event] = []
    ids = [api.md_add_watcher(log) for _ in range(MAX_WATCHERS)]
    try:
        assert sorted(ids) == list(range(MAX_WATCHERS))
        with pytest.raises(RuntimeError, match="no more multidict watcher IDs"):
            api.md_add_watcher(log)
    finally:
        for watcher_id in ids:
            api.md_clear_watcher(watcher_id)
        api.watch_release_refs()


@pytest.mark.parametrize("watcher_id", [-1, MAX_WATCHERS, 0])
def test_unregistered_watcher_id_is_rejected(api: object, watcher_id: int) -> None:
    md: MultiDictStr = multidict.MultiDict()
    with pytest.raises(ValueError, match="invalid watcher ID"):
        api.md_watch(watcher_id, md, None)
    with pytest.raises(ValueError, match="invalid watcher ID"):
        api.md_unwatch(watcher_id, md)
    with pytest.raises(ValueError, match="invalid watcher ID"):
        api.md_clear_watcher(watcher_id)


@pytest.mark.parametrize("name", ["md_watch", "md_unwatch"])
def test_watch_wrong_type(api: object, watcher: Watcher, name: str) -> None:
    args: tuple[object, ...] = (
        (watcher.id, {}, None) if name == "md_watch" else (watcher.id, {})
    )
    with pytest.raises(
        TypeError,
        match="should be a MultiDict, CIMultiDict, MultiDictProxy or "
        "CIMultiDictProxy instance",
    ):
        getattr(api, name)(*args)


def test_a_failing_callback_is_reported_as_unraisable(
    api: object, monkeypatch: pytest.MonkeyPatch
) -> None:
    # The mutation already happened and cannot be undone, so a failure can
    # only be reported. Same rule as CPython's dict watchers.
    unraisable: list[object] = []
    monkeypatch.setattr(sys, "unraisablehook", unraisable.append)
    log: list[Event] = []
    watcher_id = api.md_add_failing_watcher(log)
    md: MultiDictStr = multidict.MultiDict()
    api.md_watch(watcher_id, md, None)
    md.add("a", "1")
    api.md_clear_watcher(watcher_id)
    api.watch_release_refs()
    assert md["a"] == "1"  # the mutation still happened
    assert log == [None]  # the callback did run
    assert len(unraisable) == 1


def test_a_callback_may_mutate_the_multidict_it_watches(api: object) -> None:
    # The events that produces are queued and delivered to the same
    # callback before the flush returns, so the loop has to terminate.
    log: list[object] = []
    watcher_id = api.md_add_mutating_watcher(log)
    md: MultiDictStr = multidict.MultiDict()
    api.md_watch(watcher_id, md, None)
    md.add("seed", "1")
    api.md_unwatch(watcher_id, md)
    api.md_clear_watcher(watcher_id)
    api.watch_release_refs()
    assert log == [ADDED] * 3
    assert len(md) == 3


@pytest.mark.skipif(
    "free-threading" in sys.version,
    reason="set_nomemory() swaps the global allocator, which races the "
    "runtime's own threads on a free-threaded build",
)
def test_recording_out_of_memory_reports_one_lost_event(
    watcher: Watcher, monkeypatch: pytest.MonkeyPatch
) -> None:
    # Recording cannot fail the mutation it describes, so an allocation
    # failure turns the whole burst into a single "resynchronize".
    cpython_testcapi = pytest.importorskip("_testcapi")
    # The injected failure lands wherever the allocation counter says, and
    # the callback below allocates too. One that fails is reported as
    # unraisable, which is the documented behaviour but would otherwise
    # reach pytest's unraisable plugin and fail the test.
    monkeypatch.setattr(sys, "unraisablehook", lambda unraisable: None)
    # Bound to short names so the arm/call/disarm below fits one physical
    # line: split across lines, a tracer line event lands between them and
    # the armed failure hits the tracer's own allocation instead.
    nomemory = cpython_testcapi.set_nomemory
    restore = cpython_testcapi.remove_mem_hooks
    md: MultiDictStr = multidict.MultiDict()
    watcher.watch(md, None)
    # Sweep which allocation fails rather than breaking out on the first
    # hit: an early exit would leave the loop's exhausted arm untaken.
    seen: list[object] = []
    for nth in range(8):
        watcher.drain()
        try:
            nomemory(nth, nth + 1), md.add("k", "v"), restore()
        except MemoryError:
            restore()
        seen.extend(watcher.kinds())
    assert _testcapi.MultiDict_EVENT_LOST in seen


def test_a_failing_operation_keeps_its_own_exception(watcher: Watcher) -> None:
    # Delivery happens on the failure path too, so the flush must not let
    # the pending exception be mistaken for one a callback raised.
    md: MultiDictStr = multidict.MultiDict()
    watcher.watch(md, None)
    with pytest.raises(ValueError):
        md.extend([("ok", "1"), "not-a-pair"])  # type: ignore[list-item]
    # the pair that did land is still reported
    assert ADDED in watcher.kinds()


@pytest.fixture(params=[_CYTHON_PARAM])
def cy_watcher(request: pytest.FixtureRequest) -> object:
    # The Cython trampoline in multidict/__init__.pxd, which hands the
    # callback ordinary objects instead of raw PyObject pointers. It has no
    # counterpart in _testcapi.c, so it gets its own fixture rather than an
    # arm inside the shared `api` one.
    api = request.param
    log: list[Event] = []
    watcher_id = api.md_add_watcher_cy(log)
    yield Watcher(api, watcher_id, log)
    api.md_clear_watcher(watcher_id)
    api.watch_release_refs()


def test_cython_trampoline_delivers_objects(cy_watcher: Watcher) -> None:
    md: CIMultiDictStr = multidict.CIMultiDict()
    cy_watcher.watch(md, "ctx")
    md.add("Key", "value")
    assert cy_watcher.drain() == [
        (ADDED, md, "ctx", "key", hash("key"), "Key", "value", None)
    ]


def test_cython_trampoline_reports_a_raising_callback(
    api: object, monkeypatch: pytest.MonkeyPatch
) -> None:
    # Unlike the raw C callback, a Cython one may just `raise`; the
    # trampoline reinstates the exception so multidict can report it.
    cy_api = pytest.importorskip("multidict._testcyapi")
    unraisable: list[object] = []
    monkeypatch.setattr(sys, "unraisablehook", unraisable.append)
    log: list[Event] = []
    watcher_id = cy_api.md_add_failing_watcher_cy(log)
    md: MultiDictStr = multidict.MultiDict()
    cy_api.md_watch(watcher_id, md, None)
    md.add("a", "1")
    cy_api.md_clear_watcher(watcher_id)
    cy_api.watch_release_refs()
    assert md["a"] == "1"
    assert log == [None]
    assert len(unraisable) == 1
