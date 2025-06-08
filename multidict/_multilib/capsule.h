#ifndef __MULTIDICT_CAPSULE_H__
#define __MULTIDICT_CAPSULE_H__

#include "Python.h"
#include "dict.h"

#ifdef __cplusplus
extern "C" {
#endif


// This C-API Provides Comptability to CPython & Cython.

typedef struct  _multidict_capi_s {
    PyTypeObject *_IStrType;

    PyTypeObject *_MultiDictType;
    PyTypeObject *_CIMultiDictType;
    PyTypeObject *_MultiDictProxyType;
    PyTypeObject *_CIMultiDictProxyType;

    // multidict_copy 
    PyObject* (*_MultiDict_Copy)(MultiDictObject* self);

    // multidict_items
    PyObject* (*_MultiDict_Items)(MultiDictObject* self);

    // multidict_tp_iter 
    PyObject* (*_MultiDict_Iter)(MultiDictObject* self);
    
    // multidict_keys
    PyObject* (*_MultiDict_Keys)(MultiDictObject* self);

    // multidict_values
    PyObject* (*_MultiDict_Values)(MultiDictObject* self);

    // multidict_reduce
    PyObject* (*_MultiDict_Reduce)(MultiDictObject* self);

    // multidict_repr
    PyObject* (*_MultiDict_Repr)(MultiDictObject* self);

    // multidict_update
    PyObject* (*_MultiDict_Update)(MultiDictObject* self, PyObject* args, PyObject* kwargs);

    // multidict_proxy_copy(MultiDictProxyObject *self)
    PyObject* (*_MultiDictProxy_Copy)(MultiDictProxyObject* self);
    
} PyMultiDict_CAPI;


static PyMultiDict_CAPI* MultiDict_CAPI;


// https://docs.python.org/3.9/extending/extending.html#using-capsules

// Most inline functions have a different calling style and need a different approch.

/************************ istr *************************/

#define PyIStrType MultiDict_CAPI->_IStrType

#define PyIStr_New(args) PyIStrType->tp_new(PyIStrType, args, NULL)

// NOTE: IStr_CheckExact(state, obj) , IStr_Check(state, obj) are already defined for us...



#define PyMultiDictType MultiDict_CAPI->_MultiDictType



/********************* MultiDict *********************/


#define MultiDict_Len(self) \
    pair_list_len(&self->pairs)

MultiDictObject* MultiDict_New(PyObject* args, PyObject* kwargs){
    MultiDictObject* self = (MultiDictObject*) PyMultiDictType->tp_alloc(PyMultiDictType, 0);
    if (PyMultiDictType->tp_init((PyObject*)self, args, kwargs) < 0){
        Py_XDECREF(self);
        return NULL;
    }
    return self;
}

#define MultiDict_GetAll(self, key, list) \
    pair_list_get_all(&self->pairs, key, list)

#define MultiDict_GetOne(self, key, value) \
    pair_list_get_one(&self->pairs, key, value)


PyObject* MultiDict_Get(MultiDictObject* self, PyObject *key){
    PyObject* val;
    if (MultiDict_GetOne(self, key, &val) < 0){
        return NULL;
    }
    Py_INCREF(val);
    return val;
}

// Adopted from CPython's approch

PyObject* MultiDict_GetWithError(MultiDictObject* self,  PyObject *key){
    PyObject* val = MultiDict_Get(self, key);
    if (val == NULL){
        PyErr_SetObject(PyExc_KeyError, key);
        return NULL;
    }
    return val;
}


#define MultiDict_Del(self, key) \
    pair_list_del(&self->pairs, key);


#define MultiDict_Contains(self, key) \
    pair_list_contains(&self->pairs, key, NULL)

#define MultiDict_Add(self, key, value) \
    pair_list_add(&self->pairs, key, value)


#define MultiDict_Clear(self) \
    pair_list_clear(&self->pairs);


#define MultiDict_Replace(self, key, value) \
    pair_list_replace(&self->pairs, key, value)

#define MultiDict_PopOne(self, key, ret_val) \
    pair_list_pop_one(&self->pairs, key, ret_val)

PyObject* MultiDict_Pop(MultiDictObject *self, PyObject* key, PyObject* _default){
    PyObject* ret_val;
    if (MultiDict_PopOne(self, key, &ret_val) < 0) {
        return NULL;
    }
    if (ret_val != NULL){
        return ret_val;
    }
    if (_default != NULL){
        Py_INCREF(_default);
        return _default;
    }
    PyErr_SetObject(PyExc_KeyError, key);
    return NULL;
}

#define MultiDict_PopAll(self, key, ret_val) \
    pair_list_pop_all(&self->pairs, key, ret_val)

#define MultiDict_PopItem(self) \
    pair_list_pop_item(&self->pairs)

#define MultiDict_SetItem(self, key, val) \
    pair_list_set_default(&self->pairs, key, val)


// CAPI-Methods

#define MultiDict_Copy MultiDict_CAPI->_MultiDict_Copy
#define MultiDict_Items MultiDict_CAPI->_MultiDict_Items
#define MultiDict_Iter MultiDict_CAPI->_MultiDict_Iter
#define MultiDict_Keys MultiDict_CAPI->_MultiDict_Keys
#define MultiDict_Values MultiDict_CAPI->_MultiDict_Values
#define MultiDict_Reduce MultiDict_CAPI->_MultiDict_Reduce
#define MultiDict_Repr MultiDict_CAPI->_MultiDict_Repr
#define MultiDict_Update MultiDict_CAPI->_MultiDict_Update


/******************** CIMultiDict ********************/

#define PyCIMultiDictType MultiDict_CAPI->_CIMultiDictType

MultiDictObject* CIMultiDict_New(PyObject* args, PyObject* kwargs){
    MultiDictObject* self = (MultiDictObject*) PyCIMultiDictType->tp_alloc(PyMultiDictType, 0);
    if (PyCIMultiDictType->tp_init((PyObject*)self, args, kwargs) < 0){
        Py_XDECREF(self);
        return NULL;
    }
    return self;
}


#define CIMultiDict_GetAll MultiDict_GetAll
#define CIMultiDict_GetOne MultiDict_GetOne
#define CIMultiDict_Get MultiDict_Get
#define CIMultiDict_GetWithError MultiDict_GetWithError
#define CIMultiDict_Del MultiDict_Del
#define CIMultiDict_Contains MultiDict_Contains
#define CIMultiDict_Add MultiDict_Add
#define CIMultiDict_Clear MultiDict_Clear
#define CIMultiDict_Replace MultiDict_Replace
#define CIMultiDict_Len MultiDict_Len

#define CIMultiDict_Copy MultiDict_Copy
#define CIMultiDict_Items MultiDict_Items
#define CIMultiDict_Iter MultiDict_Iter
#define CIMultiDict_Keys MultiDict_Keys
#define CIMultiDict_Values MultiDict_Values
#define CIMultiDict_Reduce MultiDict_Reduce
#define CIMultiDict_Repr MultiDict_Repr
#define CIMultiDict_Update MultiDict_Update
#define CIMultiDict_SetItem MultiDict_SetItem




/******************** MultiDictProxy ********************/

#define PyMultiDictProxyType MultiDict_CAPI->_MultiDictProxyType

MultiDictProxyObject* MultiDictProxy_New(PyObject* args, PyObject* kwargs){
    MultiDictProxyObject* self = (MultiDictProxyObject*) PyMultiDictProxyType->tp_alloc(PyMultiDictType, 0);
    if (PyMultiDictProxyType->tp_init((PyObject*)self, args, kwargs) < 0){
        Py_XDECREF(self);
        return NULL;
    }
    return self;
}

#define MultiDictProxy_Copy(self) \
    MultiDict_CAPI->_MultiDictProxy_Copy(self)

#define MultiDictProxy_GetAll(self, key, list) \
    MultiDict_GetAll(self->md, key, list);

#define MultiDictProxy_GetOne(self, key, value) \
    MultiDict_GetOne(self->md, key, value)

#define MultiDictProxy_Get(self, key) \
    MultiDict_Get(self->md, key)

#define MultiDictProxy_Keys(self) \
    MultiDict_CAPI->MultiDict_Keys(self->md)

#define MultiDictProxy_Items(self) \
    MultiDict_CAPI->MultiDict_Items(self->md)

#define MultiDictProxy_Values(self) \
    MultiDict_CAPI->MultiDict_Values(self->md);

#define MultiDictProxy_Len(self) \
    MultiDict_Len((self->md))

#define MultiDictProxy_Iter(self) MultiDict_Iter(self->md)
#define MultiDictProxy_Reduce(self) MultiDict_Reduce(self->md)
#define MultiDictProxy_Repr(self) MultiDict_Repr(self->md)
#define MultiDictProxy_Update(self, args, kwargs) MultiDict_Update(self->md, args, kwargs)
#define MultiDictProxy_SetItem(self, key, value) MultiDict_SetItem(self->md, key, value)



/******************** CIMultiDictProxy ********************/

#define PyCIMultiDictProxyType MultiDict_CAPI->_CIMultiDictProxyType

MultiDictProxyObject* CIMultiDictProxy_New(PyObject* args, PyObject* kwargs){
    MultiDictProxyObject* self = (MultiDictProxyObject*) PyCIMultiDictProxyType->tp_alloc(PyMultiDictType, 0);
    if (PyCIMultiDictProxyType->tp_init((PyObject*)self, args, kwargs) < 0){
        Py_XDECREF(self);
        return NULL;
    }
    return self;
}

// Isn't it just hillarious how this can all be done with just macros?

// Unfortunately this had to be done so that cython would know how 
// to typecast everything without confusing it...

#define CIMultiDictProxy_Copy MultiDictProxy_Copy
#define CIMultiDictProxy_GetAll MultiDictProxy_GetAll
#define CIMultiDictProxy_GetOne MultiDictProxy_GetOne
#define CIMultiDictProxy_Get MultiDictProxy_Get
#define CIMultiDictProxy_Keys MultiDictProxy_Keys
#define CIMultiDictProxy_Items MultiDictProxy_Items
#define CIMultiDictProxy_Values MultiDictProxy_Values
#define CIMultiDictProxy_Len MultiDictProxy_Len
#define CIMultiDictProxy_Iter MultiDictProxy_Iter
#define CIMultiDictProxy_Repr MultiDictProxy_Repr
#define CIMultiDictProxy_Update MultiDictProxy_Update


// Cython / CPython helper...
static int
import_multidict(void)
{
    MultiDict_CAPI = (PyMultiDict_CAPI*)PyCapsule_Import("multidict._multidict._C_API", 0);
    return (MultiDict_CAPI != NULL) ? 0 : -1;
}


#ifdef __cplusplus
};
#endif

#endif // __MULTIDICT_CAPSULE_H__
