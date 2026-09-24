# cython: freethreading_compatible=True

# Exercises the public C API capsule from the test suite, from Cython. Not
# part of the public API and not meant to be imported or relied on outside
# tests. Mirrors _testcapi.c function-for-function.

from cpython.exc cimport PyErr_SetObject, PyErr_SetString
from cpython.object cimport PyObject

from multidict cimport (
    MultiDict_CAPI,
    MultiDict_GetCAPI,
    IStr_GetType,
    IStr_FromUnicode,
    MultiDict_GetVersion,
    MultiDict_GetType,
    CIMultiDict_GetType,
    MultiDictProxy_GetType,
    CIMultiDictProxy_GetType,
    MultiDict_New,
    CIMultiDict_New,
    MultiDictProxy_New,
    CIMultiDictProxy_New,
    MultiDict_Size,
    MultiDict_Contains,
    MultiDict_GetItem,
    MultiDict_Add,
    MultiDict_Clear,
    MultiDict_DelItem,
    MultiDict_Pop,
    MultiDict_SetDefault,
    MultiDict_SetItem,
    MultiDict_ForEach,
    MultiDict_ForEachAll,
    MultiDict_ForEachKey,
)

cdef MultiDict_CAPI *_capi = MultiDict_GetCAPI()


def istr_type():
    return IStr_GetType(_capi)


def istr_from_unicode(s):
    return IStr_FromUnicode(_capi, s)


def md_getversion(md):
    return MultiDict_GetVersion(_capi, md)


def md_type():
    return MultiDict_GetType(_capi)


def cimd_type():
    return CIMultiDict_GetType(_capi)


def mdproxy_type():
    return MultiDictProxy_GetType(_capi)


def cimdproxy_type():
    return CIMultiDictProxy_GetType(_capi)


def md_new(Py_ssize_t prealloc_size):
    return MultiDict_New(_capi, prealloc_size)


def cimd_new(Py_ssize_t prealloc_size):
    return CIMultiDict_New(_capi, prealloc_size)


def mdproxy_new(arg):
    return MultiDictProxy_New(_capi, arg)


def cimdproxy_new(arg):
    return CIMultiDictProxy_New(_capi, arg)


def md_size(md):
    return MultiDict_Size(_capi, md)


def md_contains(md, key):
    return bool(MultiDict_Contains(_capi, md, key))


def md_getitem(md, key):
    return MultiDict_GetItem(_capi, md, key)


def md_add(md, key, value):
    MultiDict_Add(_capi, md, key, value)


def md_clear(md):
    MultiDict_Clear(_capi, md)


def md_delitem(md, key):
    MultiDict_DelItem(_capi, md, key)


def md_pop(md, key):
    return MultiDict_Pop(_capi, md, key)


def md_setdefault(md, key, default):
    return MultiDict_SetDefault(_capi, md, key, default)


def md_setitem(md, key, value):
    MultiDict_SetItem(_capi, md, key, value)


ctypedef struct _ForeachCtx:
    PyObject *list  # borrowed: kept alive by md_foreach's own local `result`
    Py_ssize_t limit  # < 0 means no limit


cdef int _collect_pair(void *user_data, PyObject *key, PyObject *value) noexcept:
    cdef _ForeachCtx *ctx = <_ForeachCtx*>user_data
    # `ctx.list` is borrowed, not adopted: since this cast local isn't
    # returned, Cython's normal scope-exit cleanup decrefs it once, exactly
    # cancelling the <object> cast's own incref -- no manual Py_DECREF here
    # (unlike multidict/__init__.pxd's _steal(), which returns its local
    # and so needs one).
    cdef object result = <object>ctx.list
    result.append((<object>key, <object>value))
    if ctx.limit >= 0 and len(result) >= ctx.limit:
        return 0
    return 1


def md_foreach(md, key, Py_ssize_t limit):
    result = []
    cdef _ForeachCtx ctx
    ctx.list = <PyObject*>result
    ctx.limit = limit
    cdef PyObject *key_ptr = NULL
    if key is not None:
        key_ptr = <PyObject*>key
    MultiDict_ForEach(_capi, md, key_ptr, _collect_pair, &ctx)
    return result


cdef struct _MutateCtx:
    PyObject *md      # borrowed: kept alive by md_foreach_mutates' argument
    PyObject *added   # borrowed, likewise; never the walked key


cdef int _mutating_visitor(void *user_data, PyObject *key, PyObject *value) noexcept:
    # Mutates the multidict being walked, which the walk must refuse. A
    # noexcept callback cannot let MultiDict_Add()'s `except -1` propagate,
    # so report the failure through the return value (see docs/cyapi.rst).
    cdef _MutateCtx *ctx = <_MutateCtx*>user_data
    cdef object md = <object>ctx.md
    cdef object added = <object>ctx.added
    try:
        MultiDict_Add(_capi, md, added, added)
    except BaseException as exc:
        # MultiDict_Add() is declared `except -1`, but this callback is
        # noexcept, so its exception has to be caught here. Put it back
        # before returning: leaving the except block clears it, and the
        # negative return would then reach MultiDict_ForEach()'s caller
        # with nothing set (see docs/cyapi.rst).
        PyErr_SetObject(type(exc), exc)
        return -1
    return 1


def md_foreach_mutates(md, key, added):
    cdef _MutateCtx ctx
    ctx.md = <PyObject*>md
    ctx.added = <PyObject*>added
    cdef PyObject *key_ptr = NULL
    if key is not None:
        key_ptr = <PyObject*>key
    MultiDict_ForEach(_capi, md, key_ptr, _mutating_visitor, &ctx)


cdef int _raising_visitor(void *user_data, PyObject *key, PyObject *value) noexcept:
    # A noexcept callback can't just `raise`: Cython would treat that as an
    # unraisable exception here and clear it instead of propagating it. Set
    # the exception state directly and report the negative-return contract
    # ourselves (see docs/cyapi.rst).
    PyErr_SetString(RuntimeError, "boom from visitor")
    return -1


def md_foreach_raises(md):
    MultiDict_ForEach(_capi, md, NULL, _raising_visitor, NULL)


cdef int _collect_pair_cy(object key, object value, void *user_data) except -1:
    cdef _ForeachCtx *ctx = <_ForeachCtx*>user_data
    cdef object result = <object>ctx.list
    result.append((key, value))
    if ctx.limit >= 0 and len(result) >= ctx.limit:
        return 0
    return 1


def md_foreach_cy(md, key, Py_ssize_t limit):
    result = []
    cdef _ForeachCtx ctx
    ctx.list = <PyObject*>result
    ctx.limit = limit
    if key is None:
        MultiDict_ForEachAll(_capi, md, _collect_pair_cy, &ctx)
    else:
        MultiDict_ForEachKey(_capi, md, key, _collect_pair_cy, &ctx)
    return result


cdef int _raising_visitor_cy(object key, object value, void *user_data) except -1:
    raise RuntimeError("boom from cy visitor")


def md_foreach_cy_raises(md):
    MultiDict_ForEachAll(_capi, md, _raising_visitor_cy, NULL)
