#ifndef _MULTIDICT_API_H
#define _MULTIDICT_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>

#define MultiDict_MODULE_NAME "multidict._multidict"
#define MultiDict_CAPI_NAME "CAPI"
#define MultiDict_CAPSULE_NAME MultiDict_MODULE_NAME "." MultiDict_CAPI_NAME

typedef struct {
    /* N.B.

       For the sake of backward and future compatibility,
       new fields should be added at the end of the structure,
       unused fields should be never removed.

       Otherwise, it could lead to crashes with memory corruptions
       if the client is compiled with older multidict_api.h header
    */

    void *state;

    PyTypeObject *(*MultiDict_GetType)(void *state);

    PyObject *(*MultiDict_New)(void *state, int prealloc_size);
    int (*MultiDict_Add)(void *state, PyObject *self, PyObject *key,
                         PyObject *value);
    int (*MultiDict_Clear)(void* state, PyObject* self);

    PyObject* (*MultiDict_SetDefault)(void* state_, PyObject* self, 
        PyObject* key, PyObject* _default);
    


} MultiDict_CAPI;

#ifndef MULTIDICT_IMPL

/// @brief Imports Multidict CAPI
/// @return A Capsule Containing the Multidict CAPI Otherwise NULL
static inline MultiDict_CAPI *
MultiDict_Import()
{
    return (MultiDict_CAPI *)PyCapsule_Import(MultiDict_CAPSULE_NAME, 0);
}

/// @brief Obtains the Multidict TypeObject
/// @param api Python Capsule Pointer to the API
/// @return A CPython `PyTypeObject` is returned as a pointer,
/// `NULL` on failure
static inline PyTypeObject *
MultiDict_GetType(MultiDict_CAPI *api)
{
    return api->MultiDict_GetType(api->state);
}
/// @brief Checks if Multidict Object Type Matches Exactly 
/// @param api Python Capsule Pointer to the API
/// @param op The Object to check
/// @return 1 if `true`, 0 if `false` 
static inline int
MultiDict_CheckExact(MultiDict_CAPI *api, PyObject *op)
{
    PyTypeObject *type = api->MultiDict_GetType(api->state);
    int ret = Py_IS_TYPE(op, type);
    Py_DECREF(type);
    return ret;
}

/// @brief Checks if Multidict Object Type Matches or is a subclass of itself
/// @param api Python Capsule Pointer to the API
/// @param op The Object to check
/// @return 1 if `true`, 0 if `false` 
static inline int
MultiDict_Check(MultiDict_CAPI *api, PyObject *op)
{
    PyTypeObject *type = api->MultiDict_GetType(api->state);
    int ret = Py_IS_TYPE(op, type) || PyObject_TypeCheck(op, type);
    Py_DECREF(type);
    return ret;
}

/// @brief Creates a New Multidict Type Object with a number entries wanted preallocated
/// @param api Python Capsule Pointer to the API
/// @param prealloc_size The Number of entires to preallocate for
/// @return `MultiDict` object if sucessful, otherwise `NULL`
static inline PyObject *
MultiDict_New(MultiDict_CAPI *api, int prealloc_size)
{
    return api->MultiDict_New(api->state, prealloc_size);
}

/// @brief Adds a new entry to the `multidict` object
/// @param api Python Capsule Pointer to the API
/// @param self the Multidict object
/// @param key The key of the entry to add
/// @param value The value of the entry to add
/// @return 0 on sucess, -1 on failure
static inline int
MultiDict_Add(MultiDict_CAPI *api, PyObject *self, PyObject *key,
              PyObject *value)
{
    return api->MultiDict_Add(api->state, self, key, value);
}

/// @brief Clears a multidict object and removes all it's entries
/// @param api Python Capsule Pointer to the API
/// @param self the multidict object
/// @return 0 if success otherwise -1 , will raise TypeError if MultiDict's Type is incorrect
static int MultiDict_Clear(MultiDict_CAPI* api, PyObject* self){
    return api->MultiDict_Clear(api->state, self);
}


/// @brief If key is in the dictionary  its the first value. 
/// If not, insert key with a value of default and return default.
/// @param state_ the module state to use
/// @param self the MultiDict object
/// @param key the key to insert
/// @param _default the default value to have inserted
/// @return default on sucess, NULL on failure
PyObject* Multidict_SetDefault(MultiDict_CAPI* api, PyObject* self, PyObject* key, PyObject* _default){
    return api->MultiDict
}


/// @brief If key is in the dictionary  its the first value. 
/// If not, insert key with a value of default and return default.
/// @param api Python Capsule Pointer to the API
/// @param self the MultiDict object
/// @param key the key to insert
/// @param _default the default value to have inserted
/// @return default on sucess, NULL on failure
PyObject* Multidict_SetDefault(MultiDict_CAPI *api, PyObject* self, PyObject* key, PyObject* _default){
    mod_state *state = (mod_state *)state_;
    if (MultiDict_Check(state, self) <= 0) {
        _invalid_type();
        return NULL;
    }
    return md_set_default(self, key, _default);
}



#endif

#ifdef __cplusplus
}
#endif

#endif
