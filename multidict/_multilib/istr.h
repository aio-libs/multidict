#ifndef _MULTIDICT_ISTR_H
#define _MULTIDICT_ISTR_H

#include "state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    PyUnicodeObject str;
    PyObject * canonical;
} istrobject;

PyDoc_STRVAR(istr__doc__, "istr class implementation");

static inline void
istr_dealloc(istrobject *self)
{
    Py_XDECREF(self->canonical);
    PyUnicode_Type.tp_dealloc((PyObject*)self);
}

static inline PyObject *
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
    multidict_state *state = get_multidict_state_by_cls(type);

    if (x != NULL && Py_TYPE(x) == state->IStrType) {
        Py_INCREF(x);
        return x;
    }
    ret = PyUnicode_Type.tp_new(type, args, kwds);
    if (!ret) {
        goto fail;
    }
    s = PyObject_CallMethodNoArgs(ret, state->str_lower);
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

static PyType_Slot IStrType_slots[] = {
    {Py_tp_dealloc, istr_dealloc},
    {Py_tp_doc, (void*)istr__doc__},
    {Py_tp_new, istr_new},
    {0, NULL},
};

static PyType_Spec IStrType_spec = {
    .name = "multidict._multidict.istr",
    .basicsize = sizeof(istrobject),
    .flags = (Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE |
              Py_TPFLAGS_HEAPTYPE | Py_TPFLAGS_UNICODE_SUBCLASS),
    .slots = IStrType_slots,
};

#ifdef __cplusplus
}
#endif
#endif
