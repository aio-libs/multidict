#ifndef _MULTIDICT_API_H
#define _MULTIDICT_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include "multidict_capi_struct.h"

static inline MultiDict_CAPI*
MultiDict_GetCAPI(void)
{
    return (MultiDict_CAPI*)PyCapsule_Import(MultiDict_CAPSULE_NAME, 0);
}

static inline PyTypeObject*
IStr_GetType(MultiDict_CAPI* capi)
{
    return capi->IStr_GetType(capi->state);
}

static inline int
IStr_CheckExact(MultiDict_CAPI* capi, PyObject* op)
{
    PyTypeObject* type = capi->IStr_GetType(capi->state);
    int ret = Py_IS_TYPE(op, type);
    Py_DECREF(type);
    return ret;
}

static inline int
IStr_Check(MultiDict_CAPI* capi, PyObject* op)
{
    PyTypeObject* type = capi->IStr_GetType(capi->state);
    int ret = Py_IS_TYPE(op, type) || PyObject_TypeCheck(op, type);
    Py_DECREF(type);
    return ret;
}

static inline PyObject*
IStr_FromUnicode(MultiDict_CAPI* capi, PyObject* str)
{
    return capi->IStr_FromUnicode(capi->state, str);
}

static inline uint64_t
MultiDict_GetVersion(MultiDict_CAPI* capi, PyObject* self)
{
    return capi->MultiDict_GetVersion(capi->state, self);
}

static inline PyTypeObject*
MultiDict_GetType(MultiDict_CAPI* capi)
{
    return capi->MultiDict_GetType(capi->state);
}

static inline int
MultiDict_CheckExact(MultiDict_CAPI* capi, PyObject* op)
{
    PyTypeObject* type = capi->MultiDict_GetType(capi->state);
    int ret = Py_IS_TYPE(op, type);
    Py_DECREF(type);
    return ret;
}

static inline int
MultiDict_Check(MultiDict_CAPI* capi, PyObject* op)
{
    PyTypeObject* type = capi->MultiDict_GetType(capi->state);
    int ret = Py_IS_TYPE(op, type) || PyObject_TypeCheck(op, type);
    Py_DECREF(type);
    return ret;
}

static inline PyTypeObject*
CIMultiDict_GetType(MultiDict_CAPI* capi)
{
    return capi->CIMultiDict_GetType(capi->state);
}

static inline int
CIMultiDict_CheckExact(MultiDict_CAPI* capi, PyObject* op)
{
    PyTypeObject* type = capi->CIMultiDict_GetType(capi->state);
    int ret = Py_IS_TYPE(op, type);
    Py_DECREF(type);
    return ret;
}

static inline int
CIMultiDict_Check(MultiDict_CAPI* capi, PyObject* op)
{
    PyTypeObject* type = capi->CIMultiDict_GetType(capi->state);
    int ret = Py_IS_TYPE(op, type) || PyObject_TypeCheck(op, type);
    Py_DECREF(type);
    return ret;
}

static inline PyTypeObject*
MultiDictProxy_GetType(MultiDict_CAPI* capi)
{
    return capi->MultiDictProxy_GetType(capi->state);
}

static inline int
MultiDictProxy_CheckExact(MultiDict_CAPI* capi, PyObject* op)
{
    PyTypeObject* type = capi->MultiDictProxy_GetType(capi->state);
    int ret = Py_IS_TYPE(op, type);
    Py_DECREF(type);
    return ret;
}

static inline int
MultiDictProxy_Check(MultiDict_CAPI* capi, PyObject* op)
{
    PyTypeObject* type = capi->MultiDictProxy_GetType(capi->state);
    int ret = Py_IS_TYPE(op, type) || PyObject_TypeCheck(op, type);
    Py_DECREF(type);
    return ret;
}

static inline PyTypeObject*
CIMultiDictProxy_GetType(MultiDict_CAPI* capi)
{
    return capi->CIMultiDictProxy_GetType(capi->state);
}

static inline int
CIMultiDictProxy_CheckExact(MultiDict_CAPI* capi, PyObject* op)
{
    PyTypeObject* type = capi->CIMultiDictProxy_GetType(capi->state);
    int ret = Py_IS_TYPE(op, type);
    Py_DECREF(type);
    return ret;
}

static inline int
CIMultiDictProxy_Check(MultiDict_CAPI* capi, PyObject* op)
{
    PyTypeObject* type = capi->CIMultiDictProxy_GetType(capi->state);
    int ret = Py_IS_TYPE(op, type) || PyObject_TypeCheck(op, type);
    Py_DECREF(type);
    return ret;
}

static inline PyObject*
MultiDict_New(MultiDict_CAPI* capi, Py_ssize_t prealloc_size)
{
    return capi->MultiDict_New(capi->state, prealloc_size);
}

static inline PyObject*
CIMultiDict_New(MultiDict_CAPI* capi, Py_ssize_t prealloc_size)
{
    return capi->CIMultiDict_New(capi->state, prealloc_size);
}

static inline PyObject*
MultiDictProxy_New(MultiDict_CAPI* capi, PyObject* arg)
{
    return capi->MultiDictProxy_New(capi->state, arg);
}

static inline PyObject*
CIMultiDictProxy_New(MultiDict_CAPI* capi, PyObject* arg)
{
    return capi->CIMultiDictProxy_New(capi->state, arg);
}

static inline int
MultiDict_Contains(MultiDict_CAPI* capi, PyObject* self, PyObject* key)
{
    return capi->MultiDict_Contains(capi->state, self, key);
}

static inline int
MultiDict_GetItem(MultiDict_CAPI* capi, PyObject* self, PyObject* key,
                  PyObject** result)
{
    return capi->MultiDict_GetItem(capi->state, self, key, result);
}

static inline Py_ssize_t
MultiDict_Size(MultiDict_CAPI* capi, PyObject* self)
{
    return capi->MultiDict_Size(capi->state, self);
}

static inline int
MultiDict_Add(MultiDict_CAPI* capi, PyObject* self, PyObject* key,
              PyObject* value)
{
    return capi->MultiDict_Add(capi->state, self, key, value);
}

static inline int
MultiDict_Clear(MultiDict_CAPI* capi, PyObject* self)
{
    return capi->MultiDict_Clear(capi->state, self);
}

static inline int
MultiDict_DelItem(MultiDict_CAPI* capi, PyObject* self, PyObject* key)
{
    return capi->MultiDict_DelItem(capi->state, self, key);
}

static inline int
MultiDict_Pop(MultiDict_CAPI* capi, PyObject* self, PyObject* key,
              PyObject** result)
{
    return capi->MultiDict_Pop(capi->state, self, key, result);
}

static inline int
MultiDict_SetDefault(MultiDict_CAPI* capi, PyObject* self, PyObject* key,
                     PyObject* default_value, PyObject** result)
{
    return capi->MultiDict_SetDefault(
        capi->state, self, key, default_value, result);
}

static inline int
MultiDict_SetItem(MultiDict_CAPI* capi, PyObject* self, PyObject* key,
                  PyObject* value)
{
    return capi->MultiDict_SetItem(capi->state, self, key, value);
}

#ifdef __cplusplus
}
#endif

#endif /* _MULTIDICT_API_H */
