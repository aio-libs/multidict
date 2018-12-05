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

static PyObject *collections_abc_mapping;

static PyTypeObject multidict_type;
static PyTypeObject multidict_proxy_type;

#define MultiDict_Check(o) (Py_TYPE(o) == &multidict_type)

typedef struct {
    PyObject_HEAD
    PyObject *impl;
} _MultiDictObject;

/******************** Base Methods ********************/

/* Forward declaration */
static PyObject *multidict_items(_MultiDictObject *self, PyObject *args);

static INLINE PyObject *
_multidict_getone(_MultiDictObject *self, PyObject *key, PyObject *_default)
{
    PyObject *val = pair_list_get_one(self->impl, key);

    if (val == NULL &&
        PyErr_ExceptionMatches(PyExc_KeyError) &&
        _default != NULL)
    {
        PyErr_Clear();
        return _default;
    }

    return val;
}

static PyObject *
_multidict_eq(_MultiDictObject *self, _MultiDictObject *other)
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
        Py_RETURN_TRUE;
    }

    if (pair_list_len(self->impl) != pair_list_len(other->impl)) {
        Py_RETURN_FALSE;
    }

    while (_pair_list_next(self->impl, &pos1, &identity1, NULL, &value1, &h1) &&
           _pair_list_next(other->impl, &pos2, &identity2, NULL, &value2, &h2))
    {
        if ((h1 != h2) ||
            PyObject_RichCompare(identity1, identity2, Py_NE) ||
            PyObject_RichCompare(value1, value2, Py_NE))
        {
            Py_RETURN_FALSE;
        }
    }

    Py_RETURN_TRUE;
}

static INLINE int
_multidict_update_items(_MultiDictObject *self, _MultiDictObject *impl)
{
    return pair_list_update((PyObject*)self->impl, (PyObject*)impl);
}

static int
_multidict_append_items(_MultiDictObject *self, _MultiDictObject *impl)
{
    PyObject *key   = NULL,
             *value = NULL;

    Py_ssize_t pos = 0;

    while (_pair_list_next((PyObject*)impl, &pos, NULL, &key, &value, NULL)) {
        if (pair_list_add(self->impl, key, value) < 0) {
            return -1;
        }
    }

    return 0;
}

static int
_multidict_append_items_seq(_MultiDictObject *self, PyObject *arg,
                            const char *name)
{
    PyObject *key   = NULL,
             *value = NULL,
             *item  = NULL,
             *iter  = PyObject_GetIter(arg);

    if (iter == NULL) {
        return -1;
    }

    while ((item = PyIter_Next(iter)) != NULL) {
        if (PyObject_Length(item) != 2) {
            PyErr_Format(
                PyExc_TypeError,
                "%s takes either dict or list of (key, value) tuples",
                name,
                NULL
            );
            Py_DECREF(item);
            Py_DECREF(iter);
            return -1;
        }

        key   = PyTuple_GET_ITEM(item, 0);
        value = PyTuple_GET_ITEM(item, 0);
        pair_list_add(self->impl, key, value);

        Py_DECREF(item);
    }

    Py_DECREF(iter);
    return 0;
}

static int
_multidict_list_extend(PyObject *list, PyObject *target_list)
{
    PyObject *item = NULL,
             *iter = PyObject_GetIter(target_list);

    if (iter == NULL) {
        return -1;
    }

    while ((item = PyIter_Next(iter)) != NULL) {
        if (PyList_Append(list, item) < 0) {
            Py_DECREF(item);
            Py_DECREF(iter);
            return -1;
        }
        Py_DECREF(item);
    }

    Py_DECREF(iter);
    return 0;
}

static int
_multidict_extend(_MultiDictObject *self, PyObject *args, PyObject *kwds,
                  const char *name, int do_add)
{
    // TOOD: Refactoring me. Split on little functions;

    PyObject *arg        = NULL,
             *arg_items  = NULL,
             *kwds_items = NULL;

    if (PyObject_Length(args) > 1) {
        PyErr_Format(PyExc_TypeError,
                     "%s takes at most 1 positional argument (%zd given)",
                     name, PyObject_Length(args), NULL);
        goto fail;
    }

    if (args != Py_None) {
        arg = PyTuple_GetItem(args, 0);
        if (!arg) {
            goto fail;
        }
        Py_INCREF(arg);

        // TODO: fixme multidict_base_type
        if (MultiDict_Check(arg) && kwds == Py_None) {
            if (do_add) {
                // TODO: add err check
                _multidict_append_items(
                    self,
                    (_MultiDictObject*)((_MultiDictObject*)arg)->impl
                );
            } else {
                // TODO: add err check
                _multidict_update_items(
                    self,
                    (_MultiDictObject*)((_MultiDictObject*)arg)->impl
                );
            }
        } else {
            if (PyObject_HasAttrString(arg, "items")) {
                arg_items = multidict_items(self, arg);
            }

            if (kwds != Py_None) {
                kwds_items = PyDict_Items(kwds);
                _multidict_list_extend(arg_items, kwds_items);
            }

            if (do_add) {
                // TODO: add err check
                _multidict_append_items_seq(self, arg, name);
            } else {
                // TODO: add err check
                pair_list_update_from_seq(self->impl, arg);
            }
        }
    } else {
        arg = PyDict_Items(kwds);
        if (do_add) {
            // TODO: add err check
            _multidict_append_items_seq(self, arg, name);
        } else {
            pair_list_update_from_seq(self->impl, arg);
        }
    }

    Py_DECREF(arg);
    Py_DECREF(arg_items);
    Py_DECREF(kwds_items);
    return 0;

fail:
    Py_XDECREF(arg);
    Py_XDECREF(arg_items);
    Py_XDECREF(kwds_items);
    return -1;
}

static PyObject *
multidict_getall(_MultiDictObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *list     = NULL,
             *key      = NULL,
             *_default = NULL;

    static char *keywords[] = {"key", "default"};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|O:getall",
                                     keywords, &key, &_default))
    {
        return NULL;
    }

    list = pair_list_get_all(self->impl, key);

    if (list == NULL &&
        PyErr_ExceptionMatches(PyExc_KeyError) &&
        _default != NULL)
    {
        PyErr_Clear();
        return _default;
    }

    return list;
}

static PyObject *
multidict_getone(_MultiDictObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *key      = NULL,
             *_default = NULL;

    static char *keywords[] = {"key", "default"};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|O:getone",
                                     keywords, &key, &_default))
    {
        return NULL;
    }

    return _multidict_getone(self, key, _default);
}

static PyObject *
multidict_keys(_MultiDictObject *self, PyObject *args)
{
    return multidict_keysview_new((PyObject*)self);
}

static PyObject *
multidict_items(_MultiDictObject *self, PyObject *args)
{
    return multidict_itemsview_new((PyObject*)self);
}

static PyObject *
multidict_values(_MultiDictObject *self, PyObject *args)
{
    return multidict_valuesview_new((PyObject*)self);
}

static Py_ssize_t
multidict_mp_len(_MultiDictObject *self)
{
    return pair_list_len(self->impl);
}

static PyObject *
multidict_mp_subscript(_MultiDictObject *self, PyObject *key)
{
    return _multidict_getone(self, key, NULL);
}

static int
multidict_mp_as_subscript(_MultiDictObject *self, PyObject *key, PyObject *val)
{
    if (val == NULL) {
        return pair_list_del(self->impl, key);
    } else {
        return pair_list_replace(self->impl, key, val);
    }
}

static int
multidict_sq_contains(_MultiDictObject *self, PyObject *key)
{
    return pair_list_contains(self->impl, key);
}

static PyObject *
mulditict_tp_iter(_MultiDictObject *self)
{
    return PyObject_GetIter(multidict_keysview_new((PyObject*)self));
}

static PyObject *
multidict_tp_richcompare(PyObject *self, PyObject *other, int op)
{
    int cmp = 0;

    if (op != Py_EQ && op != Py_NE) {
        Py_RETURN_NOTIMPLEMENTED;
    }

    // TODO: add MultiDictProxy_Check
    if (MultiDict_Check(other)) {
        return _multidict_eq(
            (_MultiDictObject*)self,
            (_MultiDictObject*)other
        );
    }

    cmp = PyObject_IsInstance(other, (PyObject*)&collections_abc_mapping);
    if (cmp < 0) {
        return NULL;
    }

    if (cmp) {
        if (pair_list_eq_to_mapping(((_MultiDictObject*)self)->impl, other)) {
            Py_RETURN_TRUE;
        }
        Py_RETURN_FALSE;
    }

    Py_RETURN_NOTIMPLEMENTED;
}

PyDoc_STRVAR(multidict_getall_doc,
"Return a list of all values matching the key.");

PyDoc_STRVAR(multidict_getone_doc,
"Get first value matching the key.");

PyDoc_STRVAR(multidict_get_doc,
"Get first value matching the key.\n\nThe method is alias for .getone().");

PyDoc_STRVAR(multidict_keys_doc,
"Return a new view of the dictionary's keys.");

PyDoc_STRVAR(multidict_items_doc,
"Return a new view of the dictionary's items *(key, value) pairs).");

PyDoc_STRVAR(multidict_values_doc,
"Return a new view of the dictionary's values.");

/******************** MultiDict ********************/

static int
multidict_tp_init(_MultiDictObject *self, PyObject *args, PyObject *kwds)
{
    self->impl = pair_list_new();
    // TOOD: add err check
    _multidict_extend(self, args, kwds, "MultiDict", 1);
    return 0;
}

static PyObject *
multidict_add(_MultiDictObject *self, PyObject *args)
{
    PyObject *key = NULL,
             *val = NULL;

    if (!PyArg_UnpackTuple(args, "set", 2, 2, &key, &val)) {
        return NULL;
    }

    if (pair_list_add(self->impl, key, val) < 0) {
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
multidict_copy(_MultiDictObject *self)
{
    _MultiDictObject *new_multidict = NULL;

    PyObject *arg_items = NULL,
             *items     = NULL;

    new_multidict = PyObject_New(_MultiDictObject, &multidict_type);
    if (new_multidict == NULL) {
        return NULL;
    }

    items = multidict_items(self, NULL);
    if (items == NULL) {
        goto fail;
    }

    arg_items = PyTuple_New(1);
    if (arg_items == NULL) {
        goto fail;
    }

    PyTuple_SET_ITEM(arg_items, 0, items);

    if (_multidict_extend(
        new_multidict, arg_items, Py_None, "copy", 1) < 0)
    {
        goto fail;
    }

    Py_DECREF(arg_items);
    Py_DECREF(items);
    return (PyObject*)new_multidict;

fail:
    Py_XDECREF(arg_items);
    Py_XDECREF(items);
    // TODO: dealloc(new_multiidct)
    return NULL;
}

static PyObject *
multidict_extend(_MultiDictObject *self, PyObject *args, PyObject *kwds)
{
    if (_multidict_extend(self, args, kwds, "extend", 1) < 0) {
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
multidict_clear(_MultiDictObject *self)
{
    pair_list_clear(self->impl);

    Py_RETURN_NONE;
}

static PyObject *
multidict_setdefault(_MultiDictObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *key      = NULL,
             *_default = NULL;

    static char *keywords[] = {"key", "default"};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|O:setdefault",
                                     keywords, &key, &_default))
    {
        return NULL;
    }
    return pair_list_set_default(self->impl, key, _default);
}

static PyObject *
multidict_popone(_MultiDictObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *key      = NULL,
             *_default = NULL,
             *ret_val  = NULL;

    static char *keywords[] = {"key", "default"};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|O:popone",
                                     keywords, &key, &_default))
    {
        return NULL;
    }

    ret_val = pair_list_pop_one(self->impl, key);

    if (ret_val == NULL &&
        PyErr_ExceptionMatches(PyExc_KeyError) &&
        _default != NULL)
    {
        PyErr_Clear();
        return _default;
    }

    return ret_val;
}

static PyObject *
multidict_popall(_MultiDictObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *key      = NULL,
             *_default = NULL,
             *ret_val  = NULL;

    static char *keywords[] = {"key", "default"};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|O:popall",
                                     keywords, &key, &_default))
    {
        return NULL;
    }

    ret_val = pair_list_pop_all(self->impl, key);

    if (ret_val == NULL &&
        PyErr_ExceptionMatches(PyExc_KeyError) &&
        _default != NULL)
    {
        PyErr_Clear();
        return _default;
    }

    return ret_val;
}

static PyObject *
multidict_popitem(_MultiDictObject *self)
{
    return pair_list_pop_item(self->impl);
}

static PyObject *
multidict_update(_MultiDictObject *self, PyObject *args, PyObject *kwds)
{
    if (_multidict_extend(self, args, kwds, "update", 0) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

PyDoc_STRVAR(multidict_add_doc,
"Add the key and value, not overwriting any previous value.");

PyDoc_STRVAR(multidict_copy_doc,
"Return a copy of itself.");

PyDoc_STRVAR(multdicit_method_extend_doc,
"Extend current MultiDict with more values.\n\
This method must be used instead of update.");

PyDoc_STRVAR(multidict_clear_doc,
"Remove all items from MultiDict");

PyDoc_STRVAR(multidict_setdefault_doc,
"Return value for key, set value to default if key is not present.");

PyDoc_STRVAR(multidict_popone_doc,
"Remove the last occurrence of key and return the corresponding value.\n\n\
If key is not found, default is returned if given, otherwise KeyError is \
raised.\n");

PyDoc_STRVAR(multidict_popall_doc,
"Remove all occurrences of key and return the list of corresponding values.\n\n\
If key is not found, default is returned if given, otherwise KeyError is \
raised.\n");

PyDoc_STRVAR(multidict_popitem_doc,
"Remove and return an arbitrary (key, value) pair.");

PyDoc_STRVAR(multidict_update_doc,
"Update the dictionary from *other*, overwriting existing keys.");

static PySequenceMethods multidict_sequence = {
    0,                                  /* sq_length */
    0,                                  /* sq_concat */
    0,                                  /* sq_repeat */
    0,                                  /* sq_item */
    0,                                  /* sq_slice */
    0,                                  /* sq_ass_item */
    0,                                  /* sq_ass_slice */
    (objobjproc)multidict_sq_contains,  /* sq_contains */
};

static PyMappingMethods multidict_mapping = {
    (lenfunc)multidict_mp_len,                /* mp_length */
    (binaryfunc)multidict_mp_subscript,       /* mp_subscript */
    (objobjargproc)multidict_mp_as_subscript  /* mp_ass_subscript */
};

static PyMethodDef multidict_methods[] = {
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
        "add",
        (PyCFunction)multidict_add,
        METH_VARARGS,
        multidict_add_doc
    },
    {
        "copy",
        (PyCFunction)multidict_copy,
        METH_O,
        multidict_copy_doc
    },
    {
        "extend",
        (PyCFunction)multidict_extend,
        METH_VARARGS | METH_KEYWORDS,
        multdicit_method_extend_doc
    },
    {
        "clear",
        (PyCFunction)multidict_clear,
        METH_O,
        multidict_clear_doc
    },
    {
        "setdefault",
        (PyCFunction)multidict_setdefault,
        METH_VARARGS | METH_KEYWORDS,
        multidict_setdefault_doc
    },
    {
        "popone",
        (PyCFunction)multidict_popone,
        METH_VARARGS | METH_KEYWORDS,
        multidict_popone_doc
    },
    {
        "popall",
        (PyCFunction)multidict_popall,
        METH_VARARGS | METH_KEYWORDS,
        multidict_popall_doc
    },
    {
        "popitem",
        (PyCFunction)multidict_popitem,
        METH_O,
        multidict_popitem_doc
    },
    {
        "update",
        (PyCFunction)multidict_update,
        METH_VARARGS | METH_KEYWORDS,
        multidict_update_doc
    },
    {
        NULL,
        NULL
    }   /* sentinel */
};

static PyTypeObject multidict_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "multidict._multidict_c.MultiDict",              /* tp_name */
    sizeof(_MultiDictObject),                        /* tp_basicsize */
    0,                                               /* tp_itemsize */
    0,                                               /* tp_dealloc */
    0,                                               /* tp_print */
    0,                                               /* tp_getattr */
    0,                                               /* tp_setattr */
    0,                                               /* tp_reserved */
    0,                                               /* tp_repr */
    0,                                               /* tp_as_number */
    &multidict_sequence,                             /* tp_as_sequence */
    &multidict_mapping,                              /* tp_as_mapping */
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
    (richcmpfunc)multidict_tp_richcompare,           /* tp_richcompare */
    0,                                               /* tp_weaklistoffset */
    (getiterfunc)mulditict_tp_iter,                  /* tp_iter */
    0,                                               /* tp_iternext */
    multidict_methods,                               /* tp_methods */
    0,                                               /* tp_members */
    0,                                               /* tp_getset */
    0,                                               /* tp_base */
    0,                                               /* tp_dict */
    0,                                               /* tp_descr_get */
    0,                                               /* tp_descr_set */
    0,                                               /* tp_dictoffset */
    (initproc)multidict_tp_init,                     /* tp_init */
    PyType_GenericAlloc,                             /* tp_alloc */
    PyType_GenericNew,                               /* tp_new */
    PyObject_GC_Del,                                 /* tp_free */
};

/******************** CIMultiDict ********************/
// TODO: impl CIMultiDict

/******************** MultiDictProxy ********************/
// TODO: impl MultiDictProxy

/******************** CIMultiDictProxy ********************/
// TODO: impl CIMultiDictProxy

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

    if (PyType_Ready(&multidict_type) < 0) {
        return NULL;
    }

    Py_INCREF(&multidict_type);
    if (PyModule_AddObject(module, "MultiDict", (PyObject*)&multidict_type) < 0) {
        Py_DECREF(&multidict_type);
        return NULL;
    }

    return module;
}