#ifndef _MULTIDICT_ITER_H
#define _MULTIDICT_ITER_H

#include "state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct multidict_iter {
    PyObject_HEAD
    MultiDictObject *md;  // MultiDict or CIMultiDict
    Py_ssize_t current;
    uint64_t version;
} MultidictIter;

static inline void
_init_iter(MultidictIter *it, MultiDictObject *md)
{
    Py_INCREF(md);

    it->md = md;
    it->current = 0;
    it->version = pair_list_version(&md->pairs);
}

static inline PyObject *
multidict_items_iter_new(MultiDictObject *md)
{
    multidict_state *state = get_multidict_state_by_def((PyObject*)md);
    MultidictIter *it = PyObject_GC_New(
        MultidictIter, state->ItemsIterType);
    if (it == NULL) {
        return NULL;
    }

    _init_iter(it, md);

    PyObject_GC_Track(it);
    return (PyObject *)it;
}

static inline PyObject *
multidict_keys_iter_new(MultiDictObject *md)
{
    multidict_state *state = get_multidict_state_by_def((PyObject*)md);
    MultidictIter *it = PyObject_GC_New(
        MultidictIter, state->KeysIterType);
    if (it == NULL) {
        return NULL;
    }

    _init_iter(it, md);

    PyObject_GC_Track(it);
    return (PyObject *)it;
}

static inline PyObject *
multidict_values_iter_new(MultiDictObject *md)
{
    multidict_state *state = get_multidict_state_by_def((PyObject*)md);
    MultidictIter *it = PyObject_GC_New(
        MultidictIter, state->ValuesIterType);
    if (it == NULL) {
        return NULL;
    }

    _init_iter(it, md);

    PyObject_GC_Track(it);
    return (PyObject *)it;
}

static inline PyObject *
multidict_items_iter_iternext(MultidictIter *self)
{
    PyObject *key = NULL;
    PyObject *value = NULL;
    PyObject *ret = NULL;

    if (self->version != pair_list_version(&self->md->pairs)) {
        PyErr_SetString(PyExc_RuntimeError, "Dictionary changed during iteration");
        return NULL;
    }

    if (!_pair_list_next(&self->md->pairs, &self->current, NULL, &key, &value, NULL)) {
        PyErr_SetNone(PyExc_StopIteration);
        return NULL;
    }

    ret = PyTuple_Pack(2, key, value);
    if (ret == NULL) {
        return NULL;
    }

    return ret;
}

static inline PyObject *
multidict_values_iter_iternext(MultidictIter *self)
{
    PyObject *value = NULL;

    if (self->version != pair_list_version(&self->md->pairs)) {
        PyErr_SetString(PyExc_RuntimeError, "Dictionary changed during iteration");
        return NULL;
    }

    if (!pair_list_next(&self->md->pairs, &self->current, NULL, NULL, &value)) {
        PyErr_SetNone(PyExc_StopIteration);
        return NULL;
    }

    Py_INCREF(value);

    return value;
}

static inline PyObject *
multidict_keys_iter_iternext(MultidictIter *self)
{
    PyObject *key = NULL;

    if (self->version != pair_list_version(&self->md->pairs)) {
        PyErr_SetString(PyExc_RuntimeError, "Dictionary changed during iteration");
        return NULL;
    }

    if (!pair_list_next(&self->md->pairs, &self->current, NULL, &key, NULL)) {
        PyErr_SetNone(PyExc_StopIteration);
        return NULL;
    }

    Py_INCREF(key);

    return key;
}

static inline void
multidict_iter_dealloc(MultidictIter *self)
{
    PyObject_GC_UnTrack(self);
    Py_XDECREF(self->md);
    PyObject_GC_Del(self);
}

static inline int
multidict_iter_traverse(MultidictIter *self, visitproc visit, void *arg)
{
    Py_VISIT(self->md);
    return 0;
}

static inline int
multidict_iter_clear(MultidictIter *self)
{
    Py_CLEAR(self->md);
    return 0;
}

static inline PyObject *
multidict_iter_len(MultidictIter *self)
{
    return PyLong_FromLong(pair_list_len(&self->md->pairs));
}

PyDoc_STRVAR(length_hint_doc,
             "Private method returning an estimate of len(list(it)).");

static PyMethodDef multidict_iter_methods[] = {
    {
        "__length_hint__",
        (PyCFunction)(void(*)(void))multidict_iter_len,
        METH_NOARGS,
        length_hint_doc
    },
    {
        NULL,
        NULL
    }   /* sentinel */
};

/***********************************************************************/

static PyType_Slot ItemsIterType_slots[] = {
    {Py_tp_dealloc, multidict_iter_dealloc},
    {Py_tp_traverse, multidict_iter_traverse},
    {Py_tp_clear, multidict_iter_clear},
    {Py_tp_iter, PyObject_SelfIter},
    {Py_tp_iternext, multidict_items_iter_iternext},
    {Py_tp_methods, multidict_iter_methods},
    {0, NULL},
};

static PyType_Spec ItemsIterType_spec = {
    .name = "multidict._multidict._itemsiter",
    .basicsize = sizeof(MultidictIter),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_HEAPTYPE,
    .slots = ItemsIterType_slots,
};


static PyType_Slot ValuesIterType_slots[] = {
    {Py_tp_dealloc, multidict_iter_dealloc},
    {Py_tp_traverse, multidict_iter_traverse},
    {Py_tp_clear, multidict_iter_clear},
    {Py_tp_iter, PyObject_SelfIter},
    {Py_tp_iternext, multidict_values_iter_iternext},
    {Py_tp_methods, multidict_iter_methods},
    {0, NULL},
};

static PyType_Spec ValuesIterType_spec = {
    .name = "multidict._multidict._valuesiter",
    .basicsize = sizeof(MultidictIter),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_HEAPTYPE,
    .slots = ValuesIterType_slots,
};

static PyType_Slot KeysIterType_slots[] = {
    {Py_tp_dealloc, multidict_iter_dealloc},
    {Py_tp_traverse, multidict_iter_traverse},
    {Py_tp_clear, multidict_iter_clear},
    {Py_tp_iter, PyObject_SelfIter},
    {Py_tp_iternext, multidict_keys_iter_iternext},
    {Py_tp_methods, multidict_iter_methods},
    {0, NULL},
};

static PyType_Spec KeysIterType_spec = {
    .name = "multidict._multidict._valuesiter",
    .basicsize = sizeof(MultidictIter),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_HEAPTYPE,
    .slots = KeysIterType_slots,
};

#ifdef __cplusplus
}
#endif
#endif
