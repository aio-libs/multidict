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

    int MULTIDICT_MAX_WATCHERS

    ctypedef enum MultiDict_WatchEvent:
        MultiDict_EVENT_ADDED
        MultiDict_EVENT_REPLACED
        MultiDict_EVENT_DELETED
        MultiDict_EVENT_CLEARED
        MultiDict_EVENT_CLONED
        MultiDict_EVENT_DEALLOCATED
        MultiDict_EVENT_BATCH_BEGIN
        MultiDict_EVENT_BATCH_END
        MultiDict_EVENT_LOST

    # `self` is spelled `md` here because it is a reserved word in Cython;
    # the C name is unchanged. Every PyObject * is borrowed and may be NULL.
    ctypedef struct MultiDict_WatchInfo:
        MultiDict_WatchEvent event
        PyObject *md "self"
        PyObject *identity
        Py_hash_t hash
        PyObject *key
        PyObject *value
        PyObject *old_value

    ctypedef int (*MultiDict_WatchCallback)(
        void *watcher_data, void *user_data,
        const MultiDict_WatchInfo *info) noexcept


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

    # watchers
    int MultiDict_AddWatcher(MultiDict_CAPI *capi, MultiDict_WatchCallback callback,
                             void *watcher_data) except -1
    int MultiDict_ClearWatcher(MultiDict_CAPI *capi, int watcher_id) except -1
    int MultiDict_Watch(MultiDict_CAPI *capi, int watcher_id, object self,
                        void *user_data) except -1
    int MultiDict_Unwatch(MultiDict_CAPI *capi, int watcher_id, object self) except -1


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


# A watch callback reaches its context through MultiDict_AddWatcher's
# `watcher_data`, which is the one slot Cython needs for a trampoline of
# its own -- unlike MultiDict_ForEach, whose single void* already belongs
# to the caller. So the Cython-facing callback below takes `key`, `value`
# and friends as ordinary objects (None where the C API passes NULL) and
# may raise, with `except -1` carrying the exception back out.
#
# `md` stays a raw PyObject * on purpose, and is the one argument that
# does not become an object here: on a MultiDict_EVENT_DEALLOCATED event
# it is at refcount 0, and Cython increfs anything typed `object` on the
# way in, which would resurrect it and then free it twice. A callback
# that wants the multidict must check the event first and only then cast
# `<object>md`; on DEALLOCATED it may use the pointer as an identity and
# nothing else. See docs/cyapi.rst.
#
# The trampoline's context is the caller's to own and keep alive for as
# long as the watcher is registered: declare a module-level
# `cdef MultiDict_CyWatcherCtx ctx` in the .pyx that registers it. Nothing
# here allocates, so nothing here has to be freed.

ctypedef int (*MultiDict_CyWatchCallback)(void *watcher_data, void *user_data,
                                          MultiDict_WatchEvent event, PyObject *md,
                                          object identity, Py_hash_t hash,
                                          object key, object value,
                                          object old_value) except -1


cdef struct MultiDict_CyWatcherCtx:
    MultiDict_CyWatchCallback callback
    void *watcher_data


cdef inline object _opt(PyObject *ptr):
    # The C API passes NULL for the fields an event does not carry.
    if ptr == NULL:
        return None
    return <object>ptr


# Same noexcept reasoning as _cy_visitor_trampoline above: this is
# assigned to a `noexcept` C function pointer, so an escaping exception
# would be printed and cleared instead of propagated. PyErr_SetObject
# reinstates it as the current exception without going through that, and
# multidict then reports it with PyErr_WriteUnraisable -- a watch callback
# runs after the mutation it describes, so a failure can only ever be
# reported, never propagated back into the operation.

cdef inline int _cy_watch_trampoline(void *ctx_, void *user_data,
                                     const MultiDict_WatchInfo *info) noexcept:
    cdef MultiDict_CyWatcherCtx *ctx = <MultiDict_CyWatcherCtx*>ctx_
    try:
        return ctx.callback(ctx.watcher_data, user_data, info.event,
                            info.md, _opt(info.identity), info.hash,
                            _opt(info.key), _opt(info.value),
                            _opt(info.old_value))
    except BaseException as exc:
        PyErr_SetObject(type(exc), exc)
        return -1


cdef inline int MultiDict_AddCyWatcher(MultiDict_CAPI *capi,
                                       MultiDict_CyWatcherCtx *ctx) except -1:
    return MultiDict_AddWatcher(capi, _cy_watch_trampoline, ctx)
