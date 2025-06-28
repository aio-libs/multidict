#ifndef _MULTIDICT_CAPSULE_H
#define _MULTIDICT_CAPSULE_H

#ifdef __cplusplus
extern "C" {
#endif

#define MULTIDICT_IMPL

#include "../multidict_api.h"
#include "dict.h"
#include "hashtable.h"
#include "state.h"

// NOTE: MACROS WITH '__' ARE INTERNAL METHODS,
// PLEASE DON'T USE IN OTHER PROJECTS!!!

#define __MULTIDICT_VALIDATION_CHECK(SELF, STATE, ON_FAIL)           \
    if (MultiDict_Check(((mod_state*)STATE), (SELF)) <= 0) {         \
        PyErr_Format(PyExc_TypeError,                                \
                     #SELF " should be a MultiDict instance not %s", \
                     Py_TYPE(SELF)->tp_name);                        \
        return ON_FAIL;                                              \
    }

#define __MULTIDICTPROXY_VALIDATION_CHECK(SELF, STATE, ON_FAIL)           \
    if (MultiDictProxy_Check(((mod_state*)STATE), (SELF)) <= 0) {         \
        PyErr_Format(PyExc_TypeError,                                     \
                     #SELF " should be a MultiDictProxy instance not %s", \
                     Py_TYPE(SELF)->tp_name);                             \
        return ON_FAIL;                                                   \
    }

#define __MULTIDICTPROXY_GET_MD(SELF) ((MultiDictProxyObject*)SELF)->md

static PyTypeObject*
MultiDict_GetType(void* state_)
{
    mod_state* state = (mod_state*)state_;
    return (PyTypeObject*)Py_NewRef(state->MultiDictType);
}

static PyObject*
MultiDict_New(void* state_, int prealloc_size)
{
    mod_state* state = (mod_state*)state_;
    MultiDictObject* md = (MultiDictObject*)state->MultiDictType->tp_alloc(
        state->MultiDictType, 0);

    if (md == NULL) {
        return NULL;
    }
    if (md_init(md, state, false, prealloc_size) < 0) {
        Py_CLEAR(md);
        return NULL;
    }
    return (PyObject*)md;
}

static int
MultiDict_Add(void* state_, PyObject* self, PyObject* key, PyObject* value)
{
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_add((MultiDictObject*)self, key, value);
}

static int
MultiDict_Clear(void* state_, PyObject* self)
{
    // TODO: Macro for repeated steps being done?
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_clear((MultiDictObject*)self);
}

static int
MultiDict_SetDefault(void* state_, PyObject* self, PyObject* key,
                     PyObject* value, PyObject** result)
{
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_set_default((MultiDictObject*)self, key, value, result);
}

static int
MultiDict_Del(void* state_, PyObject* self, PyObject* key)
{
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_del((MultiDictObject*)self, key);
}

static uint64_t
MultiDict_Version(void* state_, PyObject* self)
{
    __MULTIDICT_VALIDATION_CHECK(self, state_, 0);
    return md_version((MultiDictObject*)self);
}

static int
MultiDict_Contains(void* state_, PyObject* self, PyObject* key)
{
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_contains((MultiDictObject*)self, key, NULL);
}

// Suggestion: Would be smart in to do what python does and provide
// a version of GetOne, GetAll, PopOne & PopAll simillar
// to an unsafe call. The validation check could then be
// replaced with an assertion check such as _PyList_CAST for example
// a concept of this idea can be found for PyList_GetItem -> PyList_GET_ITEM

static int
MultiDict_GetOne(void* state_, PyObject* self, PyObject* key,
                 PyObject** result)
{
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);

    // TODO: edit md_get_one to return 0 if not found, 1 if found.
    // For now the macro made will suffice...
    return md_get_one((MultiDictObject*)self, key, result);
}

static int
MultiDict_GetAll(void* state_, PyObject* self, PyObject* key,
                 PyObject** result)
{
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_get_all((MultiDictObject*)self, key, result);
}

static int
MultiDict_PopOne(void* state_, PyObject* self, PyObject* key,
                 PyObject** result)
{
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_pop_one((MultiDictObject*)self, key, result);
}

static int
MultiDict_PopAll(void* state_, PyObject* self, PyObject* key,
                 PyObject** result)
{
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_pop_all((MultiDictObject*)self, key, result);
}

static PyObject*
MultiDict_PopItem(void* state_, PyObject* self)
{
    __MULTIDICT_VALIDATION_CHECK(self, state_, NULL);
    return md_pop_item((MultiDictObject*)self);
}

static int
MultiDict_Replace(void* state_, PyObject* self, PyObject* key, PyObject* value)
{
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_replace((MultiDictObject*)self, key, value);
}

static int
MultiDict_UpdateFromMultiDict(void* state_, PyObject* self, PyObject* other,
                              UpdateOp op)
{
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    __MULTIDICT_VALIDATION_CHECK(other, state_, -1);
    int ret =
        md_update_from_ht((MultiDictObject*)self, (MultiDictObject*)other, op);
    if (op != Extend) {
        md_post_update((MultiDictObject*)self);
    }
    return ret;
}

static int
MultiDict_UpdateFromDict(void* state_, PyObject* self, PyObject* other,
                         UpdateOp op)
{
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    if (PyDict_CheckExact(other) <= 0) {
        PyErr_Format(PyExc_TypeError,
                     "other should be a MultiDict instance not %s",
                     Py_TYPE(other)->tp_name);
        return -1;
    }
    int ret = md_update_from_dict((MultiDictObject*)self, other, op);
    if (op != Extend) {
        md_post_update((MultiDictObject*)self);
    }
    return ret;
}

static int
MultiDict_UpdateFromSequence(void* state_, PyObject* self, PyObject* seq,
                             UpdateOp op)
{
    __MULTIDICT_VALIDATION_CHECK(self, state_, -1);
    int ret = md_update_from_seq((MultiDictObject*)self, seq, op);
    if (op != Extend) {
        md_post_update((MultiDictObject*)self);
    }
    return ret;
}

static PyObject*
MultiDictProxy_New(void* state_, PyObject* md)
{
    // This is meant to be a more optimized version of
    // multidict_proxy_tp_init(...)

    mod_state* state = (mod_state*)state_;
    PyObject* self =
        state->MultiDictProxyType->tp_alloc(&state->MultiDictProxyType, 0);
    if (self == NULL) {
        return NULL;
    }
    if (!AnyMultiDictProxy_Check(((mod_state*)state_), md) &&
        !AnyMultiDict_Check(state, md)) {
        PyErr_Format(PyExc_TypeError,
                     "md requires MultiDict or MultiDictProxy instance, "
                     "not <class '%s'>",
                     Py_TYPE(md)->tp_name);
        goto fail;
    }
    if (AnyMultiDictProxy_Check(state, md)) {
        md = ((MultiDictProxyObject*)md)->md;
    } else {
        md = (MultiDictObject*)md;
    }
    Py_INCREF(md);
    ((MultiDictProxyObject*)self)->md = md;
    return self;
fail:
    Py_XDECREF(self);
    return NULL;
}

static int
MultiDictProxy_Contains(void* state_, PyObject* self, PyObject* key)
{
    __MULTIDICTPROXY_VALIDATION_CHECK(self, state_, -1);
    return md_contains(__MULTIDICTPROXY_GET_MD(self), key, NULL);
}

static int
MultiDictProxy_GetAll(void* state_, PyObject* self, PyObject* key,
                      PyObject** result)
{
    __MULTIDICTPROXY_VALIDATION_CHECK(self, state_, -1);
    return md_get_all(__MULTIDICTPROXY_GET_MD(self), key, result);
}

static int
MultiDictProxy_GetOne(void* state_, PyObject* self, PyObject* key,
                      PyObject** result)
{
    __MULTIDICTPROXY_VALIDATION_CHECK(self, state_, -1);
    return md_get_one(__MULTIDICTPROXY_GET_MD(self), key, result);
}

static PyTypeObject*
MultiDictProxy_GetType(void* state_)
{
    mod_state* state = (mod_state*)state_;
    return (PyTypeObject*)Py_NewRef(state->MultiDictProxyType);
}

static void
capsule_free(MultiDict_CAPI* capi)
{
    PyMem_Free(capi);
}

static void
capsule_destructor(PyObject* o)
{
    MultiDict_CAPI* capi = PyCapsule_GetPointer(o, MultiDict_CAPSULE_NAME);
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
    capi->MultiDict_GetType = MultiDict_GetType;
    capi->MultiDict_New = MultiDict_New;
    capi->MultiDict_Add = MultiDict_Add;
    capi->MultiDict_Clear = MultiDict_Clear;
    capi->MultiDict_SetDefault = MultiDict_SetDefault;
    capi->MultiDict_Del = MultiDict_Del;
    capi->MultiDict_Version = MultiDict_Version;
    capi->MultiDict_Contains = MultiDict_Contains;
    capi->MultiDict_GetOne = MultiDict_GetOne;
    capi->MultiDict_GetAll = MultiDict_GetAll;
    capi->MultiDict_PopOne = MultiDict_PopOne;
    capi->MultiDict_PopAll = MultiDict_PopAll;
    capi->MultiDict_PopItem = MultiDict_PopItem;
    capi->MultiDict_Replace = MultiDict_Replace;
    capi->MultiDict_UpdateFromMultiDict = MultiDict_UpdateFromMultiDict;
    capi->MultiDict_UpdateFromDict = MultiDict_UpdateFromDict;
    capi->MultiDict_UpdateFromSequence = MultiDict_UpdateFromSequence;

    capi->MultiDictProxy_New = MultiDictProxy_New;
    capi->MultiDictProxy_Contains = MultiDictProxy_Contains;
    capi->MultiDictProxy_GetAll = MultiDictProxy_GetAll;
    capi->MultiDictProxy_GetOne = MultiDictProxy_GetOne;
    capi->MultiDictProxy_GetType = MultiDictProxy_GetType;

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
