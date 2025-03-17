#ifndef _MULTIDICT_STATE_H
#define _MULTIDICT_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    PyTypeObject *IStrType;

    PyTypeObject *MultidictType;
    PyTypeObject *CIMultidictType;
    PyTypeObject *MultidictProxyType;
    PyTypeObject *CIMultidictProxyType;

    PyTypeObject *KeysViewType;
    PyTypeObject *ItemsViewType;
    PyTypeObject *ValuesViewType;

    PyTypeObject *KeysIterType;
    PyTypeObject *ItemsIterType;
    PyTypeObject *ValuesIterType;

    PyObject *collections_abc_mapping;
    PyObject *collections_abc_keys_view;
    PyObject *collections_abc_items_view;
    PyObject *collections_abc_values_view;
    PyObject *collections_abc_mut_mapping;
    PyObject *collections_abc_mut_multi_mapping;

    PyObject *str_lower;
    PyObject *multidict_repr_func;

    PyObject *viewbaseset_richcmp_func;
    PyObject *viewbaseset_and_func;
    PyObject *viewbaseset_or_func;
    PyObject *viewbaseset_sub_func;
    PyObject *viewbaseset_xor_func;

    PyObject *keysview_repr_func;
    PyObject *keysview_isdisjoint_func;
    PyObject *itemsview_isdisjoint_func;
    PyObject *itemsview_repr_func;
    PyObject *valuesview_repr_func;


} multidict_state;


static inline multidict_state *
get_multidict_state(PyObject *mod)
{
    multidict_state *state = PyModule_GetState(mod);
    assert(state != NULL);
    return state;
}

static inline multidict_state *
get_multidict_state_by_cls(PyTypeObject *cls)
{
    multidict_state *state = (multidict_state *)PyType_GetModuleState(cls);
    assert(state != NULL);
    return state;
}

#define MultiDict_CheckExact(state, o) (Py_TYPE(o) == state->multidict_type)
#define CIMultiDict_CheckExact(state, o) (Py_TYPE(o) == state->cimultidict_type)
#define MultiDictProxy_CheckExact(state, o) (Py_TYPE(o) == state->multidict_proxy_type)
#define CIMultiDictProxy_CheckExact(state, o) (Py_TYPE(o) == state->cimultidict_proxy_type)

/* Helper macro for something like isinstance(obj, Base) */
#define _MultiDict_Check(state, o)              \
    ((MultiDict_CheckExact(state, o)) ||        \
     (CIMultiDict_CheckExact(state, o)) ||      \
     (MultiDictProxy_CheckExact(state, o)) ||   \
     (CIMultiDictProxy_CheckExact(state, o)))


#ifdef __cplusplus
}
#endif
#endif
