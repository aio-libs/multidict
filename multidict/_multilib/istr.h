#ifndef _MULTIDICT_ISTR_H
#define _MULTIDICT_ISTR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "compiler.h"
#include "state.h"

typedef struct {
    PyUnicodeObject str;
    PyObject* canonical;
} istrobject;

#define IStr_CheckExact(state, obj) Py_IS_TYPE(obj, state->IStrType)

PyDoc_STRVAR(istr__doc__, "istr class implementation");

static inline void
istr_dealloc(istrobject* self)
{
    PyTypeObject* tp = Py_TYPE(self);
    Py_XDECREF(self->canonical);
    PyUnicode_Type.tp_dealloc((PyObject*)self);
    Py_DECREF(tp);
}

/* Build the instance once str.__new__() has produced its value. */
static inline PyObject*
_istr_finish(mod_state* state, PyObject* ret)
{
    PyObject* canonical = PyObject_CallMethodNoArgs(ret, state->str_lower);
    if (canonical == NULL) {
        Py_DECREF(ret);
        return NULL;
    }
    ((istrobject*)ret)->canonical = canonical;
    return ret;
}

static inline PyObject*
istr_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    PyObject* mod = PyType_GetModuleByDef(type, &multidict_module);
    if (mod == NULL) {
        return NULL;
    }
    mod_state* state = get_mod_state(mod);

    PyObject* x = NULL;
    static char* kwlist[] = {"object", "encoding", "errors", 0};
    PyObject* encoding = NULL;
    PyObject* errors = NULL;
    PyObject* ret = NULL;

    if (!PyArg_ParseTupleAndKeywords(
            args, kwds, "|OOO:str", kwlist, &x, &encoding, &errors)) {
        return NULL;
    }
    if (x != NULL && IStr_CheckExact(state, x)) {
        Py_INCREF(x);
        return x;
    }
    ret = PyUnicode_Type.tp_new(type, args, kwds);
    if (ret == NULL) {
        return NULL;
    }
    return _istr_finish(state, ret);
}

/* istr(x), the only form that carries no encoding or errors argument and
   so needs nothing from str.__new__() but the value itself. */
static inline PyObject*
_istr_from_object(PyTypeObject* type, mod_state* state, PyObject* x)
{
    if (IStr_CheckExact(state, x)) {
        return Py_NewRef(x);
    }
    PyObject* args = PyTuple_Pack(1, x);
    if (args == NULL) {
        return NULL;
    }
    PyObject* ret = PyUnicode_Type.tp_new(type, args, NULL);
    Py_DECREF(args);
    if (ret == NULL) {
        return NULL;
    }
    return _istr_finish(state, ret);
}

/* The encoding and errors form, and istr() itself: rebuild the tuple and
   dict that type_call() would have packed and let istr_new() parse them. */
COLD static PyObject*
_istr_slow_vectorcall(PyTypeObject* type, PyObject* const* args,
                      Py_ssize_t nargs, PyObject* kwnames)
{
    PyObject* ret = NULL;
    PyObject* kwds = NULL;
    PyObject* tpl = PyTuple_New(nargs);
    if (tpl == NULL) {
        goto done;
    }
    for (Py_ssize_t i = 0; i < nargs; i++) {
        PyTuple_SET_ITEM(tpl, i, Py_NewRef(args[i]));
    }
    if (kwnames != NULL) {
        kwds = PyDict_New();
        if (kwds == NULL) {
            goto done;
        }
        Py_ssize_t nkwargs = PyTuple_GET_SIZE(kwnames);
        for (Py_ssize_t i = 0; i < nkwargs; i++) {
            if (PyDict_SetItem(
                    kwds, PyTuple_GET_ITEM(kwnames, i), args[nargs + i]) < 0) {
                goto done;
            }
        }
    }
    ret = istr_new(type, tpl, kwds);
done:
    Py_XDECREF(kwds);
    Py_XDECREF(tpl);
    return ret;
}

static PyObject*
istr_vectorcall(PyObject* type, PyObject* const* args, size_t nargsf,
                PyObject* kwnames)
{
    PyTypeObject* tp = (PyTypeObject*)type;
    Py_ssize_t nargs = PyVectorcall_NARGS(nargsf);

    if (nargs != 1 || kwnames != NULL) {
        return _istr_slow_vectorcall(tp, args, nargs, kwnames);
    }
    PyObject* mod = PyType_GetModuleByDef(tp, &multidict_module);
    if (mod == NULL) {
        return NULL;
    }
    return _istr_from_object(tp, get_mod_state(mod), args[0]);
}

static inline PyObject*
istr_reduce(PyObject* self)
{
    PyObject* str = NULL;
    PyObject* args = NULL;
    PyObject* result = NULL;

    str = PyUnicode_FromObject(self);
    if (str == NULL) {
        goto ret;
    }
    args = PyTuple_Pack(1, str);
    if (args == NULL) {
        goto ret;
    }
    result = PyTuple_Pack(2, Py_TYPE(self), args);
ret:
    Py_CLEAR(str);
    Py_CLEAR(args);
    return result;
}

static PyMethodDef istr_methods[] = {
    {"__reduce__", (PyCFunction)istr_reduce, METH_NOARGS, NULL},
    {NULL, NULL} /* sentinel */
};

static PyType_Slot istr_slots[] = {
    {Py_tp_dealloc, istr_dealloc},
    {Py_tp_doc, (void*)istr__doc__},
    {Py_tp_methods, istr_methods},
    {Py_tp_new, istr_new},
#if PY_VERSION_HEX >= 0x030e00f0
    {Py_tp_vectorcall, istr_vectorcall},
#endif
    {0, NULL},
};

static PyType_Spec istr_spec = {
    .name = "multidict._multidict.istr",
    .basicsize = sizeof(istrobject),
    .flags = (Py_TPFLAGS_DEFAULT
#if PY_VERSION_HEX >= 0x030a00f0
              | Py_TPFLAGS_IMMUTABLETYPE
#endif
              | Py_TPFLAGS_UNICODE_SUBCLASS),
    .slots = istr_slots,
};

static inline PyObject*
IStr_New(mod_state* state, PyObject* str, PyObject* canonical)
{
    PyObject* args = NULL;
    PyObject* res = NULL;
    args = PyTuple_Pack(1, str);
    if (args == NULL) {
        goto ret;
    }
    res = PyUnicode_Type.tp_new(state->IStrType, args, NULL);
    if (!res) {
        goto ret;
    }
    Py_INCREF(canonical);
    ((istrobject*)res)->canonical = canonical;
ret:
    Py_CLEAR(args);
    return res;
}

static inline int
istr_init(PyObject* module, mod_state* state)
{
    PyObject* tpl = PyTuple_Pack(1, (PyObject*)&PyUnicode_Type);
    if (tpl == NULL) {
        return -1;
    }
    PyObject* tmp = PyType_FromModuleAndSpec(module, &istr_spec, tpl);
    Py_DECREF(tpl);
    if (tmp == NULL) {
        return -1;
    }
    state->IStrType = (PyTypeObject*)tmp;
#if PY_VERSION_HEX < 0x030e00f0
    /* 3.14+ sets this via the Py_tp_vectorcall slot instead. */
    state->IStrType->tp_vectorcall = istr_vectorcall;
#endif
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif
