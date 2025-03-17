#ifndef _MULTIDICT_VIEWS_H
#define _MULTIDICT_VIEWS_H

#include "state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    PyObject_HEAD
    PyObject *md;
} _Multidict_ViewObject;


/********** Base **********/

static inline void
_init_view(_Multidict_ViewObject *self, PyObject *md)
{
    Py_INCREF(md);
    self->md = md;
}

static inline void
multidict_view_dealloc(_Multidict_ViewObject *self)
{
    PyObject_GC_UnTrack(self);
    Py_XDECREF(self->md);
    PyObject_GC_Del(self);
}

static inline int
multidict_view_traverse(_Multidict_ViewObject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->md);
    return 0;
}

static inline int
multidict_view_clear(_Multidict_ViewObject *self)
{
    Py_CLEAR(self->md);
    return 0;
}

static inline Py_ssize_t
multidict_view_len(_Multidict_ViewObject *self)
{
    return pair_list_len(&((MultiDictObject*)self->md)->pairs);
}

static inline PyObject *
multidict_view_richcompare(PyObject *self, PyObject *other, int op)
{
    PyObject *ret;
    PyObject *op_obj = PyLong_FromLong(op);
    if (op_obj == NULL) {
        return NULL;
    }
    multidict_state *state = get_multidict_state_by_def(self);
    ret = PyObject_CallFunctionObjArgs(
        state->viewbaseset_richcmp_func, self, other, op_obj, NULL);
    Py_DECREF(op_obj);
    return ret;
}

static inline PyObject *
multidict_view_and(PyObject *self, PyObject *other)
{
    multidict_state *state = get_multidict_state_by_def(self);
    return PyObject_CallFunctionObjArgs(
        state->viewbaseset_and_func, self, other, NULL);
}

static inline PyObject *
multidict_view_or(PyObject *self, PyObject *other)
{
    multidict_state *state = get_multidict_state_by_def(self);
    return PyObject_CallFunctionObjArgs(
        state->viewbaseset_or_func, self, other, NULL);
}

static inline PyObject *
multidict_view_sub(PyObject *self, PyObject *other)
{
    multidict_state *state = get_multidict_state_by_def(self);
    return PyObject_CallFunctionObjArgs(
        state->viewbaseset_sub_func, self, other, NULL);
}

static inline PyObject *
multidict_view_xor(PyObject *self, PyObject *other)
{
    multidict_state *state = get_multidict_state_by_def(self);
    return PyObject_CallFunctionObjArgs(
        state->viewbaseset_xor_func, self, other, NULL);
}

/********** Items **********/

static inline PyObject *
multidict_itemsview_new(PyObject *md)
{
    multidict_state *state = get_multidict_state_by_def(md);
    _Multidict_ViewObject *mv = PyObject_GC_New(
        _Multidict_ViewObject, state->ItemsViewType);
    if (mv == NULL) {
        return NULL;
    }

    _init_view(mv, md);

    PyObject_GC_Track(mv);
    return (PyObject *)mv;
}

static inline PyObject *
multidict_itemsview_iter(_Multidict_ViewObject *self)
{
    return multidict_items_iter_new((MultiDictObject*)self->md);
}

static inline PyObject *
multidict_itemsview_repr(_Multidict_ViewObject *self)
{
    multidict_state *state = get_multidict_state_by_def((PyObject*)self);
    return PyObject_CallFunctionObjArgs(
        state->itemsview_repr_func, self, NULL);
}

static inline PyObject *
multidict_itemsview_isdisjoint(_Multidict_ViewObject *self, PyObject *other)
{
    multidict_state *state = get_multidict_state_by_def((PyObject*)self);
    return PyObject_CallFunctionObjArgs(
        state->itemsview_isdisjoint_func, self, other, NULL);
}

PyDoc_STRVAR(itemsview_isdisjoint_doc,
             "Return True if two sets have a null intersection.");

static PyMethodDef multidict_itemsview_methods[] = {
    {
        "isdisjoint",
        (PyCFunction)multidict_itemsview_isdisjoint,
        METH_O,
        itemsview_isdisjoint_doc
    },
    {
        NULL,
        NULL
    }   /* sentinel */
};

static inline int
multidict_itemsview_contains(_Multidict_ViewObject *self, PyObject *obj)
{
    PyObject *akey  = NULL,
             *aval  = NULL,
             *bkey  = NULL,
             *bval  = NULL,
             *iter  = NULL,
             *item  = NULL;
    int ret1, ret2;

    if (!PyTuple_Check(obj) || PyTuple_GET_SIZE(obj) != 2) {
        return 0;
    }

    bkey = PyTuple_GET_ITEM(obj, 0);
    bval = PyTuple_GET_ITEM(obj, 1);

    iter = multidict_itemsview_iter(self);
    if (iter == NULL) {
        return 0;
    }

    while ((item = PyIter_Next(iter)) != NULL) {
        akey = PyTuple_GET_ITEM(item, 0);
        aval = PyTuple_GET_ITEM(item, 1);

        ret1 = PyObject_RichCompareBool(akey, bkey, Py_EQ);
        if (ret1 < 0) {
            Py_DECREF(iter);
            Py_DECREF(item);
            return -1;
        }
        ret2 = PyObject_RichCompareBool(aval, bval, Py_EQ);
        if (ret2 < 0) {
            Py_DECREF(iter);
            Py_DECREF(item);
            return -1;
        }
        if (ret1 > 0 && ret2 > 0)
        {
            Py_DECREF(iter);
            Py_DECREF(item);
            return 1;
        }

        Py_DECREF(item);
    }

    Py_DECREF(iter);

    if (PyErr_Occurred()) {
        return -1;
    }

    return 0;
}


PyDoc_STRVAR(multidict_itemsview__doc__,
             "multidict ItemsView class implementation");

static PyType_Slot ItemsViewType_slots[] = {
    {Py_nb_subtract, multidict_view_sub},
    {Py_nb_and, multidict_view_and},
    {Py_nb_xor, multidict_view_xor},
    {Py_nb_or, multidict_view_or},
    {Py_sq_length, multidict_view_len},
    {Py_sq_contains, multidict_itemsview_contains},
    {Py_tp_dealloc, multidict_view_dealloc},
    {Py_tp_doc, (void*)multidict_itemsview__doc__},
    {Py_tp_repr, multidict_itemsview_repr},
    {Py_tp_getattro, PyObject_GenericGetAttr},
    {Py_tp_traverse, multidict_view_traverse},
    {Py_tp_clear, multidict_view_clear},
    {Py_tp_richcompare, multidict_view_richcompare},
    {Py_tp_iter, multidict_itemsview_iter},
    {Py_tp_methods, multidict_itemsview_methods},
    {0, NULL},
};

static PyType_Spec ItemsViewType_spec = {
    .name = "multidict._multidict._ItemsView",
    .basicsize = sizeof(_Multidict_ViewObject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | _TPFLAGS_HEAPTYPE,
    .slots = ItemsViewType_slots,
};


/********** Keys **********/

static inline PyObject *
multidict_keysview_new(PyObject *md)
{
    multidict_state *state = get_multidict_state_by_def(md);
    _Multidict_ViewObject *mv = PyObject_GC_New(
        _Multidict_ViewObject, state->KeysViewType);
    if (mv == NULL) {
        return NULL;
    }

    _init_view(mv, md);

    PyObject_GC_Track(mv);
    return (PyObject *)mv;
}

static inline PyObject *
multidict_keysview_iter(_Multidict_ViewObject *self)
{
    return multidict_keys_iter_new(((MultiDictObject*)self->md));
}

static inline PyObject *
multidict_keysview_repr(_Multidict_ViewObject *self)
{
    multidict_state *state = get_multidict_state_by_def((PyObject*)self);
    return PyObject_CallFunctionObjArgs(
        state->keysview_repr_func, self, NULL);
}

static inline PyObject *
multidict_keysview_isdisjoint(_Multidict_ViewObject *self, PyObject *other)
{
    multidict_state *state = get_multidict_state_by_def((PyObject*)self);
    return PyObject_CallFunctionObjArgs(
        state->keysview_isdisjoint_func, self, other, NULL);
}

PyDoc_STRVAR(keysview_isdisjoint_doc,
             "Return True if two sets have a null intersection.");

static PyMethodDef multidict_keysview_methods[] = {
    {
        "isdisjoint",
        (PyCFunction)multidict_keysview_isdisjoint,
        METH_O,
        keysview_isdisjoint_doc
    },
    {
        NULL,
        NULL
    }   /* sentinel */
};

static inline int
multidict_keysview_contains(_Multidict_ViewObject *self, PyObject *key)
{
    multidict_state *state = get_multidict_state_by_def((PyObject*)self);
    return pair_list_contains(state, &((MultiDictObject*)self->md)->pairs, key);
}

PyDoc_STRVAR(multidict_keysview__doc__,
             "multidict KeysView class implementation");

static PyType_Slot KeysViewType_slots[] = {
    {Py_nb_subtract, multidict_view_sub},
    {Py_nb_and, multidict_view_and},
    {Py_nb_xor, multidict_view_xor},
    {Py_nb_or, multidict_view_or},
    {Py_sq_length, multidict_view_len},
    {Py_sq_contains, multidict_keysview_contains},
    {Py_tp_dealloc, multidict_view_dealloc},
    {Py_tp_doc, (void*)multidict_keysview__doc__},
    {Py_tp_repr, multidict_keysview_repr},
    {Py_tp_getattro, PyObject_GenericGetAttr},
    {Py_tp_traverse, multidict_view_traverse},
    {Py_tp_clear, multidict_view_clear},
    {Py_tp_richcompare, multidict_view_richcompare},
    {Py_tp_iter, multidict_keysview_iter},
    {Py_tp_methods, multidict_keysview_methods},
    {0, NULL},
};

static PyType_Spec KeysViewType_spec = {
    .name = "multidict._multidict._KeysView",
    .basicsize = sizeof(_Multidict_ViewObject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | _TPFLAGS_HEAPTYPE,
    .slots = KeysViewType_slots,
};


/********** Values **********/

static inline PyObject *
multidict_valuesview_new(PyObject *md)
{
    multidict_state *state = get_multidict_state_by_def(md);
    _Multidict_ViewObject *mv = PyObject_GC_New(
        _Multidict_ViewObject, state->ValuesViewType);
    if (mv == NULL) {
        return NULL;
    }

    _init_view(mv, md);

    PyObject_GC_Track(mv);
    return (PyObject *)mv;
}

static inline PyObject *
multidict_valuesview_iter(_Multidict_ViewObject *self)
{
    return multidict_values_iter_new(((MultiDictObject*)self->md));
}

static inline PyObject *
multidict_valuesview_repr(_Multidict_ViewObject *self)
{
    multidict_state *state = get_multidict_state_by_def((PyObject*)self);
    return PyObject_CallFunctionObjArgs(
        state->valuesview_repr_func, self, NULL);
}

PyDoc_STRVAR(multidict_valuesview__doc__,
             "multidict ValuesView class implementation");


static PyType_Slot ValuesViewType_slots[] = {
    {Py_nb_subtract, multidict_view_sub},
    {Py_nb_and, multidict_view_and},
    {Py_nb_xor, multidict_view_xor},
    {Py_nb_or, multidict_view_or},
    {Py_sq_length, multidict_view_len},
    {Py_tp_dealloc, multidict_view_dealloc},
    {Py_tp_doc, (void*)multidict_valuesview__doc__},
    {Py_tp_repr, multidict_valuesview_repr},
    {Py_tp_getattro, PyObject_GenericGetAttr},
    {Py_tp_traverse, multidict_view_traverse},
    {Py_tp_clear, multidict_view_clear},
    {Py_tp_richcompare, multidict_view_richcompare},
    {Py_tp_iter, multidict_valuesview_iter},
    {0, NULL},
};

static PyType_Spec ValuesViewType_spec = {
    .name = "multidict._multidict._ValuesView",
    .basicsize = sizeof(_Multidict_ViewObject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | _TPFLAGS_HEAPTYPE,
    .slots = ValuesViewType_slots,
};

#ifdef __cplusplus
}
#endif
#endif
