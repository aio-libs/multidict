#include <Python.h>
#include <structmember.h>

#include "_pair_list.h"
#include "_multidict_views.h"

// fix for VisualC complier used by Python 3.4
#ifdef __GNUC__
#define INLINE inline
#else
#define INLINE
#endif

// TODO: mb not need
// #define MultiDict_Check(o) (Py_TYPE(o) == &multidict_base_type)

#define MARKER (0)

static PyObject *collections_abc_mapping;

static PyTypeObject multidict_base_type;
static PyTypeObject multidict_type;

/******************** Base ********************/

typedef struct {
    PyObject_HEAD
    PyObject *impl;
} _MultiDictBaseObject;

PyDoc_STRVAR(multidict_getall_doc,
             "Return a list of all values matching the key.");  

static PyObject *
multidict_getall(_MultiDictBaseObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *list     = NULL,
             *key      = NULL,
             *_default = MARKER;

    static char *keywords[] = {"key", "default"};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|O:getall",
                                     keywords, &key, &_default))
    {
        return NULL;
    }   

    list = pair_list_get_all(self->impl, key);
    if (list == NULL) {
        if (PyErr_ExceptionMatches(PyExc_KeyError)) {
            if (_default != MARKER) {
                return _default;
            }
            PyErr_SetNone(PyExc_Exception);
        }
    }

    return list;
}

static INLINE PyObject *
multidict_internal_getone(_MultiDictBaseObject *self, PyObject *key,
                          PyObject *_default)
{
    PyObject *val = pair_list_get_one(self->impl, key);
    if (val == NULL) {
        if (PyErr_ExceptionMatches(PyExc_KeyError)) {
            if (_default != MARKER) {
                return _default;
            }
            PyErr_SetNone(PyExc_Exception);
        }
    }

    return val;
}

PyDoc_STRVAR(multidict_getone_doc, "Get first value matching the key.");
PyDoc_STRVAR(multidict_get_doc,
"Get first value matching the key.\n\nThe method is alias for .getone().");

static PyObject *
multidict_getone(_MultiDictBaseObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *key      = NULL,
             *_default = MARKER;

    static char *keywords[] = {"key", "default"};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|O:getone",
                                     keywords, &key, &_default))
    {
        return NULL;
    }

    return multidict_internal_getone(self, key, _default);
}

PyDoc_STRVAR(multidict_keys_doc, "Return a new view of the dictionary's keys.");

static PyObject *
multidict_keys(_MultiDictBaseObject *self, PyObject *args)
{
    return multidict_keysview_new(self);
}

PyDoc_STRVAR(multidict_items_doc,
"Return a new view of the dictionary's items *(key, value) pairs).");

static PyObject *
multidict_items(_MultiDictBaseObject *self, PyObject *args)
{
    return multidict_itemsview_new(self);
}

PyDoc_STRVAR(multidict_values_doc,
"Return a new view of the dictionary's values.");

static PyObject *
multidict_values(_MultiDictBaseObject *self, PyObject *args)
{
    return multidict_valuesview_new(self);
}

static Py_ssize_t
multidict_mp_len(_MultiDictBaseObject *self)
{
    return pair_list_len(self->impl);
}

static PyObject *
multidict_mp_getitem(_MultiDictBaseObject *self, PyObject *key)
{
    return multidict_internal_getone(self, key, MARKER);
}

static int
multidict_sq_contains(_MultiDictBaseObject *self, PyObject *key)
{
    return pair_list_contains(self->impl, key);
}

static PyObject *
mulditict_tp_iter(_MultiDictBaseObject *self)
{
    return PyObject_GetIter(multidict_keysview_new(self));
}

static int
multidict_internal_eq(_MultiDictBaseObject *self, _MultiDictBaseObject *other)
{
    Py_ssize_t pos1 = 0,
               pos2 = 0;

    Py_hash_t h1 = 0,
              h2 = 0;

    PyObject *identity1 = NULL,
             *identity2 = NULL,
             *value1    = NULL,
             *value2    = NULL;

    if (self == other) {
        return 1;
    }

    if (pair_list_len(self->impl) != pair_list_len(other->impl)) {
        return 0;
    }

    while (_pair_list_next(self->impl, &pos1, &identity1, NULL, &value1, &h1) &&
           _pair_list_next(other->impl, &pos2, &identity2, NULL, &value2, &h2))
    {
        if ((h1 != h2) ||
            PyObject_RichCompare(identity1, identity2, Py_NE) ||
            PyObject_RichCompare(value1, value2, Py_NE))
        {
            return 0;
        }
    }

    return 1;
}

static PyObject *
multidict_tp_richcompare(PyObject *self, PyObject *other, int op)
{
    int cmp = 0;

    if (op != Py_EQ && op != Py_NE) {
        Py_RETURN_NOTIMPLEMENTED;
    }

    cmp = PyObject_IsInstance(other, (PyObject*)&multidict_base_type);
    if (cmp < 0) {
        return NULL;
    }
    if (cmp) {
        if (multidict_internal_eq(self, other)) {
            Py_RETURN_TRUE;
        }
        Py_RETURN_FALSE;
    }

    cmp = PyObject_IsInstance(other, (PyObject*)&collections_abc_mapping);
    if (cmp < 0) {
        return NULL;
    }
    if (cmp) {
        if (pair_list_eq_to_mapping(((_MultiDictBaseObject*)self)->impl, other)) {
            Py_RETURN_TRUE;
        }
        Py_RETURN_FALSE;
    }

    Py_RETURN_NOTIMPLEMENTED;
}

static PySequenceMethods multidict_base_sequence = {
    0,                                  /* sq_length */
    0,                                  /* sq_concat */
    0,                                  /* sq_repeat */
    0,                                  /* sq_item */
    0,                                  /* sq_slice */
    0,                                  /* sq_ass_item */
    0,                                  /* sq_ass_slice */
    (objobjproc)multidict_sq_contains,  /* sq_contains */
};

static PyMappingMethods multidict_base_mapping = {
    (lenfunc)multidict_mp_len,          /* mp_length */
    (binaryfunc)multidict_mp_getitem    /* mp_subscript */
};

static PyMethodDef multidict_base_methods[] = {
    {
        "getall",
        (PyCFunction)multidict_getall,
        METH_VARARGS | METH_KEYWORDS,
        multidict_getall_doc
    },
    {
        "getone",
        (PyCFunction)multidict_getone,
        METH_VARARGS | METH_KEYWORDS,
        multidict_getone_doc
    },
    {
        "get",
        (PyCFunction)multidict_getone,
        METH_VARARGS | METH_KEYWORDS,
        multidict_get_doc
    },
    {
        "keys",
        (PyCFunction)multidict_keys,
        METH_NOARGS,
        multidict_keys_doc
    },
    {
        "items",
        (PyCFunction)multidict_items,
        METH_NOARGS,
        multidict_items_doc
    },
    {
        "values",
        (PyCFunction)multidict_values,
        METH_NOARGS,
        multidict_values_doc
    },
    {
        NULL,
        NULL
    }   /* sentinel */
};

static PyTypeObject multidict_base_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "multidict._multidict_c._Base",                  /* tp_name */
    sizeof(_MultiDictBaseObject),                    /* tp_basicsize */
    0,                                               /* tp_itemsize */
    0,                                               /* tp_dealloc */
    0,                                               /* tp_print */
    0,                                               /* tp_getattr */
    0,                                               /* tp_setattr */
    0,                                               /* tp_reserved */
    0,                                               /* tp_repr */
    0,                                               /* tp_as_number */
    &multidict_base_sequence,                        /* tp_as_sequence */
    &multidict_base_mapping,                         /* tp_as_mapping */
    0,                                               /* tp_hash */
    0,                                               /* tp_call */
    0,                                               /* tp_str */
    0,                                               /* tp_getattro */
    0,                                               /* tp_setattro */
    0,                                               /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    0,                                               /* tp_doc */
    0,                                               /* tp_traverse */
    0,                                               /* tp_clear */
    (richcmpfunc)multidict_tp_richcompare,           /* tp_richcompare */
    0,                                               /* tp_weaklistoffset */
    (getiterfunc)mulditict_tp_iter,                  /* tp_iter */
    0,                                               /* tp_iternext */
    multidict_base_methods,                          /* tp_methods */
    0,                                               /* tp_members */
    0,                                               /* tp_getset */
    0,                                               /* tp_base */
    0,                                               /* tp_dict */
    0,                                               /* tp_descr_get */
    0,                                               /* tp_descr_set */
    0,                                               /* tp_dictoffset */
    0,                                               /* tp_init */
    0,                                               /* tp_alloc */
    0                                                /* tp_new */
};

/******************** MultiDict ********************/

static void
multidict_internal_extend(_MultiDictBaseObject *self, PyObject *args,
                          PyObject *kwds, const char *name, int do_add)
{

}

static int
multidict_tp_init(_MultiDictBaseObject *self, PyObject *args, PyObject *kwds)
{
    self->impl = pair_list_new();
    multidict_internal_extend(self, args, kwds, "MultiDict", 1);
    return 0;
}

static PyTypeObject multidict_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "multidict._multidict_c.MultiDict",              /* tp_name */
    sizeof(_MultiDictBaseObject),                    /* tp_basicsize */
    0,                                               /* tp_itemsize */
    0,                                               /* tp_dealloc */
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
    Py_TPFLAGS_DEFAULT
        | Py_TPFLAGS_BASETYPE
        | Py_TPFLAGS_HAVE_GC
        | Py_TPFLAGS_HAVE_FINALIZE,                  /* tp_flags */
    0,                                               /* tp_doc */
    0,                                               /* tp_traverse */
    0,                                               /* tp_clear */
    0,                                               /* tp_richcompare */
    0,                                               /* tp_weaklistoffset */
    0,                                               /* tp_iter */
    0,                                               /* tp_iternext */
    0,                                               /* tp_methods */
    0,                                               /* tp_members */
    0,                                               /* tp_getset */
    &multidict_base_type,                            /* tp_base */
    0,                                               /* tp_dict */
    0,                                               /* tp_descr_get */
    0,                                               /* tp_descr_set */
    0,                                               /* tp_dictoffset */
    (initproc)multidict_tp_init,                     /* tp_init */
    0,                                               /* tp_alloc */
    (newfunc)PyType_GenericNew                       /* tp_new */
};

/******************** Module ********************/

static void
module_free(void *m)
{
    Py_CLEAR(collections_abc_mapping);
}

static int
module_init()
{
    PyObject *module  = NULL;

#define WITH_MOD(NAME)                      \
    Py_CLEAR(module);                       \
    module = PyImport_ImportModule(NAME);   \
    if (module == NULL) {                   \
        goto fail;                          \
    }

#define GET_MOD_ATTR(VAR, NAME)                 \
    VAR = PyObject_GetAttrString(module, NAME); \
    if (VAR == NULL) {                          \
        goto fail;                              \
    }

    WITH_MOD("collections.abc")
    GET_MOD_ATTR(collections_abc_mapping, "Mapping")

    Py_DECREF(module);
    return 0;

fail:
    Py_CLEAR(collections_abc_mapping);
    module_free(NULL);
    return -1;
}

static PyModuleDef multidict_module = {
    PyModuleDef_HEAD_INIT,      /* m_base */
    "multidict._multidict_c",   /* m_name */
    NULL,                       /* m_doc */
    -1,                         /* m_size */
    NULL,                       /* m_methods */
    NULL,                       /* m_slots */
    NULL,                       /* m_traverse */
    NULL,                       /* m_clear */
    (freefunc)module_free       /* m_free */
};

PyMODINIT_FUNC
PyInit__multidict_c()
{
    PyObject *module = PyState_FindModule(&multidict_module);
    if (module) {
        Py_INCREF(module);
        return module;
    }

    module = PyModule_Create(&multidict_module);
    if (module_init() < 0) {
        return NULL;
    }

    if (PyType_Ready(&multidict_base_type) < 0) {
        return NULL;
    }

    Py_INCREF(&multidict_base_type);
    if (PyModule_AddObject(module, "_Base", (PyObject*)&multidict_base_type) < 0) {
        Py_DECREF(&multidict_base_type);
        return NULL;
    }

    return module;
}