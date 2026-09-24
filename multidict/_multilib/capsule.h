#ifndef _MULTIDICT_CAPSULE_H
#define _MULTIDICT_CAPSULE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../multidict_capi_struct.h"
#include "dict.h"
#include "hashtable.h"
#include "state.h"
#include "watch.h"

#define __MULTIDICT_VALIDATION_CHECK(SELF, STATE, ON_FAIL)           \
    if (!MultiDict_Check(((mod_state*)STATE), (SELF))) {             \
        PyErr_Format(PyExc_TypeError,                                \
                     #SELF " should be a MultiDict instance not %s", \
                     Py_TYPE(SELF)->tp_name);                        \
        return ON_FAIL;                                              \
    }

// Read-only entry points accept a MultiDict, CIMultiDict, MultiDictProxy or
// CIMultiDictProxy alike, unlike __MULTIDICT_VALIDATION_CHECK above (used by
// the mutators, which reject proxies since there is nothing to mutate
// through a read-only view).
#define __MULTIDICT_RESOLVE_ANY(SELF, STATE, MD, ON_FAIL)                  \
    if (AnyMultiDict_Check(((mod_state*)STATE), (SELF))) {                 \
        (MD) = (MultiDictObject*)(SELF);                                   \
    } else if (AnyMultiDictProxy_Check(((mod_state*)STATE), (SELF))) {     \
        (MD) = ((MultiDictProxyObject*)(SELF))->md;                        \
    } else {                                                               \
        PyErr_Format(PyExc_TypeError,                                      \
                     #SELF                                                 \
                     " should be a MultiDict, CIMultiDict, "               \
                     "MultiDictProxy or CIMultiDictProxy instance not %s", \
                     Py_TYPE(SELF)->tp_name);                              \
        return ON_FAIL;                                                    \
    }

/* ================= istr ================= */

static PyTypeObject*
IStr_GetType(void* state_)
{
    mod_state* state = (mod_state*)state_;
    return (PyTypeObject*)Py_NewRef(state->IStrType);
}

static PyObject*
IStr_FromUnicode(void* state_, PyObject* str)
{
    mod_state* state = (mod_state*)state_;
    if (!PyUnicode_Check(str)) {
        PyErr_Format(PyExc_TypeError,
                     "str argument should be a str instance not %s",
                     Py_TYPE(str)->tp_name);
        return NULL;
    }
    // Matches istr(existing_istr): return the same object, not a copy.
    if (IStr_Check(state, str)) {
        return Py_NewRef(str);
    }
    PyObject* canonical = PyObject_CallMethodNoArgs(str, state->str_lower);
    if (canonical == NULL) {
        return NULL;
    }
    PyObject* ret = IStr_New(state, str, canonical);
    Py_DECREF(canonical);
    return ret;
}

/* ================= Version counter ================= */

static uint64_t
MultiDict_GetVersion(void* state_, PyObject* self)
{
    MultiDictObject* md;
    __MULTIDICT_RESOLVE_ANY(self, state_, md, 0);
    return load_version(md);
}

/* ==== MultiDict / CIMultiDict / proxy type objects ==== */

static PyTypeObject*
MultiDict_GetType(void* state_)
{
    mod_state* state = (mod_state*)state_;
    return (PyTypeObject*)Py_NewRef(state->MultiDictType);
}

static PyTypeObject*
CIMultiDict_GetType(void* state_)
{
    mod_state* state = (mod_state*)state_;
    return (PyTypeObject*)Py_NewRef(state->CIMultiDictType);
}

static PyTypeObject*
MultiDictProxy_GetType(void* state_)
{
    mod_state* state = (mod_state*)state_;
    return (PyTypeObject*)Py_NewRef(state->MultiDictProxyType);
}

static PyTypeObject*
CIMultiDictProxy_GetType(void* state_)
{
    mod_state* state = (mod_state*)state_;
    return (PyTypeObject*)Py_NewRef(state->CIMultiDictProxyType);
}

/* ================= Constructors ================= */

static PyObject*
MultiDict_New(void* state_, Py_ssize_t prealloc_size)
{
    mod_state* state = (mod_state*)state_;
    MultiDictObject* md =
        (MultiDictObject*)_md_shell_alloc(state, state->MultiDictType);
    if (md == NULL) {
        return NULL;
    }
    md->state = state;
    md_set_module(md, state->mod);
    if (md_init(md, false, prealloc_size) < 0) {
        Py_CLEAR(md);
        return NULL;
    }
    return (PyObject*)md;
}

static PyObject*
CIMultiDict_New(void* state_, Py_ssize_t prealloc_size)
{
    mod_state* state = (mod_state*)state_;
    MultiDictObject* md =
        (MultiDictObject*)_md_shell_alloc(state, state->CIMultiDictType);
    if (md == NULL) {
        return NULL;
    }
    md->state = state;
    md_set_module(md, state->mod);
    if (md_init(md, true, prealloc_size) < 0) {
        Py_CLEAR(md);
        return NULL;
    }
    return (PyObject*)md;
}

static PyObject*
MultiDictProxy_New(void* state_, PyObject* arg)
{
    mod_state* state = (mod_state*)state_;
    if (!AnyMultiDictProxy_Check(state, arg) &&
        !AnyMultiDict_Check(state, arg)) {
        PyErr_Format(PyExc_TypeError,
                     "MultiDictProxy requires a MultiDict or "
                     "MultiDictProxy instance, not %s",
                     Py_TYPE(arg)->tp_name);
        return NULL;
    }
    MultiDictProxyObject* self = (MultiDictProxyObject*)_md_shell_alloc(
        state, state->MultiDictProxyType);
    if (self == NULL) {
        return NULL;
    }
    MultiDictObject* md = AnyMultiDictProxy_Check(state, arg)
                              ? ((MultiDictProxyObject*)arg)->md
                              : (MultiDictObject*)arg;
    self->md = (MultiDictObject*)Py_NewRef(md);
    return (PyObject*)self;
}

static PyObject*
CIMultiDictProxy_New(void* state_, PyObject* arg)
{
    mod_state* state = (mod_state*)state_;
    if (!CIMultiDictProxy_Check(state, arg) &&
        !CIMultiDict_Check(state, arg)) {
        PyErr_Format(PyExc_TypeError,
                     "CIMultiDictProxy requires a CIMultiDict or "
                     "CIMultiDictProxy instance, not %s",
                     Py_TYPE(arg)->tp_name);
        return NULL;
    }
    MultiDictProxyObject* self = (MultiDictProxyObject*)_md_shell_alloc(
        state, state->CIMultiDictProxyType);
    if (self == NULL) {
        return NULL;
    }
    MultiDictObject* md = CIMultiDictProxy_Check(state, arg)
                              ? ((MultiDictProxyObject*)arg)->md
                              : (MultiDictObject*)arg;
    self->md = (MultiDictObject*)Py_NewRef(md);
    return (PyObject*)self;
}

/* ================= Getters ================= */

static Py_ssize_t
MultiDict_Size(void* state_, PyObject* self)
{
    MultiDictObject* md;
    __MULTIDICT_RESOLVE_ANY(self, state_, md, -1);
    return md_len(md);
}

static int
MultiDict_Contains(void* state_, PyObject* self, PyObject* key)
{
    MultiDictObject* md;
    __MULTIDICT_RESOLVE_ANY(self, state_, md, -1);
    return md_contains(md, key, NULL);
}

static int
MultiDict_GetItem(void* state_, PyObject* self, PyObject* key,
                  PyObject** result)
{
    *result = NULL;
    MultiDictObject* md;
    __MULTIDICT_RESOLVE_ANY(self, state_, md, -1);
    return md_get_one(md, key, result);
}

/* ================= Setters ================= */

// `MultiDict_Check` also accepts `CIMultiDict` (it is a subclass), so all six
// of these work against either type -- there is no separate CIMultiDict_Add,
// CIMultiDict_Clear, and so on. Unlike the getters above, none of these
// accept a MultiDictProxy/CIMultiDictProxy: proxies expose no mutating
// methods at the Python level either, so there is nothing to mutate through.

static int
MultiDict_Add(void* state_, PyObject* self, PyObject* key, PyObject* value)
{
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_add((MultiDictObject*)self, key, value);
}

static int
MultiDict_Clear(void* state_, PyObject* self)
{
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    int ret;
    bool flush;
    Py_BEGIN_CRITICAL_SECTION(self);
    if (md_len((MultiDictObject*)self) != 0) {
        md_watch_record_simple((MultiDictObject*)self,
                               MultiDict_EVENT_CLEARED);
    }
    ret = md_clear((MultiDictObject*)self);
    flush = md_watch_pending((MultiDictObject*)self);
    Py_END_CRITICAL_SECTION();
    md_watch_flush_if((MultiDictObject*)self, flush);
    return ret;
}

static int
MultiDict_DelItem(void* state_, PyObject* self, PyObject* key)
{
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_del((MultiDictObject*)self, key);
}

static int
MultiDict_Pop(void* state_, PyObject* self, PyObject* key, PyObject** result)
{
    *result = NULL;
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_pop_one((MultiDictObject*)self, key, result);
}

static int
MultiDict_SetDefault(void* state_, PyObject* self, PyObject* key,
                     PyObject* default_value, PyObject** result)
{
    *result = NULL;
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_set_default((MultiDictObject*)self, key, default_value, result);
}

static int
MultiDict_SetItem(void* state_, PyObject* self, PyObject* key, PyObject* value)
{
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_replace((MultiDictObject*)self, key, value);
}

/* ================= Iteration ================= */

// `visitor` receives borrowed references: the walk holds a reference to each
// of `identity`/`key`/`value` for the duration of the visitor call and
// releases it right after, so none can be freed out from under the visitor
// even under Py_GIL_DISABLED -- the critical section held for the whole walk
// also blocks any other thread from mutating `md` in the meantime. `hash` is
// the identity's hash, the one the table stores for the entry. The
// foreach-key form passes the identity computed from `key`, and its hash,
// which compare equal to every visited entry's own.
//
// `visitor` must not call back into any method on the multidict
// being walked: both walks compare a version stamped at
// walk start against `md->version` on every step and raise
// "MultiDict is changed during iteration" the moment they diverge, so a
// reentrant mutation aborts the walk with a clear error instead of
// silently corrupting or hiding results.

static Py_ssize_t
_md_foreach_all(MultiDictObject* md, MultiDict_ItemVisitor visitor,
                void* user_data)
{
    Py_ssize_t count;
    Py_BEGIN_CRITICAL_SECTION(md);
    count = md_walk_all(md, true, visitor, user_data);
    ASSERT_CONSISTENT(md, false);
    Py_END_CRITICAL_SECTION();
    return count;
}

static Py_ssize_t
_md_foreach_key(MultiDictObject* md, PyObject* key,
                MultiDict_ItemVisitor visitor, void* user_data)
{
    PyObject* identity;
    Py_hash_t hash;
    if (md_calc_identity_hash(md, key, &identity, &hash) < 0) {
        return -1;
    }
    Py_ssize_t count;
    Py_BEGIN_CRITICAL_SECTION(md);
    count = md_walk_with_hash(md, identity, hash, true, visitor, user_data);
    ASSERT_CONSISTENT(md, false);
    Py_END_CRITICAL_SECTION();
    Py_DECREF(identity);
    return count;
}

static Py_ssize_t
MultiDict_ForEach(void* state_, PyObject* self, PyObject* key,
                  MultiDict_ItemVisitor visitor, void* user_data)
{
    MultiDictObject* md;
    __MULTIDICT_RESOLVE_ANY(self, state_, md, -1);
    if (key == NULL) {
        return _md_foreach_all(md, visitor, user_data);
    }
    return _md_foreach_key(md, key, visitor, user_data);
}

/* ================= Watchers ================= */

/* Registration writes the module-wide slot table and is not thread safe
 * against a concurrent MultiDict_AddWatcher() or MultiDict_ClearWatcher(),
 * exactly like CPython's PyDict_AddWatcher(): register during module
 * initialization, before the watcher can fire. MultiDict_Watch() and
 * MultiDict_Unwatch() touch only the target multidict and do take its
 * critical section, so those are safe from any thread.
 */

static int
MultiDict_AddWatcher(void* state_, MultiDict_WatchCallback callback,
                     void* watcher_data)
{
    mod_state* state = (mod_state*)state_;
    if (callback == NULL) {
        PyErr_SetString(PyExc_ValueError, "callback must not be NULL");
        return -1;
    }
    for (int watcher_id = 0; watcher_id < MULTIDICT_MAX_WATCHERS;
         watcher_id++) {
        if (state->watchers[watcher_id] == NULL) {
            state->watchers[watcher_id] = callback;
            state->watcher_data[watcher_id] = watcher_data;
            return watcher_id;
        }
    }
    PyErr_SetString(PyExc_RuntimeError,
                    "no more multidict watcher IDs available");
    return -1;
}

static int
_multidict_check_watcher_id(mod_state* state, int watcher_id)
{
    if (watcher_id < 0 || watcher_id >= MULTIDICT_MAX_WATCHERS ||
        state->watchers[watcher_id] == NULL) {
        PyErr_Format(PyExc_ValueError, "invalid watcher ID %d", watcher_id);
        return -1;
    }
    return 0;
}

static int
MultiDict_ClearWatcher(void* state_, int watcher_id)
{
    mod_state* state = (mod_state*)state_;
    if (_multidict_check_watcher_id(state, watcher_id) < 0) {
        return -1;
    }
    /* Multidicts still carrying the bit keep it: nothing enumerates them.
       A stale bit resolves to this NULL slot and is skipped, same as
       CPython's PyDict_ClearWatcher(). */
    state->watchers[watcher_id] = NULL;
    state->watcher_data[watcher_id] = NULL;
    return 0;
}

static int
MultiDict_Watch(void* state_, int watcher_id, PyObject* self, void* user_data)
{
    MultiDictObject* md;
    __MULTIDICT_RESOLVE_ANY(self, state_, md, -1);
    if (_multidict_check_watcher_id((mod_state*)state_, watcher_id) < 0) {
        return -1;
    }
    int ret;
    Py_BEGIN_CRITICAL_SECTION(md);
    ret = md_watch_attach(md, watcher_id, user_data);
    Py_END_CRITICAL_SECTION();
    return ret;
}

static int
MultiDict_Unwatch(void* state_, int watcher_id, PyObject* self)
{
    MultiDictObject* md;
    __MULTIDICT_RESOLVE_ANY(self, state_, md, -1);
    if (_multidict_check_watcher_id((mod_state*)state_, watcher_id) < 0) {
        return -1;
    }
    Py_BEGIN_CRITICAL_SECTION(md);
    md_watch_detach(md, watcher_id);
    Py_END_CRITICAL_SECTION();
    return 0;
}

/* =================== Capsule ==================== */

static void
capsule_free(MultiDict_CAPI* capi)
{
    PyMem_Free(capi);
}

static void
capsule_destructor(PyObject* o)
{
    MultiDict_CAPI* capi =
        (MultiDict_CAPI*)PyCapsule_GetPointer(o, MultiDict_CAPSULE_NAME);
    capsule_free(capi);
}

static PyObject*
new_capsule(mod_state* state)
{
    MultiDict_CAPI* capi =
        (MultiDict_CAPI*)PyMem_Malloc(sizeof(MultiDict_CAPI));
    if (capi == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    capi->api_version = MultiDict_CAPI_VERSION;
    capi->state = state;

    capi->IStr_GetType = IStr_GetType;
    capi->IStr_FromUnicode = IStr_FromUnicode;

    capi->MultiDict_GetVersion = MultiDict_GetVersion;

    capi->MultiDict_GetType = MultiDict_GetType;
    capi->CIMultiDict_GetType = CIMultiDict_GetType;
    capi->MultiDictProxy_GetType = MultiDictProxy_GetType;
    capi->CIMultiDictProxy_GetType = CIMultiDictProxy_GetType;

    capi->MultiDict_New = MultiDict_New;
    capi->CIMultiDict_New = CIMultiDict_New;
    capi->MultiDictProxy_New = MultiDictProxy_New;
    capi->CIMultiDictProxy_New = CIMultiDictProxy_New;

    capi->MultiDict_Size = MultiDict_Size;
    capi->MultiDict_Contains = MultiDict_Contains;
    capi->MultiDict_GetItem = MultiDict_GetItem;

    capi->MultiDict_Add = MultiDict_Add;
    capi->MultiDict_Clear = MultiDict_Clear;
    capi->MultiDict_DelItem = MultiDict_DelItem;
    capi->MultiDict_Pop = MultiDict_Pop;
    capi->MultiDict_SetDefault = MultiDict_SetDefault;
    capi->MultiDict_SetItem = MultiDict_SetItem;

    capi->MultiDict_ForEach = MultiDict_ForEach;

    capi->MultiDict_AddWatcher = MultiDict_AddWatcher;
    capi->MultiDict_ClearWatcher = MultiDict_ClearWatcher;
    capi->MultiDict_Watch = MultiDict_Watch;
    capi->MultiDict_Unwatch = MultiDict_Unwatch;

    PyObject* ret =
        PyCapsule_New(capi, MultiDict_CAPSULE_NAME, capsule_destructor);
    if (ret == NULL) {
        capsule_free(capi);
    }
    return ret;
}

#ifdef __cplusplus
}
#endif

#endif
