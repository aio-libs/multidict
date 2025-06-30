#ifndef _MULTIDICT_API_H
#define _MULTIDICT_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <assert.h>
#include <stdbool.h>

#define MultiDict_MODULE_NAME "multidict._multidict"
#define MultiDict_CAPI_NAME "CAPI"
#define MultiDict_CAPSULE_NAME MultiDict_MODULE_NAME "." MultiDict_CAPI_NAME

typedef enum _UpdateOp {
    Extend = 0,
    Update = 1,
    Merge = 2,
} UpdateOp;

typedef struct {
    /* N.B.

       For the sake of backward and future compatibility,
       new fields should be added at the end of the structure,
       unused fields should be never removed.

       Otherwise, it could lead to crashes with memory corruptions
       if the client is compiled with older multidict_api.h header
    */

    void* state;

    PyTypeObject* (*MultiDict_GetType)(void* state);

    PyObject* (*MultiDict_New)(void* state, int prealloc_size);
    int (*MultiDict_Add)(void* state, PyObject* self, PyObject* key,
                         PyObject* value);

    int (*MultiDict_Clear)(void* state, PyObject* self);

    int (*MultiDict_SetDefault)(void* state, PyObject* self, PyObject* key,
                                PyObject* default_, PyObject** result);

    int (*MultiDict_Del)(void* state, PyObject* self, PyObject* key);
    uint64_t (*MultiDict_Version)(void* state, PyObject* self);

    int (*MultiDict_Contains)(void* state, PyObject* self, PyObject* key);
    int (*MultiDict_GetOne)(void* state, PyObject* self, PyObject* key,
                            PyObject** result);
    int (*MultiDict_GetAll)(void* state, PyObject* self, PyObject* key,
                            PyObject** result);
    int (*MultiDict_PopOne)(void* state, PyObject* self, PyObject* key,
                            PyObject** result);
    int (*MultiDict_PopAll)(void* state, PyObject* self, PyObject* key,
                            PyObject** result);
    PyObject* (*MultiDict_PopItem)(void* state, PyObject* self);
    int (*MultiDict_Replace)(void* state, PyObject* self, PyObject* key,
                             PyObject* value);
    int (*MultiDict_UpdateFromMultiDict)(void* state, PyObject* self,
                                         PyObject* other, UpdateOp op);
    int (*MultiDict_UpdateFromDict)(void* state, PyObject* self,
                                    PyObject* kwds, UpdateOp op);
    int (*MultiDict_UpdateFromSequence)(void* state, PyObject* self,
                                        PyObject* kwds, UpdateOp op);

    PyObject* (*MultiDictProxy_New)(void* state, PyObject* md);
    int (*MultiDictProxy_Contains)(void* state, PyObject* self, PyObject* key);
    int (*MultiDictProxy_GetAll)(void* state, PyObject* self, PyObject* key,
                                 PyObject** result);
    int (*MultiDictProxy_GetOne)(void* state, PyObject* self, PyObject* key,
                                 PyObject** result);
    PyTypeObject* (*MultiDictProxy_GetType)(void* state);

    PyObject* (*IStr_FromUnicode)(void* state, PyObject* str);
    PyObject* (*IStr_FromStringAndSize)(void* state, const char* str,
                                        Py_ssize_t size);
    PyObject* (*IStr_FromString)(void* state, const char* str);
    PyTypeObject* (*IStr_GetType)(void* state);

    PyTypeObject* (*CIMultiDict_GetType)(void* state);

    PyObject* (*CIMultiDict_New)(void* state, int prealloc_size);
    int (*CIMultiDict_Add)(void* state, PyObject* self, PyObject* key,
                           PyObject* value);
    int (*CIMultiDict_Clear)(void* state, PyObject* self);

    int (*CIMultiDict_SetDefault)(void* state, PyObject* self, PyObject* key,
                                  PyObject* default_, PyObject** result);

    int (*CIMultiDict_Del)(void* state, PyObject* self, PyObject* key);
    uint64_t (*CIMultiDict_Version)(void* state, PyObject* self);

    int (*CIMultiDict_Contains)(void* state, PyObject* self, PyObject* key);
    int (*CIMultiDict_GetOne)(void* state, PyObject* self, PyObject* key,
                              PyObject** result);
    int (*CIMultiDict_GetAll)(void* state, PyObject* self, PyObject* key,
                              PyObject** result);
    int (*CIMultiDict_PopOne)(void* state, PyObject* self, PyObject* key,
                              PyObject** result);
    int (*CIMultiDict_PopAll)(void* state, PyObject* self, PyObject* key,
                              PyObject** result);
    PyObject* (*CIMultiDict_PopItem)(void* state, PyObject* self);
    int (*CIMultiDict_Replace)(void* state, PyObject* self, PyObject* key,
                               PyObject* value);
    int (*CIMultiDict_UpdateFromMultiDict)(void* state, PyObject* self,
                                           PyObject* other, UpdateOp op);
    int (*CIMultiDict_UpdateFromDict)(void* state, PyObject* self,
                                      PyObject* kwds, UpdateOp op);
    int (*CIMultiDict_UpdateFromSequence)(void* state, PyObject* self,
                                          PyObject* kwds, UpdateOp op);

    PyObject* (*CIMultiDictProxy_New)(void* state, PyObject* md);
    int (*CIMultiDictProxy_Contains)(void* state, PyObject* self,
                                     PyObject* key);
    int (*CIMultiDictProxy_GetAll)(void* state, PyObject* self, PyObject* key,
                                   PyObject** result);
    int (*CIMultiDictProxy_GetOne)(void* state, PyObject* self, PyObject* key,
                                   PyObject** result);
    PyTypeObject* (*CIMultiDictProxy_GetType)(void* state);

} MultiDict_CAPI;

#ifndef MULTIDICT_IMPL

static inline MultiDict_CAPI*
MultiDict_Import()
{
    return (MultiDict_CAPI*)PyCapsule_Import(MultiDict_CAPSULE_NAME, 0);
}

static inline PyTypeObject*
MultiDict_GetType(MultiDict_CAPI* api)
{
    return api->MultiDict_GetType(api->state);
}

static inline int
MultiDict_CheckExact(MultiDict_CAPI* api, PyObject* op)
{
    PyTypeObject* type = api->MultiDict_GetType(api->state);
    int ret = Py_IS_TYPE(op, type);
    Py_DECREF(type);
    return ret;
}

static inline int
MultiDict_Check(MultiDict_CAPI* api, PyObject* op)
{
    PyTypeObject* type = api->MultiDict_GetType(api->state);
    int ret = Py_IS_TYPE(op, type) || PyObject_TypeCheck(op, type);
    Py_DECREF(type);
    return ret;
}

static inline PyObject*
MultiDict_New(MultiDict_CAPI* api, int prealloc_size)
{
    return api->MultiDict_New(api->state, prealloc_size);
}

static inline int
MultiDict_Add(MultiDict_CAPI* api, PyObject* self, PyObject* key,
              PyObject* value)
{
    return api->MultiDict_Add(api->state, self, key, value);
}

static inline int
MultiDict_Clear(MultiDict_CAPI* api, PyObject* self)
{
    return api->MultiDict_Clear(api->state, self);
}

static inline int
MultiDict_SetDefault(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                     PyObject* default_, PyObject** result)
{
    return api->MultiDict_SetDefault(api->state, self, key, default_, result);
}

static inline int
MutliDict_Del(MultiDict_CAPI* api, PyObject* self, PyObject* key)
{
    return api->MultiDict_Del(api->state, self, key);
}

static uint64_t
MultiDict_Version(MultiDict_CAPI* api, PyObject* self)
{
    return api->MultiDict_Version(api->state, self);
}

static inline int
MultiDict_Contains(MultiDict_CAPI* api, PyObject* self, PyObject* key)
{
    return api->MultiDict_Contains(api->state, self, key);
}

static inline int
MultiDict_GetOne(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                 PyObject** result)
{
    return api->MultiDict_GetOne(api->state, self, key, result);
}

static inline int
MultiDict_GetAll(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                 PyObject** result)
{
    return api->MultiDict_GetAll(api->state, self, key, result);
}

static inline int
MultiDict_PopOne(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                 PyObject** result)
{
    return api->MultiDict_PopOne(api->state, self, key, result);
}

static inline int
MultiDict_PopAll(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                 PyObject** result)
{
    return api->MultiDict_PopAll(api->state, self, key, result);
}

static inline PyObject*
MultiDict_PopItem(MultiDict_CAPI* api, PyObject* self)
{
    return api->MultiDict_PopItem(api->state, self);
}

static inline int
MultiDict_Replace(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                  PyObject* value)
{
    return api->MultiDict_Replace(api->state, self, key, value);
};

static inline int
MultiDict_UpdateFromMultiDict(MultiDict_CAPI* api, PyObject* self,
                              PyObject* other, UpdateOp op)
{
    return api->MultiDict_UpdateFromMultiDict(api->state, self, other, op);
};

static inline int
MultiDict_UpdateFromDict(MultiDict_CAPI* api, PyObject* self, PyObject* other,
                         UpdateOp op)
{
    return api->MultiDict_UpdateFromDict(api->state, self, other, op);
};

static inline int
MultiDict_UpdateFromSequence(MultiDict_CAPI* api, PyObject* self,
                             PyObject* seq, UpdateOp op)
{
    return api->MultiDict_UpdateFromSequence(api->state, self, seq, op);
};

static inline PyObject*
MultiDictProxy_New(MultiDict_CAPI* api, PyObject* md)
{
    return api->MultiDictProxy_New(api->state, md);
}

static inline int
MultiDictProxy_CheckExact(MultiDict_CAPI* api, PyObject* op)
{
    PyTypeObject* type = api->MultiDict_GetType(api->state);
    int ret = Py_IS_TYPE(op, type);
    Py_DECREF(type);
    return ret;
}

static inline int
MultiDictProxy_Check(MultiDict_CAPI* api, PyObject* op)
{
    PyTypeObject* type = api->MultiDictProxy_GetType(api->state);
    int ret = Py_IS_TYPE(op, type) || PyObject_TypeCheck(op, type);
    Py_DECREF(type);
    return ret;
}

static inline int
MultiDictProxy_Contains(MultiDict_CAPI* api, PyObject* self, PyObject* key)
{
    return api->MultiDictProxy_Contains(api->state, self, key);
}

static inline int
MultiDictProxy_GetAll(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                      PyObject** result)
{
    return api->MultiDictProxy_GetAll(api->state, self, key, result);
}

static inline int
MultiDictProxy_GetOne(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                      PyObject** result)
{
    return api->MultiDictProxy_GetOne(api->state, self, key, result);
}

static inline PyTypeObject*
MultiDictProxy_GetType(MultiDict_CAPI* api)
{
    return api->MultiDictProxy_GetType(api->state);
}

static inline int
IStr_CheckExact(MultiDict_CAPI* api, PyObject* op)
{
    PyTypeObject* type = api->IStr_GetType(api->state);
    int ret = Py_IS_TYPE(op, type);
    Py_DECREF(type);
    return ret;
}

static inline int
IStr_Check(MultiDict_CAPI* api, PyObject* op)
{
    PyTypeObject* type = api->IStr_GetType(api->state);
    int ret = Py_IS_TYPE(op, type) || PyObject_TypeCheck(op, type);
    Py_DECREF(type);
    return ret;
}

static PyObject*
IStr_FromUnicode(MultiDict_CAPI* api, PyObject* str)
{
    return api->IStr_FromUnicode(api->state, str);
}

static PyObject*
IStr_FromStringAndSize(MultiDict_CAPI* api, const char* str, Py_ssize_t size)
{
    return api->IStr_FromStringAndSize(api->state, str, size);
}

static PyObject*
IStr_FromString(MultiDict_CAPI* api, const char* str)
{
    return api->IStr_FromString(api->state, str);
}

static inline PyTypeObject*
IStr_GetType(MultiDict_CAPI* api)
{
    return api->IStr_GetType(api->state);
}

static inline PyTypeObject*
CIMultiDict_GetType(MultiDict_CAPI* api)
{
    return api->CIMultiDict_GetType(api->state);
}

static inline int
CIMultiDict_CheckExact(MultiDict_CAPI* api, PyObject* op)
{
    PyTypeObject* type = api->MultiDict_GetType(api->state);
    int ret = Py_IS_TYPE(op, type);
    Py_DECREF(type);
    return ret;
}

static inline int
CIMultiDict_Check(MultiDict_CAPI* api, PyObject* op)
{
    PyTypeObject* type = api->MultiDict_GetType(api->state);
    int ret = Py_IS_TYPE(op, type) || PyObject_TypeCheck(op, type);
    Py_DECREF(type);
    return ret;
}

static inline PyObject*
CIMultiDict_New(MultiDict_CAPI* api, int prealloc_size)
{
    return api->CIMultiDict_New(api->state, prealloc_size);
}

static inline int
CIMultiDict_Add(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                PyObject* value)
{
    return api->CIMultiDict_Add(api->state, self, key, value);
}

static inline int
CIMultiDict_Clear(MultiDict_CAPI* api, PyObject* self)
{
    return api->CIMultiDict_Clear(api->state, self);
}

static inline int
CIMultiDict_SetDefault(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                       PyObject* default_, PyObject** result)
{
    return api->CIMultiDict_SetDefault(
        api->state, self, key, default_, result);
}

static inline int
CIMutliDict_Del(MultiDict_CAPI* api, PyObject* self, PyObject* key)
{
    return api->CIMultiDict_Del(api->state, self, key);
}

static uint64_t
CIMultiDict_Version(MultiDict_CAPI* api, PyObject* self)
{
    return api->CIMultiDict_Version(api->state, self);
}

static inline int
CIMultiDict_Contains(MultiDict_CAPI* api, PyObject* self, PyObject* key)
{
    return api->CIMultiDict_Contains(api->state, self, key);
}

static inline int
CIMultiDict_GetOne(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                   PyObject** result)
{
    return api->CIMultiDict_GetOne(api->state, self, key, result);
}

static inline int
CIMultiDict_GetAll(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                   PyObject** result)
{
    return api->CIMultiDict_GetAll(api->state, self, key, result);
}

static inline int
CIMultiDict_PopOne(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                   PyObject** result)
{
    return api->CIMultiDict_PopOne(api->state, self, key, result);
}

static inline int
CIMultiDict_PopAll(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                   PyObject** result)
{
    return api->CIMultiDict_PopAll(api->state, self, key, result);
}

static inline PyObject*
CIMultiDict_PopItem(MultiDict_CAPI* api, PyObject* self)
{
    return api->CIMultiDict_PopItem(api->state, self);
}

static inline int
CIMultiDict_Replace(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                    PyObject* value)
{
    return api->CIMultiDict_Replace(api->state, self, key, value);
};

static inline int
CIMultiDict_UpdateFromMultiDict(MultiDict_CAPI* api, PyObject* self,
                                PyObject* other, UpdateOp op)
{
    return api->CIMultiDict_UpdateFromMultiDict(api->state, self, other, op);
};

static inline int
CIMultiDict_UpdateFromDict(MultiDict_CAPI* api, PyObject* self,
                           PyObject* other, UpdateOp op)
{
    return api->CIMultiDict_UpdateFromDict(api->state, self, other, op);
};

static inline int
CIMultiDict_UpdateFromSequence(MultiDict_CAPI* api, PyObject* self,
                               PyObject* seq, UpdateOp op)
{
    return api->CIMultiDict_UpdateFromSequence(api->state, self, seq, op);
};

static inline PyObject*
CIMultiDictProxy_New(MultiDict_CAPI* api, PyObject* md)
{
    return api->CIMultiDictProxy_New(api->state, md);
}

static inline int
CIMultiDictProxy_CheckExact(MultiDict_CAPI* api, PyObject* op)
{
    PyTypeObject* type = api->CIMultiDictProxy_GetType(api->state);
    int ret = Py_IS_TYPE(op, type);
    Py_DECREF(type);
    return ret;
}

static inline int
CIMultiDictProxy_Check(MultiDict_CAPI* api, PyObject* op)
{
    PyTypeObject* type = api->CIMultiDictProxy_GetType(api->state);
    int ret = Py_IS_TYPE(op, type) || PyObject_TypeCheck(op, type);
    Py_DECREF(type);
    return ret;
}

static inline int
CIMultiDictProxy_Contains(MultiDict_CAPI* api, PyObject* self, PyObject* key)
{
    return api->CIMultiDictProxy_Contains(api->state, self, key);
}

static inline int
CIMultiDictProxy_GetAll(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                        PyObject** result)
{
    return api->CIMultiDictProxy_GetAll(api->state, self, key, result);
}

static inline int
CIMultiDictProxy_GetOne(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                        PyObject** result)
{
    return api->CIMultiDictProxy_GetOne(api->state, self, key, result);
}

static inline PyTypeObject*
CIMultiDictProxy_GetType(MultiDict_CAPI* api)
{
    return api->CIMultiDictProxy_GetType(api->state);
}

#endif

#ifdef __cplusplus
}
#endif

#endif
