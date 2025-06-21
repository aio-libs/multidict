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


#define PyBool_As_CBool(obj) \
    PyObject_IsTrue(obj) ? true: false

#define RETURN_NULL_OR_NEWREF(ITEM) \
    PyObject* REF = ITEM; \
    return (REF != NULL) ? Py_NewRef(REF) : NULL


/* module functions */

static PyObject *
md_type(PyObject *self, PyObject *unused)
{
    mod_state *state = get_mod_state(self);
    return Py_NewRef(MultiDict_GetType(state->capi));
}

static PyObject *
md_new(PyObject *self, PyObject *arg)
{
    mod_state *state = get_mod_state(self);
    return Py_NewRef(MultiDict_New(state->capi, 0));
}

static PyObject *
md_add(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    if (nargs != 3) {
        PyErr_SetString(PyExc_TypeError,
                        "md_add should be called with md, key and value");
        return NULL;
    }
    mod_state *state = get_mod_state(self);
    if (MultiDict_Add(state->capi, args[0], args[1], args[2]) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* 
md_clear(
    PyObject* self,  PyObject *arg
){
    mod_state *state = get_mod_state(self);
    if (MultiDict_Clear(state->capi, arg) < 0){
        return NULL;
    }
    Py_RETURN_NONE;
}


static PyObject* 
md_set_default(PyObject* self, PyObject *const *args, Py_ssize_t nargs){
    if (nargs != 3) {
        PyErr_SetString(PyExc_TypeError,
                        "md_set_default should be called with md, key and value");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    RETURN_NULL_OR_NEWREF(Multidict_SetDefault(state->capi, args[0], args[1], args[2]));
}

static PyObject*
md_del(
    PyObject* self, PyObject *const *args, Py_ssize_t nargs
){
    if (nargs != 2) {
        PyErr_SetString(PyExc_TypeError,
                        "md_del should be called with md and key");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    if ((MutliDict_Del(state->capi, args[0], args[1])) < 0){
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject*
md_version(
    PyObject* self, PyObject *arg
){
    mod_state* state = get_mod_state(self);
    return PyLong_FromUnsignedLongLong(MultiDict_Version(state->capi, arg));
}

static PyObject*
md_contains(
    PyObject* self, PyObject *const *args, Py_ssize_t nargs
){
    if (nargs != 2) {
        PyErr_SetString(PyExc_TypeError,
                        "md_contains should be called with md and key");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    int ret = MultiDict_Contains(state->capi, args[0], args[1]);
    if (ret == -1){
        return NULL;
    }
    return PyBool_FromLong(ret);
}

static PyObject*
md_get(
    PyObject* self, PyObject *const *args, Py_ssize_t nargs
){
    if (nargs != 2) {
        PyErr_SetString(PyExc_TypeError,
                        "md_get should be called with md and key");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    RETURN_NULL_OR_NEWREF(MultiDict_Get(state->capi, args[0], args[1]));
}

static PyObject*
md_get_all(
    PyObject* self, PyObject *const *args, Py_ssize_t nargs
){
    if (nargs != 2) {
        PyErr_SetString(PyExc_TypeError,
                        "md_get_all should be called with md and key");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    RETURN_NULL_OR_NEWREF(MultiDict_GetAll(state->capi, args[0], args[1]));
}

static PyObject*
md_pop(
    PyObject* self, PyObject *const *args, Py_ssize_t nargs
){
    if (nargs != 2) {
        PyErr_SetString(PyExc_TypeError,
                        "md_pop should be called with md and key");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    RETURN_NULL_OR_NEWREF(MultiDict_Pop(state->capi, args[0], args[1]));
}

static PyObject*
md_popone(
    PyObject* self, PyObject *const *args, Py_ssize_t nargs
){
    if (nargs != 2){
        PyErr_SetString(PyExc_TypeError,
                        "md_popone should be called with md and key");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    RETURN_NULL_OR_NEWREF(MultiDict_PopOne(state->capi, args[0], args[1]));
}

static PyObject*
md_popall(
    PyObject* self, PyObject *const *args, Py_ssize_t nargs
){
    if (nargs != 2){
        PyErr_SetString(PyExc_TypeError,
                        "md_popone should be called with md and key");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    RETURN_NULL_OR_NEWREF(MultiDict_PopAll(state->capi, args[0], args[1]));
}

static PyObject*
md_popitem(
    PyObject* self, PyObject* arg
){
    mod_state* state = get_mod_state(self);
    RETURN_NULL_OR_NEWREF(MultiDict_PopItem(state->capi, arg));
}

static PyObject*
md_replace(
    PyObject* self, PyObject *const *args, Py_ssize_t nargs
){
    if (nargs != 3){
        PyErr_SetString(PyExc_TypeError,
                        "md_replace should be called with md, key and value");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    if (MultiDict_Replace(state->capi, args[0], args[1], args[2]) < 0){
        return NULL;   
    }
    Py_RETURN_NONE;
}

static PyObject*
md_update_from_md(
    PyObject* self, PyObject *const *args, Py_ssize_t nargs
){
    if (nargs != 3){
        PyErr_SetString(PyExc_TypeError,
                        "md_update_from_md should be called with md, other, and update");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    
    if (MultiDict_UpdateFromMultiDict(state->capi, args[0], args[1], PyBool_As_CBool(args[2])) < 0){
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject*
md_update_from_dict(
    PyObject* self, PyObject *const *args, Py_ssize_t nargs
){
    if (nargs != 3){
        PyErr_SetString(PyExc_TypeError,
                        "md_update_from_dict should be called with md, other, and update");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    
    if (MultiDict_UpdateFromDict(state->capi, args[0], args[1], PyBool_As_CBool(args[2])) < 0){
        return NULL;
    }
    Py_RETURN_NONE;
}



static PyObject* 
md_update_from_seq(
    PyObject* self, PyObject *const *args, Py_ssize_t nargs
){
    if (nargs != 3){
        PyErr_SetString(PyExc_TypeError,
                        "md_update_from_seq should be called with md, other, and update");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    if (MultiDict_UpdateFromSequence(state->capi, args[0], args[1], PyBool_As_CBool(args[2]))){
        return NULL;
    };
    Py_RETURN_NONE;
}

static PyObject*
md_equals(
    PyObject* self, PyObject *const *args, Py_ssize_t nargs 
){
    if (nargs != 2){
        PyErr_SetString(PyExc_TypeError,
                        "md_equals should be called with md and other");
        return NULL;
    }
    mod_state* state = get_mod_state(self);

    switch (MultiDict_Equals(state->capi, args[0], args[1])) {
        case -1:
            return NULL;
        case 0:
            Py_RETURN_FALSE;
        default:
            Py_RETURN_TRUE;
    }
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
    {"md_set_default", (PyCFunction)md_set_default, METH_FASTCALL},
    {"md_del", (PyCFunction)md_del, METH_FASTCALL},
    {"md_version", (PyCFunction)md_version, METH_O},
    {"md_contains", (PyCFunction)md_contains, METH_FASTCALL},
    {"md_get", (PyCFunction)md_get, METH_FASTCALL},
    {"md_get_all", (PyCFunction)md_get_all, METH_FASTCALL},
    {"md_pop", (PyCFunction)md_pop, METH_FASTCALL},
    {"md_popone", (PyCFunction)md_popone, METH_FASTCALL},
    {"md_popall", (PyCFunction)md_popall, METH_FASTCALL},
    {"md_popitem", (PyCFunction)md_popitem, METH_O},
    {"md_replace", (PyCFunction)md_replace, METH_FASTCALL},
    {"md_update_from_md", (PyCFunction)md_update_from_md, METH_FASTCALL},
    {"md_update_from_dict", (PyCFunction)md_update_from_dict, METH_FASTCALL},
    {"md_update_from_seq", (PyCFunction)md_update_from_seq, METH_FASTCALL},
    {"md_equals", (PyCFunction)md_equals, METH_FASTCALL},
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
