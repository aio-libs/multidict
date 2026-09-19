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


# Passing a `capi` pointer to every single call above gets tedious fast.
# `capi()` caches it lazily -- one MultiDict_GetCAPI() call per compiled
# extension module that cimports it, not per call -- so callers can just
# write e.g. `MultiDict_New(capi(), 2)`. This has to be a plain `cdef
# extern` C function with its own `static` cache rather than a Cython-level
# `cdef inline` function sharing a module-level `cdef` variable: a `cdef`
# variable declared at the top level of a package's __init__.pxd is treated
# by Cython as belonging to (and exported by) a compiled Cython extension
# for that package, regardless of whether the function reading it is
# `inline`. Every cimporting module gets code that does the equivalent of
# `multidict.__pyx_capi__["_cached_capi"]` at its own import time to locate
# the real variable -- but multidict/__init__.py is plain Python, so that
# attribute doesn't exist, and import fails with exactly that
# AttributeError (confirmed by trying it and reading the generated C: the
# function itself compiles as a genuine static-inline function, but the
# variable it reads is generated as an indirect pointer populated through
# `PyObject_GetAttrString(module, "__pyx_capi__")` in the module's own
# import-time init code). A raw C `static` local, by contrast, is real
# per-translation-unit storage with no cross-module lookup involved.
cdef extern from *:
    """
    static inline MultiDict_CAPI *
    __Pyx_multidict_capi(void)
    {
        static MultiDict_CAPI *cached = NULL;
        if (cached == NULL) {
            cached = MultiDict_GetCAPI();
        }
        return cached;
    }
    """
    MultiDict_CAPI *capi "__Pyx_multidict_capi" () except NULL
