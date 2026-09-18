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
    mod_state* state = (mod_state*)state_;
    MultiDictObject* md;
    if (AnyMultiDict_Check(state, self)) {
        md = (MultiDictObject*)self;
    } else if (AnyMultiDictProxy_Check(state, self)) {
        md = ((MultiDictProxyObject*)self)->md;
    } else {
        PyErr_Format(PyExc_TypeError,
                     "self should be a MultiDict, CIMultiDict, "
                     "MultiDictProxy or CIMultiDictProxy instance not %s",
                     Py_TYPE(self)->tp_name);
        return 0;
    }
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

static int
MultiDict_Contains(void* state_, PyObject* self, PyObject* key)
{
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_contains((MultiDictObject*)self, key, NULL);
}

static int
MultiDict_GetItem(void* state_, PyObject* self, PyObject* key,
                  PyObject** result)
{
    *result = NULL;
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_get_one((MultiDictObject*)self, key, result);
}

static Py_ssize_t
MultiDict_Size(void* state_, PyObject* self)
{
    mod_state* state = (mod_state*)state_;
    MultiDictObject* md;
    if (AnyMultiDict_Check(state, self)) {
        md = (MultiDictObject*)self;
    } else if (AnyMultiDictProxy_Check(state, self)) {
        md = ((MultiDictProxyObject*)self)->md;
    } else {
        PyErr_Format(PyExc_TypeError,
                     "self should be a MultiDict, CIMultiDict, "
                     "MultiDictProxy or CIMultiDictProxy instance not %s",
                     Py_TYPE(self)->tp_name);
        return -1;
    }
    return md_len(md);
}

/* ================= Setters ================= */

// `MultiDict_Check` also accepts `CIMultiDict` (it is a subclass), so all six
// of these work against either type -- there is no separate CIMultiDict_Add,
// CIMultiDict_Clear, and so on.

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
    Py_BEGIN_CRITICAL_SECTION(self);
    ret = md_replace((MultiDictObject*)self, key, value);
    Py_END_CRITICAL_SECTION();
    return ret;
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

    capi->MultiDict_Contains = MultiDict_Contains;
    capi->MultiDict_GetItem = MultiDict_GetItem;
    capi->MultiDict_Size = MultiDict_Size;

    capi->MultiDict_Add = MultiDict_Add;
    capi->MultiDict_Clear = MultiDict_Clear;
    capi->MultiDict_DelItem = MultiDict_DelItem;
    capi->MultiDict_Pop = MultiDict_Pop;
    capi->MultiDict_SetDefault = MultiDict_SetDefault;
    capi->MultiDict_SetItem = MultiDict_SetItem;

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
