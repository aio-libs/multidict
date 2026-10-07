#ifndef _MULTIDICT_ISTR_H
#define _MULTIDICT_ISTR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "compiler.h"
#include "istr_object.h"
#include "state.h"

#define IStr_CheckExact(state, obj) Py_IS_TYPE(obj, state->IStrType)

PyDoc_STRVAR(istr__doc__, "istr class implementation");

/* The characters of an istr built by _istr_from_exact_str(), which follow
   the object in its own block. */
static inline void*
_istr_inline_data(istrobject* self)
{
    return (char*)self + sizeof(istrobject);
}

/* A buffer str's dealloc would free: PyObject_Malloc()'s up to 3.12,
   PyMem_Malloc()'s from 3.13. */
static inline void
_istr_free_buffer(void* p)
{
#if PY_VERSION_HEX < 0x030d0000
    PyObject_Free(p);
#else
    PyMem_Free(p);
#endif
}

/* What str's dealloc does for an istr, which is never interned, except
   that characters following the object go with it: only a buffer of its
   own, from unicode_subtype_new(), is freed on its own. */
static void
istr_tp_dealloc(istrobject* self)
{
    PyTypeObject* tp = Py_TYPE(self);
    Py_XDECREF(self->canonical);
    void* data = self->str.data.any;
#if PY_VERSION_HEX < 0x030c0000
    wchar_t* wstr = ((PyASCIIObject*)self)->wstr;
    if (wstr != NULL && (void*)wstr != data) {
        PyObject_Free(wstr);
    }
#endif
    char* utf8 = ((PyCompactUnicodeObject*)self)->utf8;
    if (utf8 != NULL && (void*)utf8 != data) {
        _istr_free_buffer(utf8);
    }
    if (data != NULL && data != _istr_inline_data(self)) {
        _istr_free_buffer(data);
    }
    tp->tp_free(self);
    Py_DECREF(tp);
}

/* An istr holding the same string as `str`, an exact str: what
   str.__new__(type, str) does, without the argument tuple, the argument
   parsing and the str() call around it. It mirrors CPython's
   unicode_subtype_new(), except that the characters follow the object in
   one block, where unicode_subtype_new() allocates two, and no field is
   zeroed only to be set below. istr_tp_dealloc() frees the block whole.
   The C extension is never built for PyPy.

   Checked for 3.10 to 3.15 against their unicode_subtype_new() and
   unicode_dealloc(). A newer CPython must be checked the same way, and
   the tests run under PYTHONMALLOC=debug, before the limit is raised. */
#if PY_VERSION_HEX >= 0x03100000
#error "_istr_from_exact_str() is unchecked for this CPython; see istr.h"
#endif
static PyObject*
_istr_from_exact_str(PyTypeObject* type, PyObject* str)
{
    assert(PyUnicode_CheckExact(str));
    assert(type->tp_basicsize == (Py_ssize_t)sizeof(istrobject));
#if PY_VERSION_HEX < 0x030c0000
    if (PyUnicode_READY(str) < 0) {
        return NULL;
    }
#endif
    unsigned int kind = PyUnicode_KIND(str);
    Py_ssize_t length = PyUnicode_GET_LENGTH(str);
    size_t size = (size_t)kind * ((size_t)length + 1);
    PyObject* self = PyObject_Malloc(sizeof(istrobject) + size);
    if (self == NULL) {
        return PyErr_NoMemory();
    }
    PyObject_Init(self, type);
    PyASCIIObject* ascii = (PyASCIIObject*)self;
    PyCompactUnicodeObject* compact = (PyCompactUnicodeObject*)self;
    void* data = _istr_inline_data((istrobject*)self);
    ((istrobject*)self)->canonical = NULL;
    ascii->length = length;
#ifdef Py_GIL_DISABLED
    // another thread may be caching the source's hash right now
    ascii->hash = atomic_load_ssize_relaxed(&((PyASCIIObject*)str)->hash);
#else
    ascii->hash = ((PyASCIIObject*)str)->hash;
#endif
    memset(&ascii->state, 0, sizeof(ascii->state));
    ascii->state.kind = ((PyASCIIObject*)str)->state.kind;
    ascii->state.ascii = ((PyASCIIObject*)str)->state.ascii;
#if PY_VERSION_HEX < 0x030c0000
    ascii->state.ready = 1;
    ascii->wstr = NULL;
    compact->wstr_length = 0;
#endif
    compact->utf8_length = 0;
    compact->utf8 = NULL;
    ((PyUnicodeObject*)self)->data.any = data;
    // An ASCII string is its own UTF-8, and shares the buffer as such.
    if (ascii->state.ascii) {
        compact->utf8_length = length;
        compact->utf8 = data;
    }
#if PY_VERSION_HEX < 0x030c0000
    if (kind == sizeof(wchar_t)) {
        compact->wstr_length = length;
        ascii->wstr = (wchar_t*)data;
    }
#endif
    memcpy(data, PyUnicode_DATA(str), size);
    return self;
}

/* Anything but an exact str: a str subclass goes through its own
   __str__(), as istr(x) does through str.__new__(). */
COLD static PyObject*
_istr_from_object(PyTypeObject* type, PyObject* x)
{
    PyObject* args = PyTuple_Pack(1, x);
    if (args == NULL) {
        return NULL;
    }
    PyObject* ret = PyUnicode_Type.tp_new(type, args, NULL);
    Py_DECREF(args);
    return ret;
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
    ((istrobject*)ret)->canonical_hash = PyObject_Hash(canonical);
    if (((istrobject*)ret)->canonical_hash == -1) {
        Py_DECREF(ret);
        return NULL;
    }
    return ret;
}

static PyObject*
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
istr_tp_vectorcall(PyObject* type, PyObject* const* args, size_t nargsf,
                   PyObject* kwnames)
{
    PyTypeObject* tp = (PyTypeObject*)type;
    Py_ssize_t nargs = PyVectorcall_NARGS(nargsf);

    if (UNLIKELY(nargs != 1 || kwnames != NULL)) {
        return _istr_slow_vectorcall(tp, args, nargs, kwnames);
    }
    PyObject* mod = PyType_GetModuleByDef(tp, &multidict_module);
    if (mod == NULL) {
        return NULL;
    }
    mod_state* state = get_mod_state(mod);
    /* istr(x), the only form that carries no encoding or errors argument
       and so needs nothing from str.__new__() but the value itself. */
    PyObject* x = args[0];
    if (IStr_CheckExact(state, x)) {
        return Py_NewRef(x);
    }
    PyObject* ret;
    if (PyUnicode_CheckExact(x)) {
        ret = _istr_from_exact_str(tp, x);
    } else {
        ret = _istr_from_object(tp, x);
    }
    if (ret == NULL) {
        return NULL;
    }
    return _istr_finish(state, ret);
}

static PyObject*
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
    Py_XDECREF(str);
    Py_XDECREF(args);
    return result;
}

static PyMethodDef istr_methods[] = {
    {"__reduce__", (PyCFunction)istr_reduce, METH_NOARGS, NULL},
    {NULL, NULL} /* sentinel */
};

static PyType_Slot istr_slots[] = {
    {Py_tp_dealloc, istr_tp_dealloc},
    {Py_tp_doc, (void*)istr__doc__},
    {Py_tp_methods, istr_methods},
    {Py_tp_new, istr_new},
#if PY_VERSION_HEX >= 0x030e00f0
    {Py_tp_vectorcall, istr_tp_vectorcall},
#endif
    {0, NULL},
};

static PyType_Spec istr_spec = {
    .name = "multidict._multidict.istr",
    .basicsize = sizeof(istrobject),
    .flags = (Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE |
              Py_TPFLAGS_UNICODE_SUBCLASS),
    .slots = istr_slots,
};

static PyObject*
istr_create(mod_state* state, PyObject* str, PyObject* canonical)
{
    PyObject* res;
    if (PyUnicode_CheckExact(str)) {
        res = _istr_from_exact_str(state->IStrType, str);
    } else {
        res = _istr_from_object(state->IStrType, str);
    }
    if (res == NULL) {
        return NULL;
    }
    Py_INCREF(canonical);
    ((istrobject*)res)->canonical = canonical;
    ((istrobject*)res)->canonical_hash = PyObject_Hash(canonical);
    if (((istrobject*)res)->canonical_hash == -1) {
        Py_CLEAR(res);
    }
    return res;
}

NOINLINE static int
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
    state->IStrType->tp_vectorcall = istr_tp_vectorcall;
#endif
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif
