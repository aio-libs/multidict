#include <Python.h>

#include "multidict_capi.h"

/* Exercises the public C API capsule from the test suite. Not part of the
   public API and not meant to be imported or relied on outside tests. */

typedef struct {
    MultiDict_CAPI* capi;
} mod_state;

static inline mod_state*
get_mod_state(PyObject* mod)
{
    mod_state* state = (mod_state*)PyModule_GetState(mod);
    assert(state != NULL);
    return state;
}

/* module functions */

static PyObject*
istr_type(PyObject* self, PyObject* unused)
{
    mod_state* state = get_mod_state(self);
    return (PyObject*)IStr_GetType(state->capi);
}

static PyObject*
istr_from_unicode(PyObject* self, PyObject* arg)
{
    mod_state* state = get_mod_state(self);
    return IStr_FromUnicode(state->capi, arg);
}

static PyObject*
md_getversion(PyObject* self, PyObject* arg)
{
    mod_state* state = get_mod_state(self);
    uint64_t version = MultiDict_GetVersion(state->capi, arg);
    if (version == 0 && PyErr_Occurred()) {
        return NULL;
    }
    return PyLong_FromUnsignedLongLong(version);
}

static PyObject*
md_type(PyObject* self, PyObject* unused)
{
    mod_state* state = get_mod_state(self);
    return (PyObject*)MultiDict_GetType(state->capi);
}

static PyObject*
cimd_type(PyObject* self, PyObject* unused)
{
    mod_state* state = get_mod_state(self);
    return (PyObject*)CIMultiDict_GetType(state->capi);
}

static PyObject*
mdproxy_type(PyObject* self, PyObject* unused)
{
    mod_state* state = get_mod_state(self);
    return (PyObject*)MultiDictProxy_GetType(state->capi);
}

static PyObject*
cimdproxy_type(PyObject* self, PyObject* unused)
{
    mod_state* state = get_mod_state(self);
    return (PyObject*)CIMultiDictProxy_GetType(state->capi);
}

static PyObject*
md_new(PyObject* self, PyObject* arg)
{
    mod_state* state = get_mod_state(self);
    Py_ssize_t prealloc_size = PyLong_AsSsize_t(arg);
    if (prealloc_size == -1 && PyErr_Occurred()) {
        return NULL;
    }
    return MultiDict_New(state->capi, prealloc_size);
}

static PyObject*
cimd_new(PyObject* self, PyObject* arg)
{
    mod_state* state = get_mod_state(self);
    Py_ssize_t prealloc_size = PyLong_AsSsize_t(arg);
    if (prealloc_size == -1 && PyErr_Occurred()) {
        return NULL;
    }
    return CIMultiDict_New(state->capi, prealloc_size);
}

static PyObject*
mdproxy_new(PyObject* self, PyObject* arg)
{
    mod_state* state = get_mod_state(self);
    return MultiDictProxy_New(state->capi, arg);
}

static PyObject*
cimdproxy_new(PyObject* self, PyObject* arg)
{
    mod_state* state = get_mod_state(self);
    return CIMultiDictProxy_New(state->capi, arg);
}

static PyObject*
md_size(PyObject* self, PyObject* arg)
{
    mod_state* state = get_mod_state(self);
    Py_ssize_t size = MultiDict_Size(state->capi, arg);
    if (size < 0) {
        return NULL;
    }
    return PyLong_FromSsize_t(size);
}

static PyObject*
md_contains(PyObject* self, PyObject* const* args, Py_ssize_t nargs)
{
    if (nargs != 2) {
        PyErr_SetString(PyExc_TypeError,
                        "md_contains should be called with md and key");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    int ret = MultiDict_Contains(state->capi, args[0], args[1]);
    if (ret < 0) {
        return NULL;
    }
    return PyBool_FromLong(ret);
}

/* Both elements of the returned tuple mirror PyDict_GetItemRef /
   PyDict_SetDefaultRef's `int` return plus `PyObject **result`
   design: (found, value_or_None). `result` is consumed (its
   reference, if any, is transferred into the returned tuple). */

static PyObject*
handle_result(int ret, PyObject* result)
{
    PyObject* value = result != NULL ? result : Py_NewRef(Py_None);
    PyObject* found = PyBool_FromLong(ret);
    if (found == NULL) {
        Py_DECREF(value);
        return NULL;
    }
    PyObject* tuple = PyTuple_Pack(2, found, value);
    Py_DECREF(found);
    Py_DECREF(value);
    return tuple;
}

static PyObject*
md_getitem(PyObject* self, PyObject* const* args, Py_ssize_t nargs)
{
    if (nargs != 2) {
        PyErr_SetString(PyExc_TypeError,
                        "md_getitem should be called with md and key");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    PyObject* result = NULL;
    int ret = MultiDict_GetItem(state->capi, args[0], args[1], &result);
    if (ret < 0) {
        return NULL;
    }
    return handle_result(ret, result);
}

static PyObject*
md_add(PyObject* self, PyObject* const* args, Py_ssize_t nargs)
{
    if (nargs != 3) {
        PyErr_SetString(PyExc_TypeError,
                        "md_add should be called with md, key and value");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    if (MultiDict_Add(state->capi, args[0], args[1], args[2]) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject*
md_clear(PyObject* self, PyObject* md)
{
    mod_state* state = get_mod_state(self);
    if (MultiDict_Clear(state->capi, md) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject*
md_delitem(PyObject* self, PyObject* const* args, Py_ssize_t nargs)
{
    if (nargs != 2) {
        PyErr_SetString(PyExc_TypeError,
                        "md_delitem should be called with md and key");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    if (MultiDict_DelItem(state->capi, args[0], args[1]) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject*
md_pop(PyObject* self, PyObject* const* args, Py_ssize_t nargs)
{
    if (nargs != 2) {
        PyErr_SetString(PyExc_TypeError,
                        "md_pop should be called with md and key");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    PyObject* result = NULL;
    int ret = MultiDict_Pop(state->capi, args[0], args[1], &result);
    if (ret < 0) {
        return NULL;
    }
    return handle_result(ret, result);
}

static PyObject*
md_setdefault(PyObject* self, PyObject* const* args, Py_ssize_t nargs)
{
    if (nargs != 3) {
        PyErr_SetString(
            PyExc_TypeError,
            "md_setdefault should be called with md, key and default");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    PyObject* result = NULL;
    int ret =
        MultiDict_SetDefault(state->capi, args[0], args[1], args[2], &result);
    if (ret < 0) {
        return NULL;
    }
    return handle_result(ret, result);
}

static PyObject*
md_setitem(PyObject* self, PyObject* const* args, Py_ssize_t nargs)
{
    if (nargs != 3) {
        PyErr_SetString(PyExc_TypeError,
                        "md_setitem should be called with md, key and value");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    if (MultiDict_SetItem(state->capi, args[0], args[1], args[2]) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

typedef struct {
    PyObject* list;
    Py_ssize_t limit;  // < 0 means no limit
} visit_ctx;

static int
collect_pair(void* user_data, PyObject* key, PyObject* value)
{
    visit_ctx* ctx = (visit_ctx*)user_data;
    PyObject* pair = PyTuple_Pack(2, key, value);
    if (pair == NULL) {
        return 0;
    }
    int appended = PyList_Append(ctx->list, pair) == 0;
    Py_DECREF(pair);
    if (!appended) {
        return 0;
    }
    if (ctx->limit >= 0 && PyList_GET_SIZE(ctx->list) >= ctx->limit) {
        return 0;
    }
    return 1;
}

static PyObject*
md_foreach(PyObject* self, PyObject* const* args, Py_ssize_t nargs)
{
    if (nargs != 3) {
        PyErr_SetString(PyExc_TypeError,
                        "md_foreach should be called with md, key and limit");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    PyObject* key = args[1] == Py_None ? NULL : args[1];
    Py_ssize_t limit = PyLong_AsSsize_t(args[2]);
    if (limit == -1 && PyErr_Occurred()) {
        return NULL;
    }
    visit_ctx ctx = {PyList_New(0), limit};
    if (ctx.list == NULL) {
        return NULL;
    }
    Py_ssize_t count =
        MultiDict_ForEach(state->capi, args[0], key, collect_pair, &ctx);
    if (count < 0) {
        Py_DECREF(ctx.list);
        return NULL;
    }
    return ctx.list;
}

static PyObject*
check_api_version(PyObject* self, PyObject* arg)
{
    long fake_version = PyLong_AsLong(arg);
    if (fake_version == -1 && PyErr_Occurred()) {
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    /* A stack copy so corrupting api_version can't affect the real,
       shared capsule used by every other test. */
    MultiDict_CAPI fake_capi = *state->capi;
    fake_capi.api_version = (int)fake_version;
    if (_MultiDict_CheckAPIVersion(&fake_capi) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

/* module slots */

static int
module_traverse(PyObject* mod, visitproc visit, void* arg)
{
    return 0;
}

static int
module_clear(PyObject* mod)
{
    return 0;
}

static void
module_free(void* mod)
{
    (void)module_clear((PyObject*)mod);
}

static PyMethodDef module_methods[] = {
    {"istr_type", (PyCFunction)istr_type, METH_NOARGS},
    {"istr_from_unicode", (PyCFunction)istr_from_unicode, METH_O},
    {"md_getversion", (PyCFunction)md_getversion, METH_O},
    {"md_type", (PyCFunction)md_type, METH_NOARGS},
    {"cimd_type", (PyCFunction)cimd_type, METH_NOARGS},
    {"mdproxy_type", (PyCFunction)mdproxy_type, METH_NOARGS},
    {"cimdproxy_type", (PyCFunction)cimdproxy_type, METH_NOARGS},
    {"md_new", (PyCFunction)md_new, METH_O},
    {"cimd_new", (PyCFunction)cimd_new, METH_O},
    {"mdproxy_new", (PyCFunction)mdproxy_new, METH_O},
    {"cimdproxy_new", (PyCFunction)cimdproxy_new, METH_O},
    {"md_size", (PyCFunction)md_size, METH_O},
    {"md_contains", (PyCFunction)md_contains, METH_FASTCALL},
    {"md_getitem", (PyCFunction)md_getitem, METH_FASTCALL},
    {"md_add", (PyCFunction)md_add, METH_FASTCALL},
    {"md_clear", (PyCFunction)md_clear, METH_O},
    {"md_delitem", (PyCFunction)md_delitem, METH_FASTCALL},
    {"md_pop", (PyCFunction)md_pop, METH_FASTCALL},
    {"md_setdefault", (PyCFunction)md_setdefault, METH_FASTCALL},
    {"md_setitem", (PyCFunction)md_setitem, METH_FASTCALL},
    {"md_foreach", (PyCFunction)md_foreach, METH_FASTCALL},
    {"check_api_version", (PyCFunction)check_api_version, METH_O},
    {NULL, NULL} /* sentinel */
};

static int
module_exec(PyObject* mod)
{
    mod_state* state = get_mod_state(mod);
    state->capi = MultiDict_GetCAPI();
    if (state->capi == NULL) {
        return -1;
    }
    return 0;
}

static struct PyModuleDef_Slot module_slots[] = {
    {Py_mod_exec, module_exec},
#if PY_VERSION_HEX >= 0x030c00f0
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
#endif
#if PY_VERSION_HEX >= 0x030d00f0
    {Py_mod_gil, Py_MOD_GIL_NOT_USED},
#endif
    {0, NULL},
};

static PyModuleDef testcapi_module = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "_testcapi",
    .m_size = sizeof(mod_state),
    .m_methods = module_methods,
    .m_slots = module_slots,
    .m_traverse = module_traverse,
    .m_clear = module_clear,
    .m_free = (freefunc)module_free,
};

PyMODINIT_FUNC
PyInit__testcapi(void)
{
    return PyModuleDef_Init(&testcapi_module);
}
