#include <Python.h>
#include <multidict_api.h>
#include <stdbool.h>

typedef struct {
    MultiDict_CAPI *capi;
} mod_state;

static inline mod_state *
get_mod_state(PyObject *mod)
{
    mod_state *state = (mod_state *)PyModule_GetState(mod);
    assert(state != NULL);
    return state;
}

static inline MultiDict_CAPI *
get_capi(PyObject *mod)
{
    return get_mod_state(mod)->capi;
}

static int
check_nargs(const char *name, Py_ssize_t nargs, Py_ssize_t required)
{
    if (nargs != required) {
        PyErr_Format(PyExc_TypeError,
                     "%s should be called with %d arguments, got %d",
                     name,
                     required,
                     nargs);
        return -1;
    }
    return 0;
}

// Took the most repetative part and put it right here to help
// you can get rid of this comment before the pr is finished,
// Using a function was less confusing here than a macro - Vizonex

static PyObject *
handle_result(int ret, PyObject *result)
{
    if (ret < 0) {
        return NULL;
    }
    // Test if we missed
    if (ret == 0) {
        return PyTuple_Pack(2, Py_None, Py_False);
    }
    assert(result != NULL);
    PyObject *val = PyBool_FromLong(ret);
    if (val == NULL) {
        Py_CLEAR(result);
        return NULL;
    }
    return PyTuple_Pack(2, result, val);
}

/* module functions */

static PyObject *
md_type(PyObject *self, PyObject *unused)
{
    return Py_NewRef(MultiDict_GetType(get_capi(self)));
}

static PyObject *
md_new(PyObject *self, PyObject *arg)
{
    long prealloc_size = PyLong_AsLong(arg);
    if (prealloc_size < 0) {
        if (!PyErr_Occurred()) {
            PyErr_SetString(PyExc_ValueError,
                            "Negative prealloc_size is not allowed");
        }
        return NULL;
    }
    return MultiDict_New(get_capi(self), prealloc_size);
}

static PyObject *
md_add(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    if (check_nargs("md_add", nargs, 3) < 0) {
        return NULL;
    }
    if (MultiDict_Add(get_capi(self), args[0], args[1], args[2]) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
md_clear(PyObject *self, PyObject *arg)
{
    if (MultiDict_Clear(get_capi(self), arg) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
md_setdefault(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    if (check_nargs("md_setdefault", nargs, 3) < 0) {
        return NULL;
    }
    PyObject *result = NULL;
    int ret = MultiDict_SetDefault(
        get_capi(self), args[0], args[1], args[2], &result);
    return handle_result(ret, result);
}

static PyObject *
md_del(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    // handle this check first so that there's an immediate exit
    // rather than waiting for the state to be obtained
    if (check_nargs("md_del", nargs, 2) < 0) {
        return NULL;
    }
    if ((MutliDict_Del(get_capi(self), args[0], args[1])) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
md_version(PyObject *self, PyObject *arg)
{
    return PyLong_FromUnsignedLongLong(MultiDict_Version(get_capi(self), arg));
}

static PyObject *
md_contains(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    if (check_nargs("md_contains", nargs, 2) < 0) {
        return NULL;
    }
    int ret = MultiDict_Contains(get_capi(self), args[0], args[1]);
    if (ret == -1) {
        return NULL;
    }
    return PyBool_FromLong(ret);
}

static PyObject *
md_getone(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    if (check_nargs("md_getone", nargs, 2) < 0) {
        return NULL;
    }
    PyObject *result = NULL;
    int ret = MultiDict_GetOne(get_capi(self), args[0], args[1], &result);
    return handle_result(ret, result);
}

static PyObject *
md_getall(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    if (check_nargs("md_getall", nargs, 2) < 0) {
        return NULL;
    }
    PyObject *result = NULL;
    int ret = MultiDict_GetAll(get_capi(self), args[0], args[1], &result);
    return handle_result(ret, result);
}

static PyObject *
md_popone(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    if (check_nargs("md_popone", nargs, 2) < 0) {
        return NULL;
    }
    PyObject *result = NULL;
    int ret = MultiDict_PopOne(get_capi(self), args[0], args[1], &result);
    return handle_result(ret, result);
}

static PyObject *
md_popall(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    if (check_nargs("md_popall", nargs, 2) < 0) {
        return NULL;
    }
    mod_state *state = get_mod_state(self);
    PyObject *result = NULL;
    int ret = MultiDict_PopAll(get_capi(self), args[0], args[1], &result);
    return handle_result(ret, result);
}

static PyObject *
md_popitem(PyObject *self, PyObject *arg)
{
    mod_state *state = get_mod_state(self);
    PyObject *REF = MultiDict_PopItem(get_capi(self), arg);
    if (REF != NULL) {
        Py_INCREF(REF);
    }
    return REF;
}

static PyObject *
md_replace(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    if (check_nargs("md_replace", nargs, 3) < 0) {
        return NULL;
    }

    if (MultiDict_Replace(get_capi(self), args[0], args[1], args[2]) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
md_update_from_md(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    if (check_nargs("md_update_from_md", nargs, 3) < 0) {
        return NULL;
    }

    if (MultiDict_UpdateFromMultiDict(
            get_capi(self), args[0], args[1], PyLong_AsLong(args[2])) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
md_update_from_dict(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    if (check_nargs("md_update_from_dict", nargs, 3) < 0) {
        return NULL;
    }
    mod_state *state = get_mod_state(self);

    if (MultiDict_UpdateFromDict(
            get_capi(self), args[0], args[1], PyLong_AsLong(args[2])) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
md_update_from_seq(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    if (check_nargs("md_update_from_seq", nargs, 3) < 0) {
        return NULL;
    }
    if (MultiDict_UpdateFromSequence(
            get_capi(self), args[0], args[1], PyLong_AsLong(args[2]))) {
        return NULL;
    };
    Py_RETURN_NONE;
}

static PyObject *
md_proxy_new(PyObject *self, PyObject *arg)
{
    return MultiDictProxy_New(get_capi(self), arg);
}

static PyObject *
md_proxy_contains(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    if (check_nargs("md_proxy_contains", nargs, 2) < 0) {
        return NULL;
    }
    int ret = MultiDictProxy_Contains(get_capi(self), args[0], args[1]);
    if (ret == -1) {
        return NULL;
    }
    return PyBool_FromLong(ret);
}

static PyObject *
md_proxy_getall(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    if (check_nargs("md_proxy_getall", nargs, 2) < 0) {
        return NULL;
    }
    PyObject *result = NULL;
    int ret = MultiDictProxy_GetAll(get_capi(self), args[0], args[1], &result);
    return handle_result(ret, result);
}

static PyObject *
md_proxy_getone(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    if (check_nargs("md_proxy_getone", nargs, 2) < 0) {
        return NULL;
    }
    PyObject *result = NULL;
    int ret = MultiDictProxy_GetOne(get_capi(self), args[0], args[1], &result);
    return handle_result(ret, result);
}

static PyObject *
md_proxy_type(PyObject *self, PyObject *unused)
{
    return Py_NewRef(MultiDictProxy_GetType(get_capi(self)));
}

/* module slots */

static int
module_traverse(PyObject *mod, visitproc visit, void *arg)
{
    return 0;
}

static int
module_clear(PyObject *mod)
{
    return 0;
}

static void
module_free(void *mod)
{
    (void)module_clear((PyObject *)mod);
}

static PyMethodDef module_methods[] = {
    {"md_type", (PyCFunction)md_type, METH_NOARGS},
    {"md_new", (PyCFunction)md_new, METH_O},
    {"md_add", (PyCFunction)md_add, METH_FASTCALL},
    {"md_clear", (PyCFunction)md_clear, METH_O},
    {"md_setdefault", (PyCFunction)md_setdefault, METH_FASTCALL},
    {"md_del", (PyCFunction)md_del, METH_FASTCALL},
    {"md_version", (PyCFunction)md_version, METH_O},
    {"md_contains", (PyCFunction)md_contains, METH_FASTCALL},
    {"md_getone", (PyCFunction)md_getone, METH_FASTCALL},
    {"md_getall", (PyCFunction)md_getall, METH_FASTCALL},
    {"md_popone", (PyCFunction)md_popone, METH_FASTCALL},
    {"md_popall", (PyCFunction)md_popall, METH_FASTCALL},
    {"md_popitem", (PyCFunction)md_popitem, METH_O},
    {"md_replace", (PyCFunction)md_replace, METH_FASTCALL},
    {"md_update_from_md", (PyCFunction)md_update_from_md, METH_FASTCALL},
    {"md_update_from_dict", (PyCFunction)md_update_from_dict, METH_FASTCALL},
    {"md_update_from_seq", (PyCFunction)md_update_from_seq, METH_FASTCALL},
    {"md_proxy_new", (PyCFunction)md_proxy_new, METH_O},
    {"md_proxy_type", (PyCFunction)md_proxy_type, METH_NOARGS},
    {"md_proxy_contains", (PyCFunction)md_proxy_contains, METH_FASTCALL},
    {"md_proxy_getall", (PyCFunction)md_proxy_getall, METH_FASTCALL},
    {"md_proxy_getone", (PyCFunction)md_proxy_getone, METH_FASTCALL},
    {NULL, NULL} /* sentinel */
};

static int
module_exec(PyObject *mod)
{
    mod_state *state = get_mod_state(mod);
    state->capi = MultiDict_Import();
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

static PyModuleDef api_module = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "_api",
    .m_size = sizeof(mod_state),
    .m_methods = module_methods,
    .m_slots = module_slots,
    .m_traverse = module_traverse,
    .m_clear = module_clear,
    .m_free = (freefunc)module_free,
};

PyMODINIT_FUNC
PyInit__api(void)
{
    return PyModuleDef_Init(&api_module);
}
