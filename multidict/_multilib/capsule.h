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

/// @brief Return a version of given mdict object
/// @param state_ the module state to use
/// @param self the mdict object
/// @return the version flag of the object, otherwise 0 on failure
static uint64_t MultiDict_Version(void* state_, PyObject* self){
    mod_state *state = (mod_state *)state_;
    if (MultiDict_Check(state, self) <= 0) {
        _invalid_type();
        // Can't use -1 because version is unsigned, resort to zero...
        return 0;
    }
    return md_version((MultiDictObject*)self);
}


// MultiDict_CreatePosMarker & MultiDict_Next Took inspiration from PyDict_Next 
// it's implementations are held subject to being changed.

/// @brief Creates a new positional marker for a multidict to iterate 
/// with when being utlizied with `MultiDict_Next`
/// @param state_ the module state
/// @param self the multidict to create a positional marker for
/// @param pos the positional marker to be created
/// @return 0 on sucess, -1 on failure along with `TypeError` exception being thrown
static int MultiDict_CreatePosMarker(void* state_, PyObject* self, md_pos_t* pos){
    mod_state *state = (mod_state *)state_;
    if (MultiDict_Check(state, self) <= 0) {
        _invalid_type();
        return -1;
    }
    md_init_pos((MultiDictObject*)self, pos);
    return 0;
}



static int MultiDict_Next(void* state_, PyObject* self, 
    md_pos_t* pos, PyObject** identity, PyObject**key, PyObject **value){
    mod_state *state = (mod_state *)state_;
    if (MultiDict_Check(state, self) <= 0) {
        _invalid_type();
        return -1;
    }
    else if (pos == NULL){
        PyErr_SetString(PyExc_ValueError, 
            "positional marker cannot be left NULL"
        );
        return -1;
    }
    return md_next((MultiDictObject*)self, &pos, identity, key, value);
}

/// @brief Determines if a certain key exists a multidict object
/// @param state_ the module state
/// @param self the multidict object
/// @param key the key to look for
/// @return 1 if true, 0 if false, -1 if failure had occured
static int MultiDict_Contains(void* state_, PyObject* self, PyObject* key){
    mod_state *state = (mod_state *)state_;
    if (MultiDict_Check(state, self) <= 0) {
        _invalid_type();
        return -1;
    }
    return md_contains((MultiDictObject*)self, key, NULL);
};



static PyObject* MultiDict_GetOne(void* state_, PyObject* self, PyObject* key){
    mod_state *state = (mod_state *)state_;
    if (MultiDict_Check(state, self) <= 0) {
        _invalid_type();
        return NULL;
    }
    PyObject* ret;
    if (md_get_one((MultiDictObject*)self, key, &ret) < 0){
        PyErr_SetObject(PyExc_KeyError, key);
    };
    return ret;
}

static PyObject* MultiDict_GetAll(void* state_, PyObject* self, PyObject* key){
    mod_state *state = (mod_state *)state_;
    if (MultiDict_Check(state, self) <= 0) {
        _invalid_type();
        return NULL;
    }
    PyObject* ret;
    if (md_get_all((MultiDictObject*)self, key, &ret) < 0){
        PyErr_SetObject(PyExc_KeyError, key);
    };
    return ret;
}

static PyObject* MultiDict_PopOne(void* state_, PyObject* self, PyObject* key){
    mod_state *state = (mod_state *)state_;
    if (MultiDict_Check(state, self) <= 0) {
        _invalid_type();
        return NULL;
    }
    PyObject* ret;
    if (md_pop_one((MultiDictObject*)self, key, &ret) < 0){
        PyErr_SetObject(PyExc_KeyError, key);
    };
    return ret;
}

static PyObject* MultiDict_PopAll(void* state_, PyObject* self, PyObject* key){
    mod_state *state = (mod_state *)state_;
    if (MultiDict_Check(state, self) <= 0) {
        _invalid_type();
        return NULL;
    }
    PyObject* ret;
    if (md_pop_all((MultiDictObject*)self, key, &ret) < 0){
        PyErr_SetObject(PyExc_KeyError, key);
    };
    return ret;
}

static PyObject* MultiDict_PopItem(void* state_, PyObject* self, PyObject* key){
    mod_state *state = (mod_state *)state_;
    if (MultiDict_Check(state, self) <= 0) {
        _invalid_type();
        return NULL;
    }
    return md_pop_item((MultiDictObject*)self);
}

static int MultiDict_Replace(void* state_, PyObject* self, PyObject* key, PyObject* value){
    mod_state *state = (mod_state *)state_;
    if (MultiDict_Check(state, self) <= 0) {
        _invalid_type();
        return NULL;
    }
    return md_replace((MultiDictObject*)self, key, value);
}


static int MultiDict_UpdateFromMultiDict(void* state_, PyObject* self, PyObject* other, bool update){
    mod_state* state = (mod_state*)state_;
    // TODO: diagnose which type was wrong...
    if (MultiDict_Check(state, self) <= 0) {
        _invalid_type();
        return -1;
    }
    if (MultiDict_Check(state, other) <= 0) {
        _invalid_type();
        return -1;
    }
    return md_update_from_ht((MultiDictObject*)self, (MultiDictObject*)other, update);
}

static int MultiDict_UpdateFromDict(void* state_, PyObject* self, PyObject* kwds, bool update){
    mod_state* state = (mod_state*)state_;
    if (MultiDict_Check(state, self) <= 0) {
        _invalid_type();
        return -1;
    }
    return md_update_from_dict((MultiDictObject*)self, kwds, update);
}

static int MultiDict_UpdateFromSequence(void* state_, PyObject* self, PyObject* seq, bool update){
    mod_state* state = (mod_state*)state_;
    if (MultiDict_Check(state, self) <= 0) {
        _invalid_type();
        return -1;
    }
    return md_update_from_seq((MultiDictObject*)self, seq, update);
}

static int MultiDict_Equals(void* state_, PyObject* self, PyObject* other){
    mod_state* state = (mod_state*)state_;
    if (MultiDict_Check(state, self) <= 0) {
        _invalid_type();
        return -1;
    }
    if (PyMapping_Check(other)){
        return md_eq_to_mapping((MultiDictObject*)self, other);
    }
    else if (MultiDict_Check(state, other)){
        return md_eq((MultiDictObject*)self, (MultiDictObject*)other);
    }
    return 0;
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
