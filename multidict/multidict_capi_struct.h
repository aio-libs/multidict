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

/* Bump whenever a field is appended to MultiDict_CAPI below. Existing
   fields never move or change meaning, so a client built against an
   older version of this header keeps working against a newer
   multidict runtime; MultiDict_GetCAPI() uses this to refuse the
   other direction (an older runtime that predates a field the client
   was built to expect). */
#define MultiDict_CAPI_VERSION 1

typedef int (*MultiDict_ItemVisitor)(void* user_data, PyObject* identity,
                                     Py_hash_t hash, PyObject* key,
                                     PyObject* value);

/* Watchers. Modelled on CPython's PyDict_AddWatcher() family, with two
   deliberate differences: the callback gets two context pointers (one fixed
   per watcher, one per watched multidict), and events are delivered once the
   operation that produced them has finished rather than mid-mutation. See
   docs/capi.rst. */

#define MULTIDICT_MAX_WATCHERS 8

typedef enum {
    MultiDict_EVENT_ADDED = 0,
    MultiDict_EVENT_REPLACED,
    MultiDict_EVENT_DELETED,
    MultiDict_EVENT_CLEARED,
    MultiDict_EVENT_CLONED,
    MultiDict_EVENT_DEALLOCATED,
    MultiDict_EVENT_BATCH_BEGIN,
    MultiDict_EVENT_BATCH_END,
    MultiDict_EVENT_LOST,
} MultiDict_WatchEvent;

/* Only ever grows at the end, like MultiDict_CAPI itself. multidict
   allocates it and the client only reads it, so a client built against an
   older header simply never looks at the newer fields and needs no size
   field to stay safe. Every PyObject* is borrowed for the duration of the
   call. */
typedef struct {
    MultiDict_WatchEvent event;
    PyObject* self;
    PyObject* identity;
    /* hash of `identity`, the one multidict looked the entry up by; -1,
       which no Python hash ever is, on an event that carries no key. */
    Py_hash_t hash;
    PyObject* key;
    PyObject* value;
    PyObject* old_value;
} MultiDict_WatchInfo;

typedef int (*MultiDict_WatchCallback)(void* watcher_data, void* user_data,
                                       const MultiDict_WatchInfo* info);

typedef struct {
    int api_version;
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

    Py_ssize_t (*MultiDict_Size)(void* state, PyObject* self);
    int (*MultiDict_Contains)(void* state, PyObject* self, PyObject* key);
    int (*MultiDict_GetItem)(void* state, PyObject* self, PyObject* key,
                             PyObject** result);

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

    Py_ssize_t (*MultiDict_ForEach)(void* state, PyObject* self, PyObject* key,
                                    MultiDict_ItemVisitor visitor,
                                    void* user_data);

    int (*MultiDict_AddWatcher)(void* state, MultiDict_WatchCallback callback,
                                void* watcher_data);
    int (*MultiDict_ClearWatcher)(void* state, int watcher_id);
    int (*MultiDict_Watch)(void* state, int watcher_id, PyObject* self,
                           void* user_data);
    int (*MultiDict_Unwatch)(void* state, int watcher_id, PyObject* self);
} MultiDict_CAPI;

#ifdef __cplusplus
}
#endif

#endif /* _MULTIDICT_CAPI_STRUCT_H */
