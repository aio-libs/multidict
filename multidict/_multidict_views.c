// #include "_multidict_views.h"
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

static PyTypeObject multidict_items_view_type;
static PyTypeObject multidict_values_view_type;
static PyTypeObject multidict_keys_view_type;

typedef struct {
    PyObject_HEAD
    PyObject *md;
} _MultidictViewObject;


static INLINE void
_init_view(_MultidictViewObject *mv, PyObject *md)
{
    mv->md = md;
}


static void
view_dealloc(_MultidictViewObject *mv)
{
    // PyObject_GC_UnTrack(self);
    // Py_XDECREF(self->impl);
    // PyObject_GC_Del(self);
}

static Py_ssize_t
view_len(_MultidictViewObject *mv)
{
    Py_ssize_t len = 0;
    if (mv->md != NULL) {
        PyObject *impl = PyObject_CallMethod(mv->md, "impl", "");
        len = pair_list_len(impl);
    }
    return len;
}

/********** Items **********/

PyObject *
multidict_items_view_new(PyObject *md)
{
    _MultidictViewObject *mv = PyObject_GC_New(
        _MultidictViewObject, &multidict_items_view_type);
    if (mv == NULL) {
        return NULL;
    }

    Py_INCREF(md);

    _init_view(mv, md);

    return (PyObject *)mv;
}

static PyObject *
items_iter(_MultidictViewObject *self)
{
    if (self->md == NULL) {
        Py_RETURN_NONE;
    }
    
    PyObject *impl = PyObject_CallMethod(self->md, "impl", "");
    PyObject *iter = multidict_items_iter_new(impl);

    return iter;
}

static PyObject *
items_repr(_MultidictViewObject *mv)
{
    PyObject *key    = NULL,
             *val    = NULL,
             *lst    = NULL,
             *iter   = NULL,
             *item   = NULL,
             *str    = NULL,
             *body   = NULL,
             *result = NULL;

    lst = PyList_New(0);
    if (lst == NULL) {
        return result;
    }
    
    iter = items_iter(mv);
    if (iter == NULL) {
        goto ret;
    }

    while ((item = PyIter_Next(iter)) != NULL) {
        key = PyTuple_GET_ITEM(item, 0);
        val = PyTuple_GET_ITEM(item, 1);
        str = PyUnicode_FromFormat("%R: %R", key, val);
 
        if (PyList_Append(lst, str) < 0) {
            goto ret;
        }

        Py_DECREF(item);
    }
    
    body   = PyUnicode_Join(PyUnicode_FromString(", "), lst);
    result = PyUnicode_FromFormat("%s(%U)", Py_TYPE(mv)->tp_name, body);

ret:
    Py_DECREF(lst);
    return result;
}

static PyObject *
isdisjoint(PyObject *self, PyObject *other)
{
    PyObject *it   = NULL,
             *item = NULL;

    int contains = 0;
    
    _MultidictViewObject *mv = (_MultidictViewObject*)self;

    if (self == other) {
        if (view_len(mv) == 0) {
            Py_RETURN_TRUE;
        } else {
            Py_RETURN_FALSE;
        }
    }

    it = PyObject_GetIter(other);
    if (it == NULL) {
        return NULL;
    }

    while ((item = PyIter_Next(it)) != NULL) {
        contains = PySequence_Contains(self, item);

        Py_DECREF(item);
        if (contains == -1) {
            Py_DECREF(it);
            return NULL;
        }

        if (contains) {
            Py_DECREF(it);
            Py_RETURN_FALSE;
        }
    }
    Py_DECREF(it);
    
    if (PyErr_Occurred()) {
        return NULL;
    }

    Py_RETURN_TRUE;
}

PyDoc_STRVAR(isdisjoint_doc,
             "Return True if two sets have a null intersection.");

static PyMethodDef multidict_item_views_methods[] = {
    { "isdisjoint", (PyCFunction)isdisjoint, METH_O, isdisjoint_doc },
    { NULL, NULL }           /* sentinel */
};

static int
items_contains(_MultidictViewObject *self, PyObject *obj)
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

    iter = items_iter(self);
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

static PySequenceMethods multidict_items_as_sequence = {
    (lenfunc)view_len,                  /* sq_length */
    0,                                  /* sq_concat */
    0,                                  /* sq_repeat */
    0,                                  /* sq_item */
    0,                                  /* sq_slice */
    0,                                  /* sq_ass_item */
    0,                                  /* sq_ass_slice */
    (objobjproc)items_contains,         /* sq_contains */
};

static PyTypeObject multidict_items_view_type = {
    PyVarObject_HEAD_INIT(DEFERRED_ADDRESS(&PyType_Type), 0)
    "_ItemsView",                                   /* tp_name */
    sizeof(_MultidictViewObject),                   /* tp_basicsize */
    0,                                              /* tp_itemsize */
    (destructor)view_dealloc,                       /* tp_dealloc */
    0,                                              /* tp_print */
    0,                                              /* tp_getattr */
    0,                                              /* tp_setattr */
    0,                                              /* tp_reserved */
    (reprfunc)items_repr,                           /* tp_repr */
    0,                                              /* tp_as_number */
    &multidict_items_as_sequence,                   /* tp_as_sequence */
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
    0,                                              /* tp_richcompare */
    0,                                              /* tp_weaklistoffset */
    (getiterfunc)items_iter,                        /* tp_iter */
    0,                                              /* tp_iternext */
    multidict_item_views_methods,                   /* tp_methods */
};


/********** Keys **********/

PyObject *
multidict_keys_view_new(PyObject *md)
{
    _MultidictViewObject *mv = PyObject_GC_New(
        _MultidictViewObject, &multidict_keys_view_type);
    if (mv == NULL) {
        return NULL;
    }

    Py_INCREF(md);

    _init_view(mv, md);

    return (PyObject *)mv;
}

static PyObject *
keys_iter(_MultidictViewObject *self)
{
    if (self->md == NULL) {
        Py_RETURN_NONE;
    }
    return multidict_keys_iter_new(PyObject_GetAttrString(self->md, "_impl"));
}

static PyTypeObject multidict_keys_view_type = {
    PyVarObject_HEAD_INIT(DEFERRED_ADDRESS(&PyType_Type), 0)
    "multidict._multidict_views._keysview",        /* tp_name */
    sizeof(_MultidictViewObject),                     /* tp_basicsize */
    0,                                             /* tp_itemsize */
    (destructor)view_dealloc,                      /* tp_dealloc */
    0,                                             /* tp_print */
    0,                                             /* tp_getattr */
    0,                                             /* tp_setattr */
    0,                                             /* tp_reserved */
    0,                                             /* tp_repr */
    0,                                             /* tp_as_number */
    0,                                             /* tp_as_sequence */
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
    (getiterfunc)keys_iter,                        /* tp_iter */
    0,                                             /* tp_iternext */
};


/********** Values **********/

PyObject *
multidict_values_view_new(PyObject *md)
{
    _MultidictViewObject *mv = PyObject_GC_New(
        _MultidictViewObject, &multidict_values_view_type);
    if (mv == NULL) {
        return NULL;
    }

    Py_INCREF(md);

    _init_view(mv, md);

    return (PyObject *)mv;
}

static PyObject *
values_iter(_MultidictViewObject *self)
{
    if (self->md == NULL) {
        Py_RETURN_NONE;
    }
    return multidict_values_iter_new(PyObject_GetAttrString(self->md, "_impl"));
}

static PyTypeObject multidict_values_view_type = {
    PyVarObject_HEAD_INIT(DEFERRED_ADDRESS(&PyType_Type), 0)
    "multidict._multidict_views._valuesview",        /* tp_name */
    sizeof(_MultidictViewObject),                       /* tp_basicsize */
    0,                                               /* tp_itemsize */
    (destructor)view_dealloc,                        /* tp_dealloc */
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
    (getiterfunc)values_iter,                        /* tp_iter */
    0,                                               /* tp_iternext */
};

int
multidict_views_init()
{
    if (multidict_iter_init() < 0) {
        return -1;
    }
    
    if (PyType_Ready(&multidict_items_view_type) < 0
        // PyType_Ready(&multidict_values_views_type) < 0 ||
        // PyType_Ready(&multidict_keys_views_type) < 0
        ) {
        return -1;
    }
    return 0;
}
