from cpython.object cimport PyObject

cdef extern from "_multidict_views.h":

    object multidict_view_items_new(object impl)
    object multidict_view_keys_new(object impl)
    object multidict_view_values_new(object impl)

    int multidict_views_init() except -1
