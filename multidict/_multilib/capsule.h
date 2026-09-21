#ifndef _MULTIDICT_CAPSULE_H
#define _MULTIDICT_CAPSULE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../multidict_capi_struct.h"
#include "dict.h"
#include "hashtable.h"
#include "state.h"

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
    return md_version(md);
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
    MultiDictObject* md = (MultiDictObject*)state->MultiDictType->tp_alloc(
        state->MultiDictType, 0);
    if (md == NULL) {
        return NULL;
    }
    md->state = state;
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
    MultiDictObject* md = (MultiDictObject*)state->CIMultiDictType->tp_alloc(
        state->CIMultiDictType, 0);
    if (md == NULL) {
        return NULL;
    }
    md->state = state;
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
    MultiDictProxyObject* self =
        (MultiDictProxyObject*)state->MultiDictProxyType->tp_alloc(
            state->MultiDictProxyType, 0);
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
    MultiDictProxyObject* self =
        (MultiDictProxyObject*)state->CIMultiDictProxyType->tp_alloc(
            state->CIMultiDictProxyType, 0);
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
    int ret;
    Py_BEGIN_CRITICAL_SECTION(self);
    ret = md_add((MultiDictObject*)self, key, value);
    ASSERT_CONSISTENT((MultiDictObject*)self, false);
    Py_END_CRITICAL_SECTION();
    return ret;
}

static int
MultiDict_Clear(void* state_, PyObject* self)
{
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    int ret;
    Py_BEGIN_CRITICAL_SECTION(self);
    ret = md_clear((MultiDictObject*)self);
    Py_END_CRITICAL_SECTION();
    return ret;
}

static int
MultiDict_DelItem(void* state_, PyObject* self, PyObject* key)
{
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    int ret;
    Py_BEGIN_CRITICAL_SECTION(self);
    ret = md_del((MultiDictObject*)self, key);
    Py_END_CRITICAL_SECTION();
    return ret;
}

static int
MultiDict_Pop(void* state_, PyObject* self, PyObject* key, PyObject** result)
{
    *result = NULL;
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    int ret;
    Py_BEGIN_CRITICAL_SECTION(self);
    ret = md_pop_one((MultiDictObject*)self, key, result);
    ASSERT_CONSISTENT((MultiDictObject*)self, false);
    Py_END_CRITICAL_SECTION();
    return ret;
}

static int
MultiDict_SetDefault(void* state_, PyObject* self, PyObject* key,
                     PyObject* default_value, PyObject** result)
{
    *result = NULL;
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    int ret;
    Py_BEGIN_CRITICAL_SECTION(self);
    ret = md_set_default((MultiDictObject*)self, key, default_value, result);
    Py_END_CRITICAL_SECTION();
    return ret;
}

static int
MultiDict_SetItem(void* state_, PyObject* self, PyObject* key, PyObject* value)
{
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    int ret;
    deferred_decref_t defer;
    deferred_decref_init(&defer);
    Py_BEGIN_CRITICAL_SECTION(self);
    ret = md_replace((MultiDictObject*)self, key, value, &defer);
    Py_END_CRITICAL_SECTION();
    deferred_decref_release(&defer);
    return ret;
}

/* ================= Iteration ================= */

// `visitor` receives borrowed references: md_next/find_next hand back new
// references for `k`/`v`, held here for the duration of the visitor call and
// released right after, so the value cannot be freed out from under the
// visitor even under Py_GIL_DISABLED -- the critical section held for the
// whole walk also blocks any other thread from mutating `md` in the
// meantime. `visitor` must not call back into any method on the multidict
// being walked: both md_next and find_next compare a version stamped at
// walk start against `md->version` on every step and raise
// "MultiDict is changed during iteration" the moment they diverge, so a
// reentrant mutation aborts the walk with a clear error instead of
// silently corrupting or hiding results.

static Py_ssize_t
_md_foreach_all(MultiDictObject* md, MultiDict_ItemVisitor visitor,
                void* user_data)
{
    md_pos_t pos;
    PyObject* k;
    PyObject* v;
    int found;
    Py_ssize_t count = 0;
    bool failed = false;
    Py_BEGIN_CRITICAL_SECTION(md);
    md_init_pos(md, &pos);
    while ((found = md_next(md, &pos, NULL, &k, &v)) > 0) {
        count++;
        int ret = visitor(user_data, k, v);
        Py_DECREF(k);
        Py_DECREF(v);
        if (ret < 0) {
            assert(PyErr_Occurred());
            failed = true;
            break;
        }
        if (ret == 0) {
            break;
        }
    }
    if (found < 0) {
        failed = true;
    }
    Py_END_CRITICAL_SECTION();
    return failed ? -1 : count;
}

static Py_ssize_t
_md_foreach_key(MultiDictObject* md, PyObject* key,
                MultiDict_ItemVisitor visitor, void* user_data)
{
    PyObject* identity = md_calc_identity(md, key);
    if (identity == NULL) {
        return -1;
    }
    Py_ssize_t count = 0;
    bool failed = false;
    finder_t finder;
    Py_BEGIN_CRITICAL_SECTION(md);
    if (finder_init(md, identity, &finder) < 0) {
        failed = true;
    } else {
        PyObject* k;
        PyObject* v;
        int found;
        while ((found = find_next(&finder, &k, &v)) > 0) {
            count++;
            int ret = visitor(user_data, k, v);
            Py_DECREF(k);
            Py_DECREF(v);
            if (ret < 0) {
                assert(PyErr_Occurred());
                failed = true;
                break;
            }
            if (ret == 0) {
                break;
            }
        }
        if (found < 0) {
            failed = true;
        }
        finder_cleanup(&finder);
        ASSERT_CONSISTENT(md, false);
    }
    Py_END_CRITICAL_SECTION();
    Py_DECREF(identity);
    return failed ? -1 : count;
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
