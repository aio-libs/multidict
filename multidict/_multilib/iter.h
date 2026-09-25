#ifndef _MULTIDICT_ITER_H
#define _MULTIDICT_ITER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dict.h"
#include "hashtable.h"
#include "state.h"

typedef struct multidict_iter {
    PyObject_HEAD
    MultiDictObject* md;  // MultiDict or CIMultiDict
    md_pos_t current;
    int reverse;
    PyObject* result;  // items iterator: the last tuple handed out
} MultidictIter;

/* See _multidict_view_alloc() on what a pooled shell still holds. */
NOINLINE static MultidictIter*
_multidict_iter_alloc(MultiDictObject* md, PyTypeObject* tp)
{
    MultidictIter* it = pool_pop(&md->state->iter_pool);
    if (it == NULL) {
        return PyObject_GC_New(MultidictIter, tp);
    }
    /* PyObject_Init() rather than setting the two fields by hand: it
       also does the refcount-total bookkeeping a debug interpreter
       expects of a newly live object. */
    PyObject_Init((PyObject*)it, tp);
    return it;
}

static inline void
_init_iter(MultidictIter* it, MultiDictObject* md, int reverse)
{
    Py_INCREF(md);

    it->md = md;
    it->reverse = reverse;
    it->result = NULL;
    Py_BEGIN_CRITICAL_SECTION(md);
    if (reverse) {
        md_init_pos_reverse(md, &it->current);
    } else {
        md_init_pos(md, &it->current);
    }
    Py_END_CRITICAL_SECTION();
}

static inline PyObject*
multidict_items_iter_new(MultiDictObject* md, int reverse)
{
    MultidictIter* it = _multidict_iter_alloc(md, md->state->ItemsIterType);
    if (it == NULL) {
        return NULL;
    }

    _init_iter(it, md, reverse);
    /* None placeholders, so the first reuse has something to release. */
    it->result = PyTuple_Pack(2, Py_None, Py_None);
    if (it->result == NULL) {
        Py_DECREF(it);
        return NULL;
    }

    PyObject_GC_Track(it);
    return (PyObject*)it;
}

static inline PyObject*
multidict_keys_iter_new(MultiDictObject* md, int reverse)
{
    MultidictIter* it = _multidict_iter_alloc(md, md->state->KeysIterType);
    if (it == NULL) {
        return NULL;
    }

    _init_iter(it, md, reverse);

    PyObject_GC_Track(it);
    return (PyObject*)it;
}

static inline PyObject*
multidict_values_iter_new(MultiDictObject* md, int reverse)
{
    MultidictIter* it = _multidict_iter_alloc(md, md->state->ValuesIterType);
    if (it == NULL) {
        return NULL;
    }

    _init_iter(it, md, reverse);

    PyObject_GC_Track(it);
    return (PyObject*)it;
}

static inline PyObject*
multidict_items_iter_tp_iternext(MultidictIter* self)
{
    PyObject* key = NULL;
    PyObject* value = NULL;
    PyObject* ret = NULL;

    int res;
    Py_BEGIN_CRITICAL_SECTION(self->md);
    res = self->reverse
              ? md_prev(self->md, &self->current, NULL, &key, &value)
              : md_next(self->md, &self->current, NULL, &key, &value);
    Py_END_CRITICAL_SECTION();
    if (res < 0) {
        return NULL;
    }
    if (res == 0) {
        Py_CLEAR(key);
        Py_CLEAR(value);
        PyErr_SetNone(PyExc_StopIteration);
        return NULL;
    }

    /* Reuse the iterator's own tuple when the caller has already
       dropped it, as `for k, v in d.items()` does every step; dictiter
       does the same. self->result is set once at creation and never
       replaced: the uniqueness check is only ever true for the thread
       that owns the tuple, so replacing the field from another thread
       sharing the iterator could free it under that owner. */
    ret = self->result;
    if (PyUnstable_Object_IsUniquelyReferenced(ret)) {
        PyObject* old_key = PyTuple_GET_ITEM(ret, 0);
        PyObject* old_value = PyTuple_GET_ITEM(ret, 1);
        PyTuple_SET_ITEM(ret, 0, key);
        PyTuple_SET_ITEM(ret, 1, value);
        Py_INCREF(ret);
        Py_DECREF(old_key);
        Py_DECREF(old_value);
#if PY_VERSION_HEX >= 0x030e0000
        /* 3.14 caches a tuple's hash in the object; the pair changed. */
        ((PyTupleObject*)ret)->ob_hash = -1;
#endif
        /* The collector untracks a tuple of atomic items; it holds
           arbitrary ones again now. */
        if (!PyObject_GC_IsTracked(ret)) {
            PyObject_GC_Track(ret);
        }
        return ret;
    }
    ret = PyTuple_New(2);
    if (ret == NULL) {
        Py_DECREF(key);
        Py_DECREF(value);
        return NULL;
    }
    PyTuple_SET_ITEM(ret, 0, key);
    PyTuple_SET_ITEM(ret, 1, value);
    return ret;
}

static inline PyObject*
multidict_values_iter_tp_iternext(MultidictIter* self)
{
    PyObject* value = NULL;

    int res;
    Py_BEGIN_CRITICAL_SECTION(self->md);
    res = self->reverse
              ? md_prev(self->md, &self->current, NULL, NULL, &value)
              : md_next(self->md, &self->current, NULL, NULL, &value);
    Py_END_CRITICAL_SECTION();
    if (res < 0) {
        return NULL;
    }
    if (res == 0) {
        PyErr_SetNone(PyExc_StopIteration);
        return NULL;
    }

    return value;
}

static inline PyObject*
multidict_keys_iter_tp_iternext(MultidictIter* self)
{
    PyObject* key = NULL;

    int res;
    Py_BEGIN_CRITICAL_SECTION(self->md);
    res = self->reverse ? md_prev(self->md, &self->current, NULL, &key, NULL)
                        : md_next(self->md, &self->current, NULL, &key, NULL);
    Py_END_CRITICAL_SECTION();
    if (res < 0) {
        return NULL;
    }
    if (res == 0) {
        PyErr_SetNone(PyExc_StopIteration);
        return NULL;
    }

    return key;
}

static inline void
multidict_iter_tp_dealloc(MultidictIter* self)
{
    PyTypeObject* tp = Py_TYPE(self);
    PyObject_GC_UnTrack(self);
    /* See multidict_view_tp_dealloc() on why a cleared iterator can't be
       pooled. */
    Py_CLEAR(self->result);
    MultiDictObject* md = self->md;
    bool pooled = md != NULL && pool_push(&md->state->iter_pool, self);
    Py_XDECREF(md);
    if (!pooled) {
        tp->tp_free(self);
    }
    Py_DECREF(tp);
}

static inline int
multidict_iter_tp_traverse(MultidictIter* self, visitproc visit, void* arg)
{
    Py_VISIT(Py_TYPE(self));
    Py_VISIT(self->md);
    Py_VISIT(self->result);
    return 0;
}

static inline int
multidict_iter_tp_clear(MultidictIter* self)
{
    Py_CLEAR(self->md);
    Py_CLEAR(self->result);
    return 0;
}

static inline PyObject*
multidict_iter_len(MultidictIter* self)
{
    return PyLong_FromLong(md_len(self->md));
}

PyDoc_STRVAR(length_hint_doc,
             "Private method returning an estimate of len(list(it)).");

static PyMethodDef multidict_iter_methods[] = {
    {"__length_hint__",
     (PyCFunction)(void (*)(void))multidict_iter_len,
     METH_NOARGS,
     length_hint_doc},
    {NULL, NULL} /* sentinel */
};

/***********************************************************************/

static PyType_Slot multidict_items_iter_slots[] = {
    {Py_tp_dealloc, multidict_iter_tp_dealloc},
    {Py_tp_methods, multidict_iter_methods},
    {Py_tp_traverse, multidict_iter_tp_traverse},
    {Py_tp_clear, multidict_iter_tp_clear},
    {Py_tp_iter, PyObject_SelfIter},
    {Py_tp_iternext, multidict_items_iter_tp_iternext},
    {0, NULL},
};

static PyType_Spec multidict_items_iter_spec = {
    .name = "multidict._multidict._itemsiter",
    .basicsize = sizeof(MultidictIter),
    .flags = (Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE |
              Py_TPFLAGS_DISALLOW_INSTANTIATION | Py_TPFLAGS_HAVE_GC),
    .slots = multidict_items_iter_slots,
};

static PyType_Slot multidict_values_iter_slots[] = {
    {Py_tp_dealloc, multidict_iter_tp_dealloc},
    {Py_tp_methods, multidict_iter_methods},
    {Py_tp_traverse, multidict_iter_tp_traverse},
    {Py_tp_clear, multidict_iter_tp_clear},
    {Py_tp_iter, PyObject_SelfIter},
    {Py_tp_iternext, multidict_values_iter_tp_iternext},
    {0, NULL},
};

static PyType_Spec multidict_values_iter_spec = {
    .name = "multidict._multidict._valuesiter",
    .basicsize = sizeof(MultidictIter),
    .flags = (Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE |
              Py_TPFLAGS_DISALLOW_INSTANTIATION | Py_TPFLAGS_HAVE_GC),
    .slots = multidict_values_iter_slots,
};

static PyType_Slot multidict_keys_iter_slots[] = {
    {Py_tp_dealloc, multidict_iter_tp_dealloc},
    {Py_tp_methods, multidict_iter_methods},
    {Py_tp_traverse, multidict_iter_tp_traverse},
    {Py_tp_clear, multidict_iter_tp_clear},
    {Py_tp_iter, PyObject_SelfIter},
    {Py_tp_iternext, multidict_keys_iter_tp_iternext},
    {0, NULL},
};

static PyType_Spec multidict_keys_iter_spec = {
    .name = "multidict._multidict._keysiter",
    .basicsize = sizeof(MultidictIter),
    .flags = (Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE |
              Py_TPFLAGS_DISALLOW_INSTANTIATION | Py_TPFLAGS_HAVE_GC),
    .slots = multidict_keys_iter_slots,
};

static inline int
multidict_iter_init(PyObject* module, mod_state* state)
{
    PyObject* tmp;
    tmp = PyType_FromModuleAndSpec(module, &multidict_items_iter_spec, NULL);
    if (tmp == NULL) {
        return -1;
    }
    state->ItemsIterType = (PyTypeObject*)tmp;

    tmp = PyType_FromModuleAndSpec(module, &multidict_values_iter_spec, NULL);
    if (tmp == NULL) {
        return -1;
    }
    state->ValuesIterType = (PyTypeObject*)tmp;

    tmp = PyType_FromModuleAndSpec(module, &multidict_keys_iter_spec, NULL);
    if (tmp == NULL) {
        return -1;
    }
    state->KeysIterType = (PyTypeObject*)tmp;

    return 0;
}

#ifdef __cplusplus
}
#endif
#endif
