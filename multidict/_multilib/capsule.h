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

/// @brief Gets the multidict type object
/// @param state_ the module state to obtain the type from
/// @return MultiDictType as a pointer object otherwise NULLL
static PyTypeObject *
MultiDict_GetType(void *state_)
{
    mod_state *state = (mod_state *)state_;
    return (PyTypeObject *)Py_NewRef(state->MultiDictType);
}

/// @brief Creates a new multidict with a preallocated number of entires to work with
/// @param state_ the state to obtain the MultiDictType from
/// @param prealloc_size the number of entries to preallocate room for
/// @return A Multidict object otherwise NULL
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

/// @brief Adds a key and value to the mulitidict
/// @param state_ the module state to use
/// @param self the multidict object
/// @param key the key
/// @param value the value to set
/// @return 0 if success otherwise -1 , will raise TypeError if MultiDict's Type is incorrect
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

/// @brief Clears a multidict object and removes all it's entries
/// @param state_ the module state to use
/// @param self the multidict object
/// @return 0 if success otherwise -1 , will raise TypeError if MultiDict's Type is incorrect
static int MultiDict_Clear(void* state_, PyObject* self){
    // TODO: Macro for repeated steps being done?
    mod_state *state = (mod_state *)state_;
    if (MultiDict_Check(state, self) <= 0) {
        _invalid_type();
        return -1;
    }
    return md_clear((MultiDictObject*)self);
}

/// @brief If key is in the dictionary  its the first value. 
/// If not, insert key with a value of default and return default.
/// @param state_ the module state to use
/// @param self the MultiDict object
/// @param key the key to insert
/// @param _default the default value to have inserted
/// @return default on sucess, NULL on failure
PyObject* Multidict_SetDefault(void* state_, PyObject* self, PyObject* key, PyObject* _default){
    mod_state *state = (mod_state *)state_;
    if (MultiDict_Check(state, self) <= 0) {
        _invalid_type();
        return NULL;
    }
    return md_set_default(self, key, _default);
}

/// @brief Remove all items where key is equal to key from d. 
/// @param state_ the module state to use
/// @param self the MultiDict
/// @param key the key to be removed
/// @return 0 on success, -1 on failure followed by rasing either 
/// `TypeError` or `KeyError` if key is not in the map.
static int MutliDict_Del(void* state_, PyObject* self, PyObject* key){
    mod_state *state = (mod_state *)state_;
    if (MultiDict_Check(state, self) <= 0) {
        _invalid_type();
        return -1;
    }
    return md_del(self, key);
}


/// @brief Frees the Multidict CAPI Capsule Object from 
/// the Heap Normally you won't be needing to call this
/// @param capi The Capsule Object To Free
static void
multidict_capsule_free(MultiDict_CAPI *capi)
{
    PyMem_Free(capi);
}


static void
multidict_capsule_destructor(PyObject *o)
{
    MultiDict_CAPI *capi = PyCapsule_GetPointer(o, MultiDict_CAPSULE_NAME);
    capsule_free(capi);
}

static PyObject *
multidict_new_capsule(mod_state *state)
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
        PyCapsule_New(capi, MultiDict_CAPSULE_NAME, multidict_capsule_destructor);
    if (ret == NULL) {
        multidict_capsule_free(capi);
    }
    return ret;
}

#ifdef __cplusplus
}
#endif

#endif
