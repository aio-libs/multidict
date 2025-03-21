#ifndef _MULTIDICT_VIEWS_H
#define _MULTIDICT_VIEWS_H

#ifdef __cplusplus
extern "C" {
#endif

static PyTypeObject multidict_itemsview_type;
static PyTypeObject multidict_valuesview_type;
static PyTypeObject multidict_keysview_type;

static PyObject *viewbaseset_richcmp_func;
static PyObject *viewbaseset_and_func;
static PyObject *viewbaseset_or_func;
static PyObject *viewbaseset_sub_func;
static PyObject *viewbaseset_xor_func;

static PyObject *itemsview_isdisjoint_func;

static PyObject *keysview_isdisjoint_func;

typedef struct {
    PyObject_HEAD
    MultiDictObject *md;
} _Multidict_ViewObject;


/********** Base **********/

static inline void
_init_view(_Multidict_ViewObject *self, MultiDictObject *md)
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
    return pair_list_len(&self->md->pairs);
}

static inline PyObject *
multidict_view_richcompare(PyObject *self, PyObject *other, int op)
{
    PyObject *ret;
    PyObject *op_obj = PyLong_FromLong(op);
    if (op_obj == NULL) {
        return NULL;
    }
    ret = PyObject_CallFunctionObjArgs(
        viewbaseset_richcmp_func, self, other, op_obj, NULL);
    Py_DECREF(op_obj);
    return ret;
}

static inline PyObject *
multidict_view_and(PyObject *self, PyObject *other)
{
    return PyObject_CallFunctionObjArgs(
        viewbaseset_and_func, self, other, NULL);
}

static inline PyObject *
multidict_view_or(PyObject *self, PyObject *other)
{
    return PyObject_CallFunctionObjArgs(
        viewbaseset_or_func, self, other, NULL);
}

static inline PyObject *
multidict_view_sub(PyObject *self, PyObject *other)
{
    return PyObject_CallFunctionObjArgs(
        viewbaseset_sub_func, self, other, NULL);
}

static inline PyObject *
multidict_view_xor(PyObject *self, PyObject *other)
{
    return PyObject_CallFunctionObjArgs(
        viewbaseset_xor_func, self, other, NULL);
}

static PyNumberMethods multidict_view_as_number = {
    .nb_subtract = (binaryfunc)multidict_view_sub,
    .nb_and = (binaryfunc)multidict_view_and,
    .nb_xor = (binaryfunc)multidict_view_xor,
    .nb_or = (binaryfunc)multidict_view_or,
};

/********** Items **********/

static inline PyObject *
multidict_itemsview_new(MultiDictObject *md)
{
    _Multidict_ViewObject *mv = PyObject_GC_New(
        _Multidict_ViewObject, &multidict_itemsview_type);
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
    return multidict_items_iter_new(self->md);
}

static inline PyObject *
multidict_itemsview_repr(_Multidict_ViewObject *self)
{
    PyObject *name = PyObject_GetAttrString((PyObject*)Py_TYPE(self), "__name__");
    if (name == NULL)
        return NULL;
    PyObject *ret = _do_multidict_repr(self->md, name, true, true);
    Py_CLEAR(name);
    return ret;
}

static inline PyObject *
multidict_itemsview_isdisjoint(_Multidict_ViewObject *self, PyObject *other)
{
    return PyObject_CallFunctionObjArgs(
        itemsview_isdisjoint_func, self, other, NULL);
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

static PySequenceMethods multidict_itemsview_as_sequence = {
    .sq_length = (lenfunc)multidict_view_len,
    .sq_contains = (objobjproc)multidict_itemsview_contains,
};

static PyTypeObject multidict_itemsview_type = {
    PyVarObject_HEAD_INIT(DEFERRED_ADDRESS(&PyType_Type), 0)
    "multidict._multidict._ItemsView",              /* tp_name */
    sizeof(_Multidict_ViewObject),                  /* tp_basicsize */
    .tp_dealloc = (destructor)multidict_view_dealloc,
    .tp_repr = (reprfunc)multidict_itemsview_repr,
    .tp_as_number = &multidict_view_as_number,
    .tp_as_sequence = &multidict_itemsview_as_sequence,
    .tp_getattro = PyObject_GenericGetAttr,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_traverse = (traverseproc)multidict_view_traverse,
    .tp_clear = (inquiry)multidict_view_clear,
    .tp_richcompare = multidict_view_richcompare,
    .tp_iter = (getiterfunc)multidict_itemsview_iter,
    .tp_methods = multidict_itemsview_methods,
};


/********** Keys **********/

static inline PyObject *
multidict_keysview_new(MultiDictObject *md)
{
    _Multidict_ViewObject *mv = PyObject_GC_New(
        _Multidict_ViewObject, &multidict_keysview_type);
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
    return multidict_keys_iter_new(self->md);
}

static inline PyObject *
multidict_keysview_repr(_Multidict_ViewObject *self)
{
    PyObject *name = PyObject_GetAttrString((PyObject*)Py_TYPE(self), "__name__");
    if (name == NULL)
        return NULL;
    PyObject *ret = _do_multidict_repr(self->md, name, true, false);
    Py_CLEAR(name);
    return ret;
}

static inline PyObject *
multidict_keysview_isdisjoint(_Multidict_ViewObject *self, PyObject *other)
{
    return PyObject_CallFunctionObjArgs(
        keysview_isdisjoint_func, self, other, NULL);
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
    return pair_list_contains(&self->md->pairs, key);
}

static PySequenceMethods multidict_keysview_as_sequence = {
    .sq_length = (lenfunc)multidict_view_len,
    .sq_contains = (objobjproc)multidict_keysview_contains,
};

static PyTypeObject multidict_keysview_type = {
    PyVarObject_HEAD_INIT(DEFERRED_ADDRESS(&PyType_Type), 0)
    "multidict._multidict._KeysView",              /* tp_name */
    sizeof(_Multidict_ViewObject),                 /* tp_basicsize */
    .tp_dealloc = (destructor)multidict_view_dealloc,
    .tp_repr = (reprfunc)multidict_keysview_repr,
    .tp_as_number = &multidict_view_as_number,
    .tp_as_sequence = &multidict_keysview_as_sequence,
    .tp_getattro = PyObject_GenericGetAttr,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_traverse = (traverseproc)multidict_view_traverse,
    .tp_clear = (inquiry)multidict_view_clear,
    .tp_richcompare = multidict_view_richcompare,
    .tp_iter = (getiterfunc)multidict_keysview_iter,
    .tp_methods = multidict_keysview_methods,
};


/********** Values **********/

static inline PyObject *
multidict_valuesview_new(MultiDictObject *md)
{
    _Multidict_ViewObject *mv = PyObject_GC_New(
        _Multidict_ViewObject, &multidict_valuesview_type);
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
    return multidict_values_iter_new(self->md);
}

static inline PyObject *
multidict_valuesview_repr(_Multidict_ViewObject *self)
{
    PyObject *name = PyObject_GetAttrString((PyObject*)Py_TYPE(self), "__name__");
    if (name == NULL)
        return NULL;
    PyObject *ret = _do_multidict_repr(self->md, name, false, true);
    Py_CLEAR(name);
    return ret;
}

static PySequenceMethods multidict_valuesview_as_sequence = {
    .sq_length = (lenfunc)multidict_view_len,
};

static PyTypeObject multidict_valuesview_type = {
    PyVarObject_HEAD_INIT(DEFERRED_ADDRESS(&PyType_Type), 0)
    "multidict._multidict._ValuesView",              /* tp_name */
    sizeof(_Multidict_ViewObject),                   /* tp_basicsize */
    .tp_dealloc = (destructor)multidict_view_dealloc,
    .tp_repr = (reprfunc)multidict_valuesview_repr,
    .tp_as_sequence = &multidict_valuesview_as_sequence,
    .tp_getattro = PyObject_GenericGetAttr,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_traverse = (traverseproc)multidict_view_traverse,
    .tp_clear = (inquiry)multidict_view_clear,
    .tp_iter = (getiterfunc)multidict_valuesview_iter,
};


static inline int
multidict_views_init(void)
{
    PyObject *module = PyImport_ImportModule("multidict._multidict_base");
    if (module == NULL) {
        goto fail;
    }

#define GET_MOD_ATTR(VAR, NAME)                 \
    VAR = PyObject_GetAttrString(module, NAME); \
    if (VAR == NULL) {                          \
        goto fail;                              \
    }

    GET_MOD_ATTR(viewbaseset_richcmp_func, "_viewbaseset_richcmp");
    GET_MOD_ATTR(viewbaseset_and_func, "_viewbaseset_and");
    GET_MOD_ATTR(viewbaseset_or_func, "_viewbaseset_or");
    GET_MOD_ATTR(viewbaseset_sub_func, "_viewbaseset_sub");
    GET_MOD_ATTR(viewbaseset_xor_func, "_viewbaseset_xor");

    GET_MOD_ATTR(itemsview_isdisjoint_func, "_itemsview_isdisjoint");

    GET_MOD_ATTR(keysview_isdisjoint_func, "_keysview_isdisjoint");


    if (PyType_Ready(&multidict_itemsview_type) < 0 ||
        PyType_Ready(&multidict_valuesview_type) < 0 ||
        PyType_Ready(&multidict_keysview_type) < 0)
    {
        goto fail;
    }


    Py_DECREF(module);
    return 0;

fail:
    Py_CLEAR(module);
    return -1;

#undef GET_MOD_ATTR
}

#ifdef __cplusplus
}
#endif
#endif
