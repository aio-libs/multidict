#ifndef _MULTIDICT_API_H
#define _MULTIDICT_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include "multidict_capi_struct.h"

static inline int
_MultiDict_CheckAPIVersion(MultiDict_CAPI* capi)
{
    if (capi->api_version < MultiDict_CAPI_VERSION) {
        PyErr_Format(PyExc_RuntimeError,
                     "multidict C API version mismatch: this code was built "
                     "against multidict_capi.h version %d, but the "
                     "installed multidict only provides version %d; "
                     "upgrade multidict",
                     MultiDict_CAPI_VERSION,
                     capi->api_version);
        return -1;
    }
    return 0;
}

static inline MultiDict_CAPI*
MultiDict_GetCAPI(void)
{
    MultiDict_CAPI* capi =
        (MultiDict_CAPI*)PyCapsule_Import(MultiDict_CAPSULE_NAME, 0);
    if (capi == NULL) {
        return NULL;
    }
    if (_MultiDict_CheckAPIVersion(capi) < 0) {
        return NULL;
    }
    return capi;
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

static inline Py_ssize_t
MultiDict_Size(MultiDict_CAPI* capi, PyObject* self)
{
    return capi->MultiDict_Size(capi->state, self);
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

static inline Py_ssize_t
MultiDict_ForEach(MultiDict_CAPI* capi, PyObject* self, PyObject* key,
                  MultiDict_ItemVisitor visitor, void* user_data)
{
    return capi->MultiDict_ForEach(capi->state, self, key, visitor, user_data);
}

static inline int
MultiDict_AddWatcher(MultiDict_CAPI* capi, MultiDict_WatchCallback callback,
                     void* watcher_data)
{
    return capi->MultiDict_AddWatcher(capi->state, callback, watcher_data);
}

static inline int
MultiDict_ClearWatcher(MultiDict_CAPI* capi, int watcher_id)
{
    return capi->MultiDict_ClearWatcher(capi->state, watcher_id);
}

static inline int
MultiDict_Watch(MultiDict_CAPI* capi, int watcher_id, PyObject* self,
                void* user_data)
{
    return capi->MultiDict_Watch(capi->state, watcher_id, self, user_data);
}

static inline int
MultiDict_Unwatch(MultiDict_CAPI* capi, int watcher_id, PyObject* self)
{
    return capi->MultiDict_Unwatch(capi->state, watcher_id, self);
}

#ifdef __cplusplus
}
#endif

#endif /* _MULTIDICT_API_H */
