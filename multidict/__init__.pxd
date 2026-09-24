from cpython.exc cimport PyErr_SetObject
from cpython.object cimport PyObject
from cpython.ref cimport Py_DECREF, PyTypeObject
from libc.stdint cimport uint64_t


cdef extern from "multidict_capi_struct.h":
    ctypedef struct MultiDict_CAPI:
        pass

    ctypedef int (*MultiDict_ItemVisitor)(void *user_data, PyObject *identity,
                                          Py_hash_t hash, PyObject *key,
                                          PyObject *value) noexcept


cdef extern from "multidict_capi.h":
    MultiDict_CAPI *MultiDict_GetCAPI() except NULL

    # istr
    PyTypeObject *_IStr_GetType "IStr_GetType" (MultiDict_CAPI *capi)
    bint IStr_CheckExact(MultiDict_CAPI *capi, object op)
    bint IStr_Check(MultiDict_CAPI *capi, object op)
    object IStr_FromUnicode(MultiDict_CAPI *capi, object s)

    # version
    uint64_t MultiDict_GetVersion(MultiDict_CAPI *capi, object self) except? 0

    # type objects and checks
    PyTypeObject *_MultiDict_GetType "MultiDict_GetType" (MultiDict_CAPI *capi)
    bint MultiDict_CheckExact(MultiDict_CAPI *capi, object op)
    bint MultiDict_Check(MultiDict_CAPI *capi, object op)
    PyTypeObject *_CIMultiDict_GetType "CIMultiDict_GetType" (MultiDict_CAPI *capi)
    bint CIMultiDict_CheckExact(MultiDict_CAPI *capi, object op)
    bint CIMultiDict_Check(MultiDict_CAPI *capi, object op)
    PyTypeObject *_MultiDictProxy_GetType "MultiDictProxy_GetType" (MultiDict_CAPI *capi)
    bint MultiDictProxy_CheckExact(MultiDict_CAPI *capi, object op)
    bint MultiDictProxy_Check(MultiDict_CAPI *capi, object op)
    PyTypeObject *_CIMultiDictProxy_GetType "CIMultiDictProxy_GetType" (MultiDict_CAPI *capi)
    bint CIMultiDictProxy_CheckExact(MultiDict_CAPI *capi, object op)
    bint CIMultiDictProxy_Check(MultiDict_CAPI *capi, object op)

    # constructors
    object MultiDict_New(MultiDict_CAPI *capi, Py_ssize_t prealloc_size)
    object CIMultiDict_New(MultiDict_CAPI *capi, Py_ssize_t prealloc_size)
    object MultiDictProxy_New(MultiDict_CAPI *capi, object arg)
    object CIMultiDictProxy_New(MultiDict_CAPI *capi, object arg)

    # getters
    Py_ssize_t MultiDict_Size(MultiDict_CAPI *capi, object self) except -1
    int MultiDict_Contains(MultiDict_CAPI *capi, object self, object key) except -1
    int _MultiDict_GetItem "MultiDict_GetItem" (MultiDict_CAPI *capi, object self, object key,
                          PyObject **result) except -1

    # setters
    int MultiDict_Add(MultiDict_CAPI *capi, object self, object key, object value) except -1
    int MultiDict_Clear(MultiDict_CAPI *capi, object self) except -1
    int MultiDict_DelItem(MultiDict_CAPI *capi, object self, object key) except -1
    int _MultiDict_Pop "MultiDict_Pop" (MultiDict_CAPI *capi, object self, object key,
                      PyObject **result) except -1
    int _MultiDict_SetDefault "MultiDict_SetDefault" (MultiDict_CAPI *capi, object self, object key,
                             object default_value, PyObject **result) except -1
    int MultiDict_SetItem(MultiDict_CAPI *capi, object self, object key, object value) except -1

    # iteration
    Py_ssize_t MultiDict_ForEach(MultiDict_CAPI *capi, object self, PyObject *key,
                                 MultiDict_ItemVisitor visitor, void *user_data) except -1


# Adopts a NEW reference from a raw pointer into an ordinary,
# correctly-refcounted Cython object: the <object> cast increfs (Cython
# always brackets object-typed values this way), so this one compensating
# Py_DECREF cancels exactly that, leaving the object carrying only the
# reference it already had. Used below to wrap every raw PyObject*/
# PyTypeObject* the C API hands back (matching the real C signatures --
# an `object`-returning extern declaration has to actually return a
# PyObject * at the C level, and multidict_capi.h genuinely declares the
# type getters as returning PyTypeObject *), so callers here get a
# ready-to-use `object` back with no cast or manual Py_DECREF of their own.

cdef inline object _steal(PyObject *ptr):
    cdef object result = <object>ptr
    Py_DECREF(result)
    return result


cdef inline object IStr_GetType(MultiDict_CAPI *capi):
    return _steal(<PyObject*>_IStr_GetType(capi))


cdef inline object MultiDict_GetType(MultiDict_CAPI *capi):
    return _steal(<PyObject*>_MultiDict_GetType(capi))


cdef inline object CIMultiDict_GetType(MultiDict_CAPI *capi):
    return _steal(<PyObject*>_CIMultiDict_GetType(capi))


cdef inline object MultiDictProxy_GetType(MultiDict_CAPI *capi):
    return _steal(<PyObject*>_MultiDictProxy_GetType(capi))


cdef inline object CIMultiDictProxy_GetType(MultiDict_CAPI *capi):
    return _steal(<PyObject*>_CIMultiDictProxy_GetType(capi))


# MultiDict_GetItem/Pop/SetDefault report a found/absent value through a
# PyObject **result out-parameter at the C level; wrapping that here too
# means callers just get a (found, value) tuple back instead of declaring
# a raw pointer, passing &result, and adopting it themselves.

cdef inline object MultiDict_GetItem(MultiDict_CAPI *capi, object self, object key):
    cdef PyObject *result = NULL
    cdef int found = _MultiDict_GetItem(capi, self, key, &result)
    if result == NULL:
        return (False, None)
    return (bool(found), _steal(result))


cdef inline object MultiDict_Pop(MultiDict_CAPI *capi, object self, object key):
    cdef PyObject *result = NULL
    cdef int found = _MultiDict_Pop(capi, self, key, &result)
    if result == NULL:
        return (False, None)
    return (bool(found), _steal(result))


cdef inline object MultiDict_SetDefault(MultiDict_CAPI *capi, object self, object key,
                                        object default_value=None):
    # `object default_value` cannot hold the C level NULL that means
    # "use None", but passing None itself is what NULL is read as, so
    # defaulting the argument here gives the same signature as the
    # Python method.
    # Unlike GetItem/Pop, *result is always set on success here (the
    # existing value if found, the freshly inserted default_value
    # otherwise) -- a NULL *result would only ever follow the exception
    # already raised by the `except -1` check above, so there's no
    # "not found" case to special-case here.
    cdef PyObject *result = NULL
    cdef int found = _MultiDict_SetDefault(capi, self, key, default_value, &result)
    return (bool(found), _steal(result))


# MultiDict_ForEach takes `key` as a raw `PyObject *` where NULL means
# "visit every item" -- there's no `object` value for that which wouldn't
# also risk colliding with an actual `None` key (see docs/cyapi.rst). It
# stays exposed under its own name as the low-level entry point (matching
# the C API exactly, for direct/advanced use); MultiDict_ForEachAll/Key
# below cover the ordinary case without a raw pointer or a NULL sentinel.
#
# The visitor passed to ForEachAll/Key is still a real `cdef` function --
# one indirect C call per visited item, not a Python-level call through
# an arbitrary callable -- but declared with Cython's own `object` type
# for identity/key/value instead of MultiDict_ItemVisitor's raw
# `PyObject *`, so a visitor needs no <object> cast of its own; `hash` is
# a plain C `Py_hash_t` in both. Same
# three-way return contract as MultiDict_ItemVisitor: a positive value
# keeps the walk going, 0 stops early (not an error), and `except -1`
# reports an exception the visitor itself raised.

ctypedef int (*MultiDict_CyItemVisitor)(object identity, Py_hash_t hash,
                                        object key, object value,
                                        void *user_data) except -1


cdef struct _CyVisitorCtx:
    MultiDict_CyItemVisitor visitor
    void *user_data


# `_cy_visitor_trampoline` still can't just let an exception propagate --
# it's assigned to a `noexcept` C function pointer (MultiDict_ItemVisitor),
# so Cython would treat an escaping exception as unraisable here, print
# it, and clear it rather than propagate it. `ctx.visitor`'s own
# `except -1` already turns its raw -1 return into a raised exception at
# this call site; PyErr_SetObject then reinstates it as the current
# exception (a raw C call, not a Cython `raise`) without going through
# that noexcept-triggered unraisable-and-clear handling, so the -1
# returned here has a real exception attached, which MultiDict_ForEachAll/
# Key's `except -1` propagates normally.

cdef inline int _cy_visitor_trampoline(void *ctx_, PyObject *identity,
                                       Py_hash_t hash, PyObject *key,
                                       PyObject *value) noexcept:
    cdef _CyVisitorCtx *ctx = <_CyVisitorCtx*>ctx_
    try:
        return ctx.visitor(<object>identity, hash, <object>key, <object>value,
                           ctx.user_data)
    except BaseException as exc:
        PyErr_SetObject(type(exc), exc)
        return -1


cdef inline Py_ssize_t MultiDict_ForEachAll(MultiDict_CAPI *capi, object self,
                                            MultiDict_CyItemVisitor visitor,
                                            void *user_data) except -1:
    cdef _CyVisitorCtx ctx
    ctx.visitor = visitor
    ctx.user_data = user_data
    return MultiDict_ForEach(capi, self, NULL, _cy_visitor_trampoline, &ctx)


cdef inline Py_ssize_t MultiDict_ForEachKey(MultiDict_CAPI *capi, object self, object key,
                                            MultiDict_CyItemVisitor visitor,
                                            void *user_data) except -1:
    cdef _CyVisitorCtx ctx
    ctx.visitor = visitor
    ctx.user_data = user_data
    return MultiDict_ForEach(capi, self, <PyObject*>key, _cy_visitor_trampoline, &ctx)
