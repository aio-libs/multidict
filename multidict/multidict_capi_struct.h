#ifndef _MULTIDICT_CAPI_STRUCT_H
#define _MULTIDICT_CAPI_STRUCT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <stdint.h>

#define MultiDict_MODULE_NAME "multidict._multidict"
#define MultiDict_CAPI_NAME "CAPI"
#define MultiDict_CAPSULE_NAME MultiDict_MODULE_NAME "." MultiDict_CAPI_NAME

typedef struct {
    void* state;

    PyTypeObject* (*IStr_GetType)(void* state);
    PyObject* (*IStr_FromUnicode)(void* state, PyObject* str);

    uint64_t (*MultiDict_GetVersion)(void* state, PyObject* self);

    PyTypeObject* (*MultiDict_GetType)(void* state);
    PyTypeObject* (*CIMultiDict_GetType)(void* state);
    PyTypeObject* (*MultiDictProxy_GetType)(void* state);
    PyTypeObject* (*CIMultiDictProxy_GetType)(void* state);

    PyObject* (*MultiDict_New)(void* state, Py_ssize_t prealloc_size);
    PyObject* (*CIMultiDict_New)(void* state, Py_ssize_t prealloc_size);
    PyObject* (*MultiDictProxy_New)(void* state, PyObject* arg);
    PyObject* (*CIMultiDictProxy_New)(void* state, PyObject* arg);

    int (*MultiDict_Contains)(void* state, PyObject* self, PyObject* key);
    int (*MultiDict_GetItem)(void* state, PyObject* self, PyObject* key,
                             PyObject** result);
    Py_ssize_t (*MultiDict_Size)(void* state, PyObject* self);

    int (*MultiDict_Add)(void* state, PyObject* self, PyObject* key,
                         PyObject* value);
    int (*MultiDict_Clear)(void* state, PyObject* self);
    int (*MultiDict_DelItem)(void* state, PyObject* self, PyObject* key);
    int (*MultiDict_Pop)(void* state, PyObject* self, PyObject* key,
                         PyObject** result);
    int (*MultiDict_SetDefault)(void* state, PyObject* self, PyObject* key,
                                PyObject* default_value, PyObject** result);
    int (*MultiDict_SetItem)(void* state, PyObject* self, PyObject* key,
                             PyObject* value);
} MultiDict_CAPI;

#ifdef __cplusplus
}
#endif

#endif /* _MULTIDICT_CAPI_STRUCT_H */
