from cpython.object cimport PyObject
from libc.stdint cimport uint64_t

cdef extern from "_pair_list.h":

    object pair_list_new()

    int pair_list_len(object lst) except -1

    int pair_list_clear(object lst) except -1

    int pair_list_add_with_hash(object lst,
                                object identity, object key,
                                object value, Py_hash_t hash) except -1

    int _pair_list_next(object lst, Py_ssize_t *ppos,
                        PyObject* *pidentity,
                        PyObject* *pkey, PyObject* *pvalue,
                        Py_hash_t *hash) except -1

    int pair_list_next(object lst, Py_ssize_t *ppos,
                       PyObject* *pidentity,
                       PyObject* *pkey, PyObject* *pvalue) except -1

    int pair_list_contains(object lst, object identity) except -1
    # todo: add key param to raise proper exception key
    object pair_list_get_one(object lst, object identity)
    object pair_list_get_all(object lst, object identity)

    int pair_list_del(object lst, object identity) except -1
    int pair_list_del_hash(object lst, object identity, Py_hash_t hash) except -1

    object pair_list_set_default(object lst, object identity,
                                 object key, object value)

    object pair_list_pop_one(object lst, object identity)
    object pair_list_pop_all(object lst, object identity)
    object pair_list_pop_item(object lst)

    uint64_t pair_list_version(object lst)
