#ifndef _MULTIDICT_CAPSULE_H
#define _MULTIDICT_CAPSULE_H

#ifdef __cplusplus
extern "C" {
#endif

#define MULTIDICT_IMPL

#include "../multidict_api.h"
#include "dict.h"
#include "hashtable.h"
#include "iter.h"
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

#define __ANYMULTIDICT_VALIDATION_CHECK(SELF, STATE, ON_FAIL)                 \
    if (AnyMultiDict_Check(((mod_state*)STATE), (SELF)) <= 0) {               \
        PyErr_Format(PyExc_TypeError,                                         \
                     #SELF                                                    \
                     " should be a CIMultiDict or MultiDict instance not %s", \
                     Py_TYPE(SELF)->tp_name);                                 \
        return ON_FAIL;                                                       \
    }

#define __CIMULTIDICT_VALIDATION_CHECK(SELF, STATE, ON_FAIL)           \
    if (CIMultiDict_Check(((mod_state*)STATE), (SELF)) <= 0) {         \
        PyErr_Format(PyExc_TypeError,                                  \
                     #SELF " should be a CIMultiDict instance not %s", \
                     Py_TYPE(SELF)->tp_name);                          \
        return ON_FAIL;                                                \
    }

#define __CIMULTIDICTPROXY_VALIDATION_CHECK(SELF, STATE, ON_FAIL)           \
    if (CIMultiDictProxy_Check(((mod_state*)STATE), (SELF)) <= 0) {         \
        PyErr_Format(PyExc_TypeError,                                       \
                     #SELF " should be a CIMultiDictProxy instance not %s", \
                     Py_TYPE(SELF)->tp_name);                               \
        return ON_FAIL;                                                     \
    }

#define __CIMULTIDICTPROXY_GET_MD(SELF) ((MultiDictProxyObject*)SELF)->md

#define __CAPI_NULL_CHECK(POS, ON_FAIL) \
    if (POS == NULL) {                  \
        PyErr_NoMemory();               \
        return ON_FAIL;                 \
    }

#define __CAPI_ALLOC_POS(POS)                       \
    md_pos_t* POS = PyMem_Malloc(sizeof(md_pos_t)); \
    __CAPI_NULL_CHECK(POS, NULL)

#define __CAPI_FREE_POS(POS, SELF, TYPENAME)                                  \
    if (pos != NULL) {                                                        \
        if (((md_pos_t*)pos)->version != ((MultiDictObject*)self)->version) { \
            PyErr_Format(PyExc_RuntimeError,                                  \
                         "%s changed during cleanup after iteration",         \
                         #TYPENAME);                                          \
            return -1;                                                        \
        }                                                                     \
        PyMem_Free(pos);                                                      \
    }

/* ================= MultiDict ================= */

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

/* ================= MultiDictProxy ================= */

static PyObject*
MultiDictProxy_New(void* state_, PyObject* md)
{
    // This is meant to be a more optimized version of
    // multidict_proxy_tp_init(...)

    mod_state* state = (mod_state*)state_;
    PyObject* self =
        state->MultiDictProxyType->tp_alloc(state->MultiDictProxyType, 0);
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
    MultiDictObject* md_object;
    if (AnyMultiDictProxy_Check(state, md)) {
        md_object = ((MultiDictProxyObject*)md)->md;
    } else {
        md_object = (MultiDictObject*)md;
    }
    Py_INCREF(md_object);
    ((MultiDictProxyObject*)self)->md = md_object;
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

/* ================= istr ================= */

static PyObject*
IStr_FromUnicode(void* state_, PyObject* str)
{
    if (!PyUnicode_Check(str)) {
        PyErr_Format(PyExc_TypeError,
                     "str argument should be a str type object not \"%s\"",
                     Py_TYPE(str)->tp_name);
        return NULL;
    }

    mod_state* state = (mod_state*)state_;
    PyObject* canonical = PyObject_CallMethodNoArgs(str, state->str_lower);
    if (!canonical) {
        return NULL;
    };
    return IStr_New(state, str, canonical);
}

// Anybody using a bytes/buffer object or another C datatype should be able to
// convert to istr without issue so making a IStr_FromStringAndSize function
// made sense.

// However in cython if your using a straight up static string, use
// IStr_FromUnicode because the cython-compiler should immediately reconize and
// know what to do with it.

static PyObject*
IStr_FromStringAndSize(void* state_, const char* str, Py_ssize_t size)
{
    // Better done sooner than sorry...
    if (size == PY_SSIZE_T_MAX) {
        return PyErr_NoMemory();
    }

    PyUnicodeWriter* lc_writer = PyUnicodeWriter_Create(size);
    if (!lc_writer) {
        return NULL;
    }

    PyObject* str_obj = PyUnicode_FromKindAndData(
        PyUnicode_1BYTE_KIND, (unsigned char*)str, size);
    if (!str_obj) {
        goto fail;
    }

    for (Py_ssize_t i = 0; i < size; i++) {
        // This operation of lowercasing letters comes from llhttp
        // (more specifially autogenerated from llparse)
        // it is a slightly optimized version of C's tolower function
        unsigned char c = (unsigned char)(str[i]);
        if (PyUnicodeWriter_WriteChar(
                lc_writer, (c) >= 'A' && (c) <= 'Z' ? (c | 0x20) : (c)) < 0) {
            Py_XDECREF(str_obj);
            goto fail;
        }
    }
    PyObject* canonical = PyUnicodeWriter_Finish(lc_writer);
    return IStr_New((mod_state*)state_, str_obj, canonical);

fail:
    PyUnicodeWriter_Discard(lc_writer);
    return NULL;
}

static PyObject*
IStr_FromString(void* state_, const char* str)
{
    return IStr_FromStringAndSize(state_, str, (Py_ssize_t)strlen(str));
}

static PyTypeObject*
IStr_GetType(void* state_)
{
    mod_state* state = (mod_state*)state_;
    return (PyTypeObject*)Py_NewRef(state->IStrType);
}

/* ================= CIMultiDict ================= */

static PyTypeObject*
CIMultiDict_GetType(void* state_)
{
    mod_state* state = (mod_state*)state_;
    return (PyTypeObject*)Py_NewRef(state->CIMultiDictType);
}

static PyObject*
CIMultiDict_New(void* state_, int prealloc_size)
{
    mod_state* state = (mod_state*)state_;
    MultiDictObject* md = (MultiDictObject*)state->CIMultiDictType->tp_alloc(
        state->CIMultiDictType, 0);

    if (md == NULL) {
        return NULL;
    }
    if (md_init(md, state, true, prealloc_size) < 0) {
        Py_CLEAR(md);
        return NULL;
    }
    return (PyObject*)md;
}

static int
CIMultiDict_Add(void* state_, PyObject* self, PyObject* key, PyObject* value)
{
    __CIMULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_add((MultiDictObject*)self, key, value);
}

static int
CIMultiDict_Clear(void* state_, PyObject* self)
{
    // TODO: Macro for repeated steps being done?
    __CIMULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_clear((MultiDictObject*)self);
}

static int
CIMultiDict_SetDefault(void* state_, PyObject* self, PyObject* key,
                       PyObject* value, PyObject** result)
{
    __CIMULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_set_default((MultiDictObject*)self, key, value, result);
}

static int
CIMultiDict_Del(void* state_, PyObject* self, PyObject* key)
{
    __CIMULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_del((MultiDictObject*)self, key);
}

static uint64_t
CIMultiDict_Version(void* state_, PyObject* self)
{
    __CIMULTIDICT_VALIDATION_CHECK(self, state_, 0);
    return md_version((MultiDictObject*)self);
}

static int
CIMultiDict_Contains(void* state_, PyObject* self, PyObject* key)
{
    __CIMULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_contains((MultiDictObject*)self, key, NULL);
}

static int
CIMultiDict_GetOne(void* state_, PyObject* self, PyObject* key,
                   PyObject** result)
{
    __CIMULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_get_one((MultiDictObject*)self, key, result);
}

static int
CIMultiDict_GetAll(void* state_, PyObject* self, PyObject* key,
                   PyObject** result)
{
    __CIMULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_get_all((MultiDictObject*)self, key, result);
}

static int
CIMultiDict_PopOne(void* state_, PyObject* self, PyObject* key,
                   PyObject** result)
{
    __CIMULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_pop_one((MultiDictObject*)self, key, result);
}

static int
CIMultiDict_PopAll(void* state_, PyObject* self, PyObject* key,
                   PyObject** result)
{
    __CIMULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_pop_all((MultiDictObject*)self, key, result);
}

static PyObject*
CIMultiDict_PopItem(void* state_, PyObject* self)
{
    __CIMULTIDICT_VALIDATION_CHECK(self, state_, NULL);
    return md_pop_item((MultiDictObject*)self);
}

static int
CIMultiDict_Replace(void* state_, PyObject* self, PyObject* key,
                    PyObject* value)
{
    __CIMULTIDICT_VALIDATION_CHECK(self, state_, -1);
    return md_replace((MultiDictObject*)self, key, value);
}

static int
CIMultiDict_UpdateFromMultiDict(void* state_, PyObject* self, PyObject* other,
                                UpdateOp op)
{
    __CIMULTIDICT_VALIDATION_CHECK(self, state_, -1);
    __ANYMULTIDICT_VALIDATION_CHECK(other, state_, -1);
    int ret =
        md_update_from_ht((MultiDictObject*)self, (MultiDictObject*)other, op);
    if (op != Extend) {
        md_post_update((MultiDictObject*)self);
    }
    return ret;
}

static int
CIMultiDict_UpdateFromDict(void* state_, PyObject* self, PyObject* other,
                           UpdateOp op)
{
    __CIMULTIDICT_VALIDATION_CHECK(self, state_, -1);
    if (PyDict_CheckExact(other) <= 0) {
        PyErr_Format(PyExc_TypeError,
                     "other should be a CIMultiDict instance not %s",
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
CIMultiDict_UpdateFromSequence(void* state_, PyObject* self, PyObject* seq,
                               UpdateOp op)
{
    __CIMULTIDICT_VALIDATION_CHECK(self, state_, -1);
    int ret = md_update_from_seq((MultiDictObject*)self, seq, op);
    if (op != Extend) {
        md_post_update((MultiDictObject*)self);
    }
    return ret;
}

/* ================= CIMultiDictProxy ================= */

static PyObject*
CIMultiDictProxy_New(void* state_, PyObject* md)
{
    mod_state* state = (mod_state*)state_;
    PyObject* self =
        state->CIMultiDictProxyType->tp_alloc(state->CIMultiDictProxyType, 0);
    if (self == NULL) {
        return NULL;
    }
    if (!CIMultiDictProxy_Check(((mod_state*)state_), md) &&
        !CIMultiDict_Check(state, md)) {
        PyErr_Format(PyExc_TypeError,
                     "md requires CIMultiDict or CIMultiDictProxy instance, "
                     "not <class '%s'>",
                     Py_TYPE(md)->tp_name);
        goto fail;
    }
    MultiDictObject* md_object;
    if (CIMultiDictProxy_Check(state, md)) {
        md_object = ((MultiDictProxyObject*)md)->md;
    } else {
        md_object = (MultiDictObject*)md;
    }
    Py_INCREF(md_object);
    ((MultiDictProxyObject*)self)->md = md_object;
    return self;
fail:
    Py_XDECREF(self);
    return NULL;
}

static int
CIMultiDictProxy_Contains(void* state_, PyObject* self, PyObject* key)
{
    __CIMULTIDICTPROXY_VALIDATION_CHECK(self, state_, -1);
    return md_contains(__CIMULTIDICTPROXY_GET_MD(self), key, NULL);
}

static int
CIMultiDictProxy_GetAll(void* state_, PyObject* self, PyObject* key,
                        PyObject** result)
{
    __CIMULTIDICTPROXY_VALIDATION_CHECK(self, state_, -1);
    return md_get_all(__CIMULTIDICTPROXY_GET_MD(self), key, result);
}

static int
CIMultiDictProxy_GetOne(void* state_, PyObject* self, PyObject* key,
                        PyObject** result)
{
    __CIMULTIDICTPROXY_VALIDATION_CHECK(self, state_, -1);
    return md_get_one(__CIMULTIDICTPROXY_GET_MD(self), key, result);
}

static PyTypeObject*
CIMultiDictProxy_GetType(void* state_)
{
    mod_state* state = (mod_state*)state_;
    return (PyTypeObject*)Py_NewRef(state->CIMultiDictProxyType);
}

/* ================== MultiDictIter ================== */

// Creates a new iterator from MultiDict, CIMultiDict
// MutliDictProxy & CIMultiDictProxy
static PyObject*
MultiDictIter_New(void* state_, PyObject* self)
{
    mod_state* state = (mod_state*)state_;
    MultiDictObject* md;
    if (MultiDict_Check(state, self) || CIMultiDict_Check(state, self)) {
        md = (MultiDictObject*)self;
    } else if (MultiDictProxy_Check(state, self) ||
               CIMultiDictProxy_Check(state, self)) {
        md = __MULTIDICTPROXY_GET_MD(self);
    } else {
        PyErr_Format(PyExc_TypeError,
                     "Expected MultiDict, CIMultiDict, MultiDictProxy"
                     " or CIMultiDictProxy type object not %s",
                     Py_TYPE(self)->tp_name);
        return NULL;
    }
    return multidict_items_iter_new(md);
}

static int
MultiDictIter_Next(void* state_, PyObject* self, PyObject** key,
                   PyObject** value)
{
    mod_state* state = (mod_state*)state_;
    if (!Py_IS_TYPE(self, state->ItemsIterType)) {
        PyErr_Format(PyExc_TypeError,
                     "Expected A MultiDict itemsiter type not %s",
                     Py_TYPE(self)->tp_name);
        return -1;
    }
    MultidictIter* iter = (MultidictIter*)self;
    return md_next(iter->md, &iter->current, NULL, key, value);
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

    capi->IStr_FromUnicode = IStr_FromUnicode;
    capi->IStr_FromStringAndSize = IStr_FromStringAndSize;
    capi->IStr_FromString = IStr_FromString;
    capi->IStr_GetType = IStr_GetType;

    capi->CIMultiDict_GetType = CIMultiDict_GetType;
    capi->CIMultiDict_New = CIMultiDict_New;
    capi->CIMultiDict_Add = CIMultiDict_Add;
    capi->CIMultiDict_Clear = CIMultiDict_Clear;
    capi->CIMultiDict_SetDefault = CIMultiDict_SetDefault;
    capi->CIMultiDict_Del = CIMultiDict_Del;
    capi->CIMultiDict_Version = CIMultiDict_Version;
    capi->CIMultiDict_Contains = CIMultiDict_Contains;
    capi->CIMultiDict_GetOne = CIMultiDict_GetOne;
    capi->CIMultiDict_GetAll = CIMultiDict_GetAll;
    capi->CIMultiDict_PopOne = CIMultiDict_PopOne;
    capi->CIMultiDict_PopAll = CIMultiDict_PopAll;
    capi->CIMultiDict_PopItem = CIMultiDict_PopItem;
    capi->CIMultiDict_Replace = CIMultiDict_Replace;
    capi->CIMultiDict_UpdateFromMultiDict = CIMultiDict_UpdateFromMultiDict;
    capi->CIMultiDict_UpdateFromDict = CIMultiDict_UpdateFromDict;
    capi->CIMultiDict_UpdateFromSequence = CIMultiDict_UpdateFromSequence;

    capi->CIMultiDictProxy_New = CIMultiDictProxy_New;
    capi->CIMultiDictProxy_Contains = CIMultiDictProxy_Contains;
    capi->CIMultiDictProxy_GetAll = CIMultiDictProxy_GetAll;
    capi->CIMultiDictProxy_GetOne = CIMultiDictProxy_GetOne;
    capi->CIMultiDictProxy_GetType = CIMultiDictProxy_GetType;

    capi->MultiDictIter_New = MultiDictIter_New;
    capi->MultiDictIter_Next = MultiDictIter_Next;

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
