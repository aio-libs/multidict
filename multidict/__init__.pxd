from cpython.object cimport PyObject
from cpython.ref cimport PyTypeObject
from libc.stdint cimport uint64_t


cdef extern from "multidict_capi_struct.h":
    ctypedef struct MultiDict_CAPI:
        pass

    ctypedef int (*MultiDict_ItemVisitor)(void *user_data, PyObject *key,
                                          PyObject *value) noexcept


cdef extern from "multidict_capi.h":
    MultiDict_CAPI *MultiDict_GetCAPI() except NULL

    # istr
    PyTypeObject *IStr_GetType(MultiDict_CAPI *capi)
    bint IStr_CheckExact(MultiDict_CAPI *capi, object op)
    bint IStr_Check(MultiDict_CAPI *capi, object op)
    object IStr_FromUnicode(MultiDict_CAPI *capi, object s)

    # version
    uint64_t MultiDict_GetVersion(MultiDict_CAPI *capi, object self) except? 0

    # type objects and checks
    PyTypeObject *MultiDict_GetType(MultiDict_CAPI *capi)
    bint MultiDict_CheckExact(MultiDict_CAPI *capi, object op)
    bint MultiDict_Check(MultiDict_CAPI *capi, object op)
    PyTypeObject *CIMultiDict_GetType(MultiDict_CAPI *capi)
    bint CIMultiDict_CheckExact(MultiDict_CAPI *capi, object op)
    bint CIMultiDict_Check(MultiDict_CAPI *capi, object op)
    PyTypeObject *MultiDictProxy_GetType(MultiDict_CAPI *capi)
    bint MultiDictProxy_CheckExact(MultiDict_CAPI *capi, object op)
    bint MultiDictProxy_Check(MultiDict_CAPI *capi, object op)
    PyTypeObject *CIMultiDictProxy_GetType(MultiDict_CAPI *capi)
    bint CIMultiDictProxy_CheckExact(MultiDict_CAPI *capi, object op)
    bint CIMultiDictProxy_Check(MultiDict_CAPI *capi, object op)

    # constructors
    object MultiDict_New(MultiDict_CAPI *capi, Py_ssize_t prealloc_size)
    object CIMultiDict_New(MultiDict_CAPI *capi, Py_ssize_t prealloc_size)
    object MultiDictProxy_New(MultiDict_CAPI *capi, object arg)
    object CIMultiDictProxy_New(MultiDict_CAPI *capi, object arg)

    # getters
    Py_ssize_t MultiDict_Size(MultiDict_CAPI *capi, object self) except -1
    int MultiDict_Contains(MultiDict_CAPI *capi, object self, object key) except -1
    int MultiDict_GetItem(MultiDict_CAPI *capi, object self, object key,
                          PyObject **result) except -1

    # setters
    int MultiDict_Add(MultiDict_CAPI *capi, object self, object key, object value) except -1
    int MultiDict_Clear(MultiDict_CAPI *capi, object self) except -1
    int MultiDict_DelItem(MultiDict_CAPI *capi, object self, object key) except -1
    int MultiDict_Pop(MultiDict_CAPI *capi, object self, object key,
                      PyObject **result) except -1
    int MultiDict_SetDefault(MultiDict_CAPI *capi, object self, object key,
                             object default_value, PyObject **result) except -1
    int MultiDict_SetItem(MultiDict_CAPI *capi, object self, object key, object value) except -1

    # iteration
    Py_ssize_t MultiDict_ForEach(MultiDict_CAPI *capi, object self, PyObject *key,
                                 MultiDict_ItemVisitor visitor, void *user_data) except -1


# MultiDict_ForEach's `key` is a raw `PyObject *` where NULL means "visit
# every item" -- there's no `object` value for that which wouldn't also
# risk colliding with an actual `None` key (see docs/cyapi.rst). These two
# specializations split the two cases into their own signatures so ordinary
# callers never touch a raw pointer or a NULL sentinel.

cdef inline Py_ssize_t MultiDict_ForEachAll(MultiDict_CAPI *capi, object self,
                                            MultiDict_ItemVisitor visitor,
                                            void *user_data) except -1:
    return MultiDict_ForEach(capi, self, NULL, visitor, user_data)


cdef inline Py_ssize_t MultiDict_ForEachKey(MultiDict_CAPI *capi, object self, object key,
                                            MultiDict_ItemVisitor visitor,
                                            void *user_data) except -1:
    return MultiDict_ForEach(capi, self, <PyObject*>key, visitor, user_data)
