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

inline static void
_invalid_type()
{
    PyErr_SetString(PyExc_TypeError, "self should be a MultiDict instance");
}

static PyTypeObject *
MultiDict_GetType(void *state_)
{
    mod_state *state = (mod_state *)state_;
    return (PyTypeObject *)Py_NewRef(state->MultiDictType);
}

static PyObject *
MultiDict_New(void *state_, int prealloc_size)
{
    mod_state *state = (mod_state *)state_;
    MultiDictObject *md =
        PyObject_GC_New(MultiDictObject, state->MultiDictType);
    if (md == NULL) {
        return NULL;
    }
    if (md_init(md, state, false, prealloc_size) < 0) {
        Py_CLEAR(md);
        return NULL;
    }
    PyObject_GC_Track(md);
    return (PyObject *)md;
}

static int
MultiDict_Add(void *state_, PyObject *self, PyObject *key, PyObject *value)
{
    mod_state *state = (mod_state *)state_;
    if (MultiDict_Check(state, self) <= 0) {
        _invalid_type();
        return -1;
    }
    return md_add((MultiDictObject *)self, key, value);
}

static int
MultiDict_Clear(void *state_, PyObject *self)
{
    mod_state *state = (mod_state *)state_;
    if (MultiDict_Check(state, self) <= 0) {
        _invalid_type();
        return -1;
    }
    return md_clear((MultiDictObject *)self);
}

static void
capsule_free(MultiDict_CAPI *capi)
{
    PyMem_Free(capi);
}

static void
capsule_destructor(PyObject *o)
{
    MultiDict_CAPI *capi = PyCapsule_GetPointer(o, MultiDict_CAPSULE_NAME);
    capsule_free(capi);
}

static PyObject *
new_capsule(mod_state *state)
{
    MultiDict_CAPI *capi =
        (MultiDict_CAPI *)PyMem_Malloc(sizeof(MultiDict_CAPI));
    if (capi == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    capi->state = state;
    capi->MultiDict_GetType = MultiDict_GetType;
    capi->MultiDict_New = MultiDict_New;
    capi->MultiDict_Add = MultiDict_Add;
    capi->MultiDict_Clear = MultiDict_Clear;

    PyObject *ret =
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
