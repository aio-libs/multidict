#ifndef _MULTIDICT_API_H
#define _MULTIDICT_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <stdbool.h>

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

    void* state;

    PyTypeObject* (*MultiDict_GetType)(void* state);

    PyObject* (*MultiDict_New)(void* state, int prealloc_size);
    int (*MultiDict_Add)(void* state, PyObject* self, PyObject* key,
                         PyObject* value);
    int (*MultiDict_Clear)(void* state, PyObject* self);

    PyObject* (*MultiDict_SetDefault)(void* state, PyObject* self,
                                      PyObject* key, PyObject* _default);

    int (*MultiDict_Del)(void* state, PyObject* self, PyObject* key);
    uint64_t (*MultiDict_Version)(void* state, PyObject* self);

    // proposed but IDK yet...
    // static int MultiDict_CreatePosMarker(void* state, PyObject* self,
    // md_pos_t* pos)

    // static int MultiDict_Next(void* state, PyObject* self,
    //     md_pos_t* pos, PyObject** identity, PyObject**key, PyObject **value)

    int (*MultiDict_Contains)(void* state, PyObject* self, PyObject* key);
    PyObject* (*MultiDict_Get)(void* state, PyObject* self, PyObject* key);
    PyObject* (*MultiDict_GetOne)(void* state, PyObject* self, PyObject* key);
    PyObject* (*MultiDict_GetAll)(void* state, PyObject* self, PyObject* key);
    PyObject* (*MultiDict_Pop)(void* state, PyObject* self, PyObject* key);
    PyObject* (*MultiDict_PopOne)(void* state, PyObject* self, PyObject* key);
    PyObject* (*MultiDict_PopAll)(void* state, PyObject* self, PyObject* key);
    PyObject* (*MultiDict_PopItem)(void* state, PyObject* self);
    int (*MultiDict_Replace)(void* state, PyObject* self, PyObject* key,
                             PyObject* value);
    int (*MultiDict_UpdateFromMultiDict)(void* state, PyObject* self,
                                         PyObject* other, bool update);
    int (*MultiDict_UpdateFromDict)(void* state, PyObject* self,
                                    PyObject* kwds, bool update);
    int (*MultiDict_UpdateFromSequence)(void* state, PyObject* self,
                                        PyObject* kwds, bool update);
    int (*MultiDict_Equals)(void* state, PyObject* self, PyObject* other);

} MultiDict_CAPI;

#ifndef MULTIDICT_IMPL

/// @brief Imports Multidict CAPI
/// @return A Capsule Containing the Multidict CAPI Otherwise NULL
static inline MultiDict_CAPI*
MultiDict_Import()
{
    return (MultiDict_CAPI*)PyCapsule_Import(MultiDict_CAPSULE_NAME, 0);
}

/// @brief Obtains the Multidict TypeObject
/// @param api Python Capsule Pointer to the API
/// @return A CPython `PyTypeObject` is returned as a pointer,
/// `NULL` on failure
static inline PyTypeObject*
MultiDict_GetType(MultiDict_CAPI* api)
{
    return api->MultiDict_GetType(api->state);
}
/// @brief Checks if Multidict Object Type Matches Exactly
/// @param api Python Capsule Pointer to the API
/// @param op The Object to check
/// @return 1 if `true`, 0 if `false`
static inline int
MultiDict_CheckExact(MultiDict_CAPI* api, PyObject* op)
{
    PyTypeObject* type = api->MultiDict_GetType(api->state);
    int ret = Py_IS_TYPE(op, type);
    Py_DECREF(type);
    return ret;
}

/// @brief Checks if Multidict Object Type Matches or is a subclass of itself
/// @param api Python Capsule Pointer to the API
/// @param op The Object to check
/// @return 1 if `true`, 0 if `false`
static inline int
MultiDict_Check(MultiDict_CAPI* api, PyObject* op)
{
    PyTypeObject* type = api->MultiDict_GetType(api->state);
    int ret = Py_IS_TYPE(op, type) || PyObject_TypeCheck(op, type);
    Py_DECREF(type);
    return ret;
}

/// @brief Creates a New Multidict Type Object with a number entries wanted
/// preallocated
/// @param api Python Capsule Pointer to the API
/// @param prealloc_size The Number of entires to preallocate for
/// @return `MultiDict` object if successful, otherwise `NULL`
static inline PyObject*
MultiDict_New(MultiDict_CAPI* api, int prealloc_size)
{
    return api->MultiDict_New(api->state, prealloc_size);
}

/// @brief Adds a new entry to the `multidict` object
/// @param api Python Capsule Pointer to the API
/// @param self the Multidict object
/// @param key The key of the entry to add
/// @param value The value of the entry to add
/// @return 0 on success, -1 on failure
static inline int
MultiDict_Add(MultiDict_CAPI* api, PyObject* self, PyObject* key,
              PyObject* value)
{
    return api->MultiDict_Add(api->state, self, key, value);
}

/// @brief Clears a multidict object and removes all it's entries
/// @param api Python Capsule Pointer to the API
/// @param self the multidict object
/// @return 0 if success otherwise -1 , will raise TypeError if MultiDict's
/// Type is incorrect
static int
MultiDict_Clear(MultiDict_CAPI* api, PyObject* self)
{
    return api->MultiDict_Clear(api->state, self);
}

/// @brief If key is in the dictionary  its the first value.
/// If not, insert key with a value of default and return default.
/// @param api Python Capsule Pointer
/// @param self the MultiDict object
/// @param key the key to insert
/// @param _default the default value to have inserted
/// @return default on success, NULL on failure
PyObject*
Multidict_SetDefault(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                     PyObject* _default)
{
    return api->MultiDict_SetDefault(api->state, self, key, _default);
}

/// @brief Remove all items where key is equal to key from d.
/// @param api Python Capsule Pointer
/// @param self the MultiDict
/// @param key the key to be removed
/// @return 0 on success, -1 on failure followed by rasing either
/// `TypeError` or `KeyError` if key is not in the map.
static int
MutliDict_Del(MultiDict_CAPI* api, PyObject* self, PyObject* key)
{
    return api->MultiDict_Del(api->state, self, key);
}

/// @brief Return a version of given mdict object
/// @param api Python Capsule Pointer
/// @param self the mdict object
/// @return the version flag of the object, otherwise 0 on failure
static uint64_t
MultiDict_Version(MultiDict_CAPI* api, PyObject* self)
{
    return api->MultiDict_Version(api->state, self);
}

// Under debate as concept

// /// @brief Creates a new positional marker for a multidict to iterate
// /// with when being utlizied with `MultiDict_Next`
// /// @param api Python Capsule Pointer
// /// @param self the multidict to create a positional marker for
// /// @param pos the positional marker to be created
// /// @return 0 on success, -1 on failure along with `TypeError` exception
// being thrown static int MultiDict_CreatePosMarker(MultiDict_CAPI* api,
// PyObject* self, md_pos_t* pos){
//     return api->MultiDict_CreatePosMarker(api->state, self, pos);
// }
// static int MultiDict_Next(MultiDict_CAPI* api, PyObject* self,
//     md_pos_t* pos, PyObject** identity, PyObject**key, PyObject **value){
//     return api->MultiDict_Next(api->state, self, pos, identity, key, value);
// };

/// @brief Determines if a certain key exists a multidict object
/// @param api Python Capsule Pointer
/// @param self the multidict object
/// @param key the key to look for
/// @return 1 if true, 0 if false, -1 if failure had occured
static int
MultiDict_Contains(MultiDict_CAPI* api, PyObject* self, PyObject* key)
{
    return api->MultiDict_Contains(api->state, self, key);
}

/// @brief  Return the **first** value for *key* if *key* is in the
/// dictionary, else *default*.
/// @param api Python Capsule Pointer
/// @param self the multidict object
/// @param key the key to get one item from
/// @return returns a default value on success, -1 with `KeyError` or
/// `TypeError` on failure
static PyObject*
MultiDict_GetOne(MultiDict_CAPI* api, PyObject* self, PyObject* key)
{
    return api->MultiDict_GetOne(api->state, self, key);
}

/// @brief Return the **first** value for *key* if *key* is in the
/// dictionary, else None.
/// @param api Python Capsule Pointer
/// @param self the multidict object
/// @param key the key to get one item from
/// @return returns a default value on success,  NULL is returned and raises
/// `TypeError` on failure
static PyObject*
MultiDict_Get(MultiDict_CAPI* api, PyObject* self, PyObject* key)
{
    return api->MultiDict_Get(api->state, self, key);
}

/// @brief Return a list of all values for *key* if *key* is in the
/// dictionary, else *default*.
/// @param api Python Capsule Pointer
/// @param self the multidict obeject
/// @param key the key to obtain all the items from
/// @return a list of all the values, otherwise NULL on error
/// raises either `KeyError` or `TypeError`
static PyObject*
MultiDict_GetAll(MultiDict_CAPI* api, PyObject* self, PyObject* key)
{
    return api->MultiDict_GetAll(api, self, key);
}

/// @brief  Remove and return a value from the dictionary.
/// @param api Python Capsule Pointer
/// @param self the multidict object
/// @param key the key to remove
/// @return corresponding value on success or None, otherwise raises TypeError
/// and returns NULL
static PyObject*
MultiDict_Pop(MultiDict_CAPI* api, PyObject* self, PyObject* key)
{
    return api->MultiDict_Pop(api->state, self, key);
}

/// @brief  If `key` is in the dictionary, remove it and return its the
/// `first` value, else return `default`.
/// @param api Python Capsule Pointer
/// @param self the multidict object
/// @param key the key to pop
/// @return object on success, otherwise NULL on error along
/// with `KeyError` or `TypeError` being raised
static PyObject*
MultiDict_PopOne(MultiDict_CAPI* api, PyObject* self, PyObject* key)
{
    return api->MultiDict_PopOne(api->state, self, key);
}

/// @brief Pops all related objects corresponding to `key`
/// @param api Python Capsule Pointer
/// @param self the multidict object
/// @param key the key to pop all of
/// @return list object on success, otherwise NULL, on error and raises either
/// `KeyError` or `TyperError`
static PyObject*
MultiDict_PopAll(MultiDict_CAPI* api, PyObject* self, PyObject* key)
{
    return api->MultiDict_PopAll(api->state, self, key);
}

/// @brief Remove and return an arbitrary `(key, value)` pair from the
/// dictionary.
/// @param api Python Capsule Pointer
/// @param self the multidict object
/// @return an arbitray tuple on success, otherwise NULL on error along
/// with `TypeError` or `KeyError` raised
static PyObject*
MultiDict_PopItem(MultiDict_CAPI* api, PyObject* self)
{
    return api->MultiDict_PopItem(api->state, self);
}

/// @brief Replaces a set object with another object
/// @param api Python Capsule Pointer
/// @param self the multidict object
/// @param key the key to lookup for replacement
/// @param value the value to replace with
/// @return 0 on sucess, -1 on Failure and raises TypeError
static int
MultiDict_Replace(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                  PyObject* value)
{
    return api->MultiDict_Replace(api->state, self, key, value);
};

/// @brief Updates Multidict object using another MultiDict Object
/// @param api Python Capsule Pointer
/// @param self the multidict object
/// @param other a multidict object to update corresponding object with
/// @param update if true append references and stack them, otherwise steal all
/// references.
/// @return 0 on sucess, -1 on failure
static int
MultiDict_UpdateFromMultiDict(MultiDict_CAPI* api, PyObject* self,
                              PyObject* other, bool update)
{
    return api->MultiDict_UpdateFromMultiDict(api->state, self, other, update);
};

/// @brief Updates Multidict object using another Dictionary Object
/// @param api Python Capsule Pointer
/// @param self the multidict object
/// @param kwds the keywords or Dictionary object to merge
/// @param update if true append references and stack them, otherwise steal all
/// references.
/// @return 0 on sucess, -1 on failure
static int
MultiDict_UpdateFromDict(MultiDict_CAPI* api, PyObject* self, PyObject* kwds,
                         bool update)
{
    return api->MultiDict_UpdateFromDict(api->state, self, kwds, update);
};

/// @brief Updates Multidict object using a sequence object
/// @param api Python Capsule Pointer
/// @param self the multidict object
/// @param seq the sequence to merge with.
/// @param update if true append references and stack them, otherwise steal all
/// references.
/// @return 0 on sucess, -1 on failure
static int
MultiDict_UpdateFromSequence(MultiDict_CAPI* api, PyObject* self,
                             PyObject* seq, bool update)
{
    return api->MultiDict_UpdateFromSequence(api->state, self, seq, update);
};

/// @brief Checks to see if a multidict matches another dictionary or multidict
/// object
/// @param api Python Capsule Pointer
/// @param self the multidict object
/// @param other the corresponding object to check against
/// @return 1 if true, 0 if false, -1 if failue occured follwed by raising a
/// TypeError
static int
MultiDict_Equals(MultiDict_CAPI* api, PyObject* self, PyObject* other)
{
    return api->MultiDict_Equals(api->state, self, other);
};

#endif

#ifdef __cplusplus
}
#endif

#endif
