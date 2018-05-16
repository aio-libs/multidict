from libc.stdint import uint64_t

cdef extern from "_pair_list.h":

    object pair_list_new()

    int pair_list_len(object lst)

    int pair_list_clear(object lst)

    int pair_list_add(lobect lst, object identity, object key,
                      object value, object hash)

    int pair_list_next(object lst, Py_ssize_t *ppos,
                       PyObject* *pkey, PyObject* *pvalue)

    int pair_list_contains(object lst, object identity)
    object pair_list_get_one(object lst, object identity)
    object pair_list_get_all(object lst, object identity)

    int pair_list_del(object lst, object identity)
    int pair_list_del_hash(object lst, object identity, Py_hash_t hash)

    uint64_t pair_list_version(object lst)
