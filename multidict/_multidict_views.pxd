from cpython.object cimport PyObject

cdef extern from "_multidict_views.h":

    object multidict_items_view_new(object impl)
    object multidict_keys_view_new(object impl)
    object multidict_values_view_new(object impl)

    int multidict_views_init() except -1
