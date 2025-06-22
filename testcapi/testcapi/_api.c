#include <Python.h>
#include <multidict_api.h>

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

/* module functions */

static PyObject *
md_type(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    mod_state *state = get_mod_state(self);
    if (check_nargs("md_type", nargs, 0) < 0) {
        return NULL;
    }
    return Py_NewRef(MultiDict_GetType(state->capi));
}

static PyObject *
md_new(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    mod_state *state = get_mod_state(self);
    if (check_nargs("md_new", nargs, 1) < 0) {
        return NULL;
    }
    long prealloc_size = PyLong_AsLong(args[0]);
    if (prealloc_size < 0) {
        if (!PyErr_Occurred()) {
            PyErr_SetString(PyExc_ValueError,
                            "Negative prealloc_size is not allowed");
        }
        return NULL;
    }
    return MultiDict_New(state->capi, prealloc_size);
}

static PyObject *
md_add(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    mod_state *state = get_mod_state(self);
    if (check_nargs("md_add", nargs, 3) < 0) {
        return NULL;
    }
    if (MultiDict_Add(state->capi, args[0], args[1], args[2]) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
md_clear(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    mod_state *state = get_mod_state(self);
    if (check_nargs("md_clear", nargs, 1) < 0) {
        return NULL;
    }
    if (MultiDict_Clear(state->capi, args[0]) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
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
    {"md_type", (PyCFunction)md_type, METH_FASTCALL},
    {"md_new", (PyCFunction)md_new, METH_FASTCALL},
    {"md_add", (PyCFunction)md_add, METH_FASTCALL},
    {"md_clear", (PyCFunction)md_clear, METH_FASTCALL},
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
