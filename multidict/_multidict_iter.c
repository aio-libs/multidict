#include "_pair_list.h"

#include <Python.h>

/* We link this module statically for convenience.  If compiled as a shared
   library instead, some compilers don't allow addresses of Python objects
   defined in other libraries to be used in static initializers here.  The
   DEFERRED_ADDRESS macro is used to tag the slots where such addresses
   appear; the module init function must fill in the tagged slots at runtime.
   The argument is for documentation -- the macro ignores it.
*/
#define DEFERRED_ADDRESS(ADDR) 0


typedef struct multidict_items_iter {
    PyObject_HEAD
    PyObject *impl;
    Py_ssize_t current;
    uint64_t version;
} MultidictItemsIter;


static int
multidict_items_iter_init(MultidictItemsIter *self, PyObject *impl)
{
    assert(impl != NULL);
    Py_INCREF(impl);

    self->impl = impl;
    self->current = 0;
    self->version = pair_list_version(impl);

    return 0;
}

static PyObject *
multidict_items_iter_new(PyTypeObject *type, PyObject *impl)
{
    MultidictItemsIter *self = PyObject_GC_New(MultidictItemsIter, type);
    if (self == NULL) {
        return NULL;
    }

    assert(impl != NULL);
    
    multidict_items_iter_init(self, impl);
    
    return (PyObject *)self;
}

static PyObject *
multidict_items_iter_iternext(MultidictItemsIter *self)
{
    PyObject *key = NULL;
    PyObject *value = NULL;
    PyObject *ret = NULL;

    if (self->version != pair_list_version(self->impl)) {
        PyErr_SetString(PyExc_RuntimeError, "Dictionary changed during iteration");
        return NULL;
    }

    if (!_pair_list_next(self->impl, &self->current, NULL, &key, &value, NULL)) {
        PyErr_SetNone(PyExc_StopIteration);
        return NULL;
    }

    ret = PyTuple_Pack(2, key, value);
    if (ret == NULL) {
        return NULL;
    }

    return ret;
}

static void
multidict_items_iter_dealloc(MultidictItemsIter *self)
{
    PyObject_GC_UnTrack(self);
    Py_TRASHCAN_SAFE_BEGIN(self)

    Py_XDECREF(self->impl);
        
    Py_TYPE(self)->tp_free((PyObject *)self);
    Py_TRASHCAN_SAFE_END(self)
}

static int
multidict_items_iter_clear(MultidictItemsIter *self)
{
    Py_CLEAR(self->impl);
    return 0;
}

static int
multidict_items_iter_traverse(MultidictItemsIter *self, visitproc visit, void *arg)
{
    Py_VISIT(self->impl);
    return 0;
}

static PyTypeObject multidict_items_iter_type = {
    PyVarObject_HEAD_INIT(DEFERRED_ADDRESS(&PyType_Type), 0)
    "multidict._multidict_iter._ItemsIter",
    sizeof(MultidictItemsIter),
    .tp_iter     = PyObject_SelfIter,
    .tp_iternext = (iternextfunc)multidict_items_iter_iternext,
    .tp_dealloc  = (destructor)multidict_items_iter_dealloc,
    .tp_flags    = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_traverse = (traverseproc)multidict_items_iter_traverse,
    .tp_clear    = (inquiry)multidict_items_iter_clear,
    .tp_init     = (initproc)multidict_items_iter_init,
    .tp_alloc    = PyType_GenericAlloc,
    .tp_new      = (newfunc)multidict_items_iter_new,
    .tp_free     = PyObject_GC_Del,
};

static struct PyModuleDef _multidict_iter_module = {
    PyModuleDef_HEAD_INIT,       /* m_base */
    "multidict._multidict_iter", /* m_name */
    NULL,                        /* m_doc */
    -1,                          /* m_size */
    NULL,                        /* m_methods */
    NULL,                        /* m_reload */
    NULL,                        /* m_traverse */
    NULL,                        /* m_clear */
    NULL                         /* m_free */
};

PyMODINIT_FUNC
PyInit__multidict_iter(void)
{
    PyObject *module = PyState_FindModule(&_multidict_iter_module);
    if (module) {
        Py_INCREF(module);
        return module;
    }

    module = PyModule_Create(&_multidict_iter_module);
    if (!module) {
        goto err;
    }

    if (PyType_Ready(&multidict_items_iter_type) < 0) {
        goto err;
    }

    Py_INCREF(&multidict_items_iter_type);
    if (PyModule_AddObject(module, "_ItemsIter",
                           (PyObject *)&multidict_items_iter_type) < 0) {
        Py_DECREF(&multidict_items_iter_type);
        goto err;
    }
    
    return module;

err:
    Py_DECREF(module);
    return NULL;
}
