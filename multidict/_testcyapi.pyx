# Exercises the public C API capsule from the test suite, from Cython. Not
# part of the public API and not meant to be imported or relied on outside
# tests. Mirrors _testcapi.c function-for-function.

from cpython.object cimport PyObject
from cpython.ref cimport Py_DECREF

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
    MultiDict_ForEachAll,
    MultiDict_ForEachKey,
)

cdef MultiDict_CAPI *_capi = MultiDict_GetCAPI()


cdef inline object _steal(PyObject *ptr):
    # Adopts a NEW reference from a raw PyObject* into an ordinary,
    # correctly-refcounted Cython object: the <object> cast increfs
    # (Cython always brackets object-typed values this way), so this one
    # compensating Py_DECREF cancels exactly that, leaving the object
    # carrying only the reference it already had.
    cdef object obj = <object>ptr
    Py_DECREF(obj)
    return obj


def istr_type():
    return _steal(<PyObject*>IStr_GetType(_capi))


def istr_from_unicode(s):
    return IStr_FromUnicode(_capi, s)


def md_getversion(md):
    return MultiDict_GetVersion(_capi, md)


def md_type():
    return _steal(<PyObject*>MultiDict_GetType(_capi))


def cimd_type():
    return _steal(<PyObject*>CIMultiDict_GetType(_capi))


def mdproxy_type():
    return _steal(<PyObject*>MultiDictProxy_GetType(_capi))


def cimdproxy_type():
    return _steal(<PyObject*>CIMultiDictProxy_GetType(_capi))


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


# Both elements of the returned tuple mirror PyDict_GetItemRef /
# PyDict_SetDefaultRef's `int` return plus `PyObject **result` design:
# (found, value_or_None). Branch on `result` being NULL, not on `found`:
# for GetItem/Pop, found == 0 implies result == NULL, but SetDefault always
# sets *result to a new reference (the existing value if found, the freshly
# inserted default otherwise) even when found == 0.
cdef _handle_result(int found, PyObject *result):
    if result == NULL:
        return (bool(found), None)
    return (bool(found), _steal(result))


def md_getitem(md, key):
    cdef PyObject *result = NULL
    cdef int found = MultiDict_GetItem(_capi, md, key, &result)
    return _handle_result(found, result)


def md_add(md, key, value):
    MultiDict_Add(_capi, md, key, value)


def md_clear(md):
    MultiDict_Clear(_capi, md)


def md_delitem(md, key):
    MultiDict_DelItem(_capi, md, key)


def md_pop(md, key):
    cdef PyObject *result = NULL
    cdef int found = MultiDict_Pop(_capi, md, key, &result)
    return _handle_result(found, result)


def md_setdefault(md, key, default):
    cdef PyObject *result = NULL
    cdef int found = MultiDict_SetDefault(_capi, md, key, default, &result)
    return _handle_result(found, result)


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
    # (unlike _steal(), which returns its local and so needs one).
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
    if key is None:
        MultiDict_ForEachAll(_capi, md, _collect_pair, &ctx)
    else:
        MultiDict_ForEachKey(_capi, md, key, _collect_pair, &ctx)
    return result
