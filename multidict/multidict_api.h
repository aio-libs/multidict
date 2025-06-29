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
    int (*MultiDict_GetOne)(void* state_, PyObject* self, PyObject* key,
                            PyObject** result);
    int (*MultiDict_GetAll)(void* state_, PyObject* self, PyObject* key,
                            PyObject** result);
    int (*MultiDict_PopOne)(void* state_, PyObject* self, PyObject* key,
                            PyObject** result);
    int (*MultiDict_PopAll)(void* state_, PyObject* self, PyObject* key,
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

    PyObject* (*IStr_FromUnicode)(void* state_, PyObject* str);
    PyObject* (*IStr_FromStringAndSize)(void* state_, PyObject* str,
                                        Py_ssize_t size);
    PyObject* (*IStr_FromString)(void* state_, const char* str);
    PyTypeObject* (*IStr_GetType)(void* state_);

} MultiDict_CAPI;

// TODO (Vizonex): cleanup Docstrings We can put the function documentation
// into the cython c-api simillar to what cython did with cpython in the
// future... I see no reason to use doxygen styled documentation. And if we
// decide to keep it then it should be transformed into the sphinx-styled
// documentation instead.

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
static inline int
MultiDict_Clear(MultiDict_CAPI* api, PyObject* self)
{
    return api->MultiDict_Clear(api->state, self);
}

/// XXX: Documentation is incorrect I will need to edit in a bit - Vizonex
/// @brief If key is in the dictionary  its the first value.
/// If not, insert key with a value of default and return default.
/// @param api Python Capsule Pointer
/// @param self the MultiDict object
/// @param key the key to insert
/// @param _default the default value to have inserted
/// @return default on success, NULL on failure
static inline int
MultiDict_SetDefault(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                     PyObject* default_, PyObject** result)
{
    return api->MultiDict_SetDefault(api->state, self, key, default_, result);
}

/// @brief Remove all items where key is equal to key from d.
/// @param api Python Capsule Pointer
/// @param self the MultiDict
/// @param key the key to be removed
/// @return 0 on success, -1 on failure followed by rasing either
/// `TypeError` or `KeyError` if key is not in the map.
static inline int
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

/// @brief Determines if a certain key exists a multidict object
/// @param api Python Capsule Pointer
/// @param self the multidict object
/// @param key the key to look for
/// @return 1 if true, 0 if false, -1 if failure had occured
static inline int
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
static inline int
MultiDict_GetOne(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                 PyObject** result)
{
    return api->MultiDict_GetOne(api->state, self, key, result);
}

/// @brief Return a list of all values for *key* if *key* is in the
/// dictionary, else *default*.
/// @param api Python Capsule Pointer
/// @param self the multidict obeject
/// @param key the key to obtain all the items from
/// @return a list of all the values, otherwise NULL on error
/// raises either `KeyError` or `TypeError`
static inline int
MultiDict_GetAll(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                 PyObject** result)
{
    return api->MultiDict_GetAll(api->state, self, key, result);
}

/// @brief  If `key` is in the dictionary, remove it and return its the
/// `first` value, else return `default`.
/// @param api Python Capsule Pointer
/// @param self the multidict object
/// @param key the key to pop
/// @return object on success, otherwise NULL on error along
/// with `KeyError` or `TypeError` being raised
static inline int
MultiDict_PopOne(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                 PyObject** result)
{
    return api->MultiDict_PopOne(api->state, self, key, result);
}

/// @brief Pops all related objects corresponding to `key`
/// @param api Python Capsule Pointer
/// @param self the multidict object
/// @param key the key to pop all of
/// @return list object on success, otherwise NULL, on error and raises either
/// `KeyError` or `TyperError`
static inline int
MultiDict_PopAll(MultiDict_CAPI* api, PyObject* self, PyObject* key,
                 PyObject** result)
{
    return api->MultiDict_PopAll(api->state, self, key, result);
}

/// @brief Remove and return an arbitrary `(key, value)` pair from the
/// dictionary.
/// @param api Python Capsule Pointer
/// @param self the multidict object
/// @return an arbitray tuple on success, otherwise NULL on error along
/// with `TypeError` or `KeyError` raised
static inline PyObject*
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
static inline int
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
static inline int
MultiDict_UpdateFromMultiDict(MultiDict_CAPI* api, PyObject* self,
                              PyObject* other, UpdateOp op)
{
    return api->MultiDict_UpdateFromMultiDict(api->state, self, other, op);
};

/// @brief Updates Multidict object using another Dictionary Object
/// @param api Python Capsule Pointer
/// @param self the multidict object
/// @param kwds the keywords or Dictionary object to merge
/// @param update if true append references and stack them, otherwise steal all
/// references.
/// @return 0 on sucess, -1 on failure
static inline int
MultiDict_UpdateFromDict(MultiDict_CAPI* api, PyObject* self, PyObject* other,
                         UpdateOp op)
{
    return api->MultiDict_UpdateFromDict(api->state, self, other, op);
};

/// @brief Updates Multidict object using a sequence object
/// @param api Python Capsule Pointer
/// @param self the multidict object
/// @param seq the sequence to merge with.
/// @param update if true append references and stack them, otherwise steal all
/// references.
/// @return 0 on sucess, -1 on failure
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

#endif

#ifdef __cplusplus
}
#endif

#endif
