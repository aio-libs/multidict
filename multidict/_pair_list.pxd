from cpython.object cimport PyObject
from libc.stdint cimport uint64_t

cdef extern from "_pair_list.h":

    object pair_list_new()

    int pair_list_len(object lst)

    int pair_list_clear(object lst)

    int pair_list_add_with_hash(object lst,
                                object identity, object key,
                                object value, Py_hash_t hash)

    int pair_list_next(object lst, Py_ssize_t *ppos,
                       PyObject* *pkey, PyObject* *pvalue)

    int pair_list_contains(object lst, object identity)
    object pair_list_get_one(object lst, object identity)
    object pair_list_get_all(object lst, object identity)

    int pair_list_del(object lst, object identity)
    int pair_list_del_hash(object lst, object identity, Py_hash_t hash)

    object pair_list_set_default(object lst, object identity,
                                 object key, object value)

    uint64_t pair_list_version(object lst)
