#include "_istr.h"
#include "structmember.h"

PyDoc_STRVAR(istr__doc__, "istr class implementation");

/* We link this module statically for convenience.  If compiled as a shared
   library instead, some compilers don't allow addresses of Python objects
   defined in other libraries to be used in static initializers here.  The
   DEFERRED_ADDRESS macro is used to tag the slots where such addresses
   appear; the module init function must fill in the tagged slots at runtime.
   The argument is for documentation -- the macro ignores it.
*/
#define DEFERRED_ADDRESS(ADDR) 0


typedef struct {
    PyObject *lower;
    PyObject *emptystr;
} ModData;

static struct PyModuleDef _istrmodule;
static PyTypeObject istr_type;

static ModData *
modstate(PyObject *mod)
{
    return (ModData*)PyModule_GetState(mod);
}

static ModData *
global_state(void)
{
    return modstate(PyState_FindModule(&_istrmodule));
}

void istr_dealloc(istrobject *self)
{
    Py_XDECREF(self->canonical);
    PyUnicode_Type.tp_dealloc((PyObject*)self);
}

static PyObject *
istr_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyObject *x = NULL;
    static char *kwlist[] = {"object", "encoding", "errors", 0};
    PyObject *encoding = NULL;
    PyObject *errors = NULL;
    PyObject *s = NULL;
    PyObject * ret = NULL;

    ModData * state = global_state();

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|OOO:str",
                                     kwlist, &x, &encoding, &errors))
        return NULL;
    if (x == NULL) {
        s = state->emptystr;
        Py_INCREF(s);
    }
    else if (PyObject_IsInstance(x, (PyObject*)&istr_type)) {
        Py_INCREF(x);
        return x;
    }
    ret = PyUnicode_Type.tp_new(type, args, kwds);
    if (!ret) {
        goto fail;
    }
    s = PyObject_CallMethodObjArgs(ret, state->lower, NULL);
    if (!s) {
        goto fail;
    }
    ((istrobject*)ret)->canonical = s;
    s = NULL;  /* the reference is stollen by .canonical */
    return ret;
fail:
    Py_XDECREF(ret);
    return NULL;
}

static PyTypeObject istr_type = {
    PyVarObject_HEAD_INIT(DEFERRED_ADDRESS(&PyType_Type), 0)
    "multidict._istr.istr",
    sizeof(istrobject),
    0,
    (destructor)istr_dealloc,                   /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_reserved */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    0,                                          /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_UNICODE_SUBCLASS,
                                                /* tp_flags */
    0,                                          /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    0,                                          /* tp_methods */
    0,                                          /* tp_members */
    0,                                          /* tp_getset */
    DEFERRED_ADDRESS(&PyUnicode_Type),          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    0,                                          /* tp_init */
    0,                                          /* tp_alloc */
    (newfunc)istr_new,                          /* tp_new */
};


static int mod_clear(PyObject *m)
{
  Py_CLEAR(modstate(m)->lower);
  Py_CLEAR(modstate(m)->emptystr);
  return 0;
}


static struct PyModuleDef _istrmodule = {
    PyModuleDef_HEAD_INIT,
    "multidict._istr",
    istr__doc__,
    sizeof(ModData),
    NULL,  /* m_methods */
    NULL,  /* m_reload */
    NULL,  /* m_traverse */
    mod_clear,  /* m_clear */
    NULL   /* m_free */
};


PyObject* PyInit__istr(void)
{
    PyObject * tmp;
    PyObject *mod;

    mod = PyState_FindModule(&_istrmodule);
    if (mod) {
        Py_INCREF(mod);
        return mod;
    }

    istr_type.tp_base = &PyUnicode_Type;
    if (PyType_Ready(&istr_type) < 0) {
        return NULL;
    }

    mod = PyModule_Create(&_istrmodule);
    if (!mod) {
        return NULL;
    }
    tmp = PyUnicode_FromString("lower");
    if (!tmp) {
        goto err;
    }
    modstate(mod)->lower = tmp;
    tmp = PyUnicode_New(0, 0);
    if (!tmp) {
        goto err;
    }
    modstate(mod)->emptystr = tmp;

    Py_INCREF(&istr_type);
    if (PyModule_AddObject(mod, "istr", (PyObject *)&istr_type) < 0)
        goto err;

    return mod;
err:
    Py_DECREF(mod);
    return NULL;
}
