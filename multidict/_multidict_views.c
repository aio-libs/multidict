#include "_multidict_views.h"
#include "_multidict_iter.h"
#include "_pair_list.h"

#include <Python.h>

// fix for VisualC complier used by Python 3.4
#ifdef __GNUC__
#define INLINE inline
#else
#define INLINE
#endif

/* We link this module statically for convenience.  If compiled as a shared
   library instead, some compilers don't allow addresses of Python objects
   defined in other libraries to be used in static initializers here.  The
   DEFERRED_ADDRESS macro is used to tag the slots where such addresses
   appear; the module init function must fill in the tagged slots at runtime.
   The argument is for documentation -- the macro ignores it.
*/
#define DEFERRED_ADDRESS(ADDR) 0

_Py_IDENTIFIER(impl);

static PyTypeObject multidict_itemsview_type;
static PyTypeObject multidict_valuesview_type;
static PyTypeObject multidict_keysview_type;

static PyObject *viewbaseset_richcmp_func;
static PyObject *viewbaseset_and_func;
static PyObject *viewbaseset_or_func;
static PyObject *viewbaseset_sub_func;
static PyObject *viewbaseset_xor_func;

static PyObject *itemsview_isdisjoint_func;
static PyObject *itemsview_repr_func;

static PyObject *abc_itemsview_register_func;

typedef struct {
    PyObject_HEAD
    PyObject *md;
} _Multidict_ViewObject;


/********** Base **********/

static INLINE void
_init_view(_Multidict_ViewObject *self, PyObject *md)
{
    self->md = md;
}

static void
multidict_view_dealloc(_Multidict_ViewObject *self)
{
    PyObject_GC_UnTrack(self);
    Py_XDECREF(self->md);
    PyObject_GC_Del(self);
}

static Py_ssize_t
multidict_view_len(_Multidict_ViewObject *self)
{
    Py_ssize_t len = 0;
    if (self->md != NULL) {
        PyObject *impl = _PyObject_CallMethodId(self->md, &PyId_impl, NULL);
        len = pair_list_len(impl);
    }
    return len;
}

static PyObject *
multidict_view_richcompare(PyObject *self, PyObject *other, int op)
{
    PyObject *op_obj = PyLong_FromLong(op);
    return PyObject_CallFunctionObjArgs(
        viewbaseset_richcmp_func, self, other, op_obj, NULL);
}

static PyObject *
multidict_view_and(PyObject *self, PyObject *other)
{
    return PyObject_CallFunctionObjArgs(
        viewbaseset_and_func, self, other, NULL);
}

static PyObject *
multidict_view_or(PyObject *self, PyObject *other)
{
    return PyObject_CallFunctionObjArgs(
        viewbaseset_or_func, self, other, NULL);
}

static PyObject *
multidict_view_sub(PyObject *self, PyObject *other)
{
    return PyObject_CallFunctionObjArgs(
        viewbaseset_sub_func, self, other, NULL);
}

static PyObject *
multidict_view_xor(PyObject *self, PyObject *other)
{
    return PyObject_CallFunctionObjArgs(
        viewbaseset_xor_func, self, other, NULL);
}


/********** Items **********/

PyObject *
multidict_itemsview_new(PyObject *md)
{
    _Multidict_ViewObject *mv = PyObject_GC_New(
        _Multidict_ViewObject, &multidict_itemsview_type);
    if (mv == NULL) {
        return NULL;
    }

    Py_INCREF(md);

    _init_view(mv, md);

    return (PyObject *)mv;
}

static PyObject *
multidict_itemsview_iter(_Multidict_ViewObject *self)
{
    if (self->md == NULL) {
        Py_RETURN_NONE;
    }

    PyObject *impl = _PyObject_CallMethodId(self->md, &PyId_impl, NULL);
    PyObject *iter = multidict_items_iter_new(impl);

    return iter;
}

static PyObject *
multidict_itemsview_repr(_Multidict_ViewObject *self)
{
    return PyObject_CallFunctionObjArgs(
        itemsview_repr_func, self, NULL);
}

static PyObject *
multidict_itemsview_isdisjoint(_Multidict_ViewObject *self, PyObject *other)
{
    return PyObject_CallFunctionObjArgs(
        itemsview_isdisjoint_func, self, other, NULL);
}

PyDoc_STRVAR(isdisjoint_doc,
             "Return True if two sets have a null intersection.");

static PyMethodDef multidict_itemsview_methods[] = {
    {
        "isdisjoint",
        (PyCFunction)multidict_itemsview_isdisjoint,
        METH_O,
        isdisjoint_doc
    },
    {
        NULL,
        NULL
    }   /* sentinel */
};

static int
multidict_itemsview_contains(_Multidict_ViewObject *self, PyObject *obj)
{
    int contains = 0;
    
    PyObject *akey  = NULL,
             *aval  = NULL,
             *bkey  = NULL,
             *bval  = NULL,
             *iter  = NULL,
             *item  = NULL;

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

        if (PyObject_RichCompareBool(akey, bkey, Py_EQ) > 0 &&
            PyObject_RichCompareBool(aval, bval, Py_EQ) > 0)
        {
            contains = 1;
            goto ret;
        }
        
        Py_DECREF(item);
    }

ret:
    return contains;
}

static PySequenceMethods multidict_itemsview_as_sequence = {
    (lenfunc)multidict_view_len,               /* sq_length */
    0,                                         /* sq_concat */
    0,                                         /* sq_repeat */
    0,                                         /* sq_item */
    0,                                         /* sq_slice */
    0,                                         /* sq_ass_item */
    0,                                         /* sq_ass_slice */
    (objobjproc)multidict_itemsview_contains, /* sq_contains */
};

static PyNumberMethods multidict_itemsview_as_number = {
    0,                              /* nb_add */
    (binaryfunc)multidict_view_sub, /* nb_subtract */
    0,                              /* nb_multiply */
    0,                              /* nb_remainder */
    0,                              /* nb_divmod */
    0,                              /* nb_power */
    0,                              /* nb_negative */
    0,                              /* nb_positive */
    0,                              /* nb_absolute */
    0,                              /* nb_bool */
    0,                              /* nb_invert */
    0,                              /* nb_lshift */
    0,                              /* nb_rshift */
    (binaryfunc)multidict_view_and, /* nb_and */
    (binaryfunc)multidict_view_xor, /* nb_xor */
    (binaryfunc)multidict_view_or,  /* nb_or */
};

static PyTypeObject multidict_itemsview_type = {
    PyVarObject_HEAD_INIT(DEFERRED_ADDRESS(&PyType_Type), 0)
    "_ItemsView",                                   /* tp_name */
    sizeof(_Multidict_ViewObject),                  /* tp_basicsize */
    0,                                              /* tp_itemsize */
    (destructor)multidict_view_dealloc,             /* tp_dealloc */
    0,                                              /* tp_print */
    0,                                              /* tp_getattr */
    0,                                              /* tp_setattr */
    0,                                              /* tp_reserved */
    (reprfunc)multidict_itemsview_repr,             /* tp_repr */
    &multidict_itemsview_as_number,                 /* tp_as_number */
    &multidict_itemsview_as_sequence,               /* tp_as_sequence */
    0,                                              /* tp_as_mapping */
    0,                                              /* tp_hash */
    0,                                              /* tp_call */
    0,                                              /* tp_str */
    0,                                              /* tp_getattro */
    0,                                              /* tp_setattro */
    0,                                              /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,        /* tp_flags */
    0,                                              /* tp_doc */
    0,                                              /* tp_traverse */
    0,                                              /* tp_clear */
    multidict_view_richcompare,                     /* tp_richcompare */
    0,                                              /* tp_weaklistoffset */
    (getiterfunc)multidict_itemsview_iter,          /* tp_iter */
    0,                                              /* tp_iternext */
    multidict_itemsview_methods,                    /* tp_methods */
};


/********** Keys **********/

PyObject *
multidict_keysview_new(PyObject *md)
{
    _Multidict_ViewObject *mv = PyObject_GC_New(
        _Multidict_ViewObject, &multidict_keysview_type);
    if (mv == NULL) {
        return NULL;
    }

    Py_INCREF(md);

    _init_view(mv, md);

    return (PyObject *)mv;
}

static PyObject *
multidict_keysview_iter(_Multidict_ViewObject *self)
{
    if (self->md == NULL) {
        Py_RETURN_NONE;
    }
    PyObject *impl = _PyObject_CallMethodId(self->md, &PyId_impl, NULL);
    return multidict_keys_iter_new(impl);
}

static int
multidict_keysview_contains(_Multidict_ViewObject *self, PyObject *key)
{
    PyObject *impl = _PyObject_CallMethodId(self->md, &PyId_impl, NULL);
    return pair_list_contains(impl, key);
}

static PySequenceMethods multidict_keysview_as_sequence = {
    (lenfunc)multidict_view_len,               /* sq_length */
    0,                                         /* sq_concat */
    0,                                         /* sq_repeat */
    0,                                         /* sq_item */
    0,                                         /* sq_slice */
    0,                                         /* sq_ass_item */
    0,                                         /* sq_ass_slice */
    (objobjproc)multidict_keysview_contains,   /* sq_contains */
};

static PyTypeObject multidict_keysview_type = {
    PyVarObject_HEAD_INIT(DEFERRED_ADDRESS(&PyType_Type), 0)
    "_KeysView",                                   /* tp_name */
    sizeof(_Multidict_ViewObject),                 /* tp_basicsize */
    0,                                             /* tp_itemsize */
    (destructor)multidict_view_dealloc,            /* tp_dealloc */
    0,                                             /* tp_print */
    0,                                             /* tp_getattr */
    0,                                             /* tp_setattr */
    0,                                             /* tp_reserved */
    0,                                             /* tp_repr */
    0,                                             /* tp_as_number */
    &multidict_keysview_as_sequence,               /* tp_as_sequence */
    0,                                             /* tp_as_mapping */
    0,                                             /* tp_hash */
    0,                                             /* tp_call */
    0,                                             /* tp_str */
    0,                                             /* tp_getattro */
    0,                                             /* tp_setattro */
    0,                                             /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,       /* tp_flags */
    0,                                             /* tp_doc */
    0,                                             /* tp_traverse */
    0,                                             /* tp_clear */
    0,                                             /* tp_richcompare */
    0,                                             /* tp_weaklistoffset */
    (getiterfunc)multidict_keysview_iter,          /* tp_iter */
    0,                                             /* tp_iternext */
};


/********** Values **********/

PyObject *
multidict_valuesview_new(PyObject *md)
{
    _Multidict_ViewObject *mv = PyObject_GC_New(
        _Multidict_ViewObject, &multidict_valuesview_type);
    if (mv == NULL) {
        return NULL;
    }

    Py_INCREF(md);

    _init_view(mv, md);

    return (PyObject *)mv;
}

static PyObject *
multidict_valuesview_iter(_Multidict_ViewObject *self)
{
    if (self->md == NULL) {
        Py_RETURN_NONE;
    }
    PyObject *impl = _PyObject_CallMethodId(self->md, &PyId_impl, NULL);
    return multidict_values_iter_new(impl);
}

static PyTypeObject multidict_valuesview_type = {
    PyVarObject_HEAD_INIT(DEFERRED_ADDRESS(&PyType_Type), 0)
    "_ValuesView",                                   /* tp_name */
    sizeof(_Multidict_ViewObject),                   /* tp_basicsize */
    0,                                               /* tp_itemsize */
    (destructor)multidict_view_dealloc,              /* tp_dealloc */
    0,                                               /* tp_print */
    0,                                               /* tp_getattr */
    0,                                               /* tp_setattr */
    0,                                               /* tp_reserved */
    0,                                               /* tp_repr */
    0,                                               /* tp_as_number */
    0,                                               /* tp_as_sequence */
    0,                                               /* tp_as_mapping */
    0,                                               /* tp_hash */
    0,                                               /* tp_call */
    0,                                               /* tp_str */
    0,                                               /* tp_getattro */
    0,                                               /* tp_setattro */
    0,                                               /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,         /* tp_flags */
    0,                                               /* tp_doc */
    0,                                               /* tp_traverse */
    0,                                               /* tp_clear */
    0,                                               /* tp_richcompare */
    0,                                               /* tp_weaklistoffset */
    (getiterfunc)multidict_valuesview_iter,          /* tp_iter */
    0,                                               /* tp_iternext */
};

int
multidict_views_init()
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

    GET_MOD_ATTR(itemsview_repr_func, "_itemsview_isdisjoint");
    GET_MOD_ATTR(itemsview_repr_func, "_itemsview_repr");
    GET_MOD_ATTR(abc_itemsview_register_func, "_abc_itemsview_register");
    
    if (multidict_iter_init() < 0) {
        goto fail;
    }
    
    if (PyType_Ready(&multidict_itemsview_type) < 0
        // PyType_Ready(&multidict_values_views_type) < 0 ||
        // PyType_Ready(&multidict_keys_views_type) < 0
        ) {
        goto fail;
    }

    // abc.ItemsView.register(_ItemsView)
    PyObject_CallFunctionObjArgs(
        abc_itemsview_register_func, (PyObject*)&multidict_itemsview_type, NULL);

    Py_DECREF(module);
    return 0;

fail:
    Py_CLEAR(module);
    return -1;

#undef GET_MOD_ATTR
}
