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


_Py_IDENTIFIER(lower);


static PyTypeObject istr_type;


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

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|OOO:str",
                                     kwlist, &x, &encoding, &errors)) {
        return NULL;
    }
    if (x != NULL && Py_TYPE(x) == &istr_type) {
        Py_INCREF(x);
        return x;
    }
    ret = PyUnicode_Type.tp_new(type, args, kwds);
    if (!ret) {
        goto fail;
    }
    s =_PyObject_CallMethodId(ret, &PyId_lower, NULL);
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
    "multidict._multidict.istr",
    sizeof(istrobject),
    .tp_dealloc = (destructor)istr_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT
              | Py_TPFLAGS_BASETYPE
              | Py_TPFLAGS_UNICODE_SUBCLASS,
    .tp_doc = istr__doc__,
    .tp_base = DEFERRED_ADDRESS(&PyUnicode_Type),
    .tp_new = (newfunc)istr_new,
};


PyObject* istr_init(void)
{
    istr_type.tp_base = &PyUnicode_Type;
    if (PyType_Ready(&istr_type) < 0) {
        return NULL;
    }

    Py_INCREF(&istr_type);
    return (PyObject *)&istr_type;
}
