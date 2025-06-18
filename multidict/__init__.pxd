# cython: language_level = 3

from cpython.object cimport PyObject, Py_TYPE

cdef extern from "Python.h":
    """
/* Needed in order to ignore kwargs for a faster istr.__new__ */
#define PyType_GenericNew_NoKwargs(type, args) PyType_GenericNew(type, args, NULL)
    """
    void Py_INCREF(PyObject* o)
    int Py_IS_TYPE(object, type)
    object PyType_GenericNew_NoKwargs(type, tuple args)

 
cdef extern from "_multilib/dict.h":

    ctypedef struct MultiDictObject:
        pass

    ctypedef struct MultiDictProxyObject:
        pass
     
    ctypedef class _multidict.MultiDict [object MultiDictObject, check_size ignore]:
        pass

    ctypedef class _multidict.MultiDictProxy [object MultiDictProxyObject, check_size ignore]:
        cdef MultiDict md
    
    ctypedef class _multidict.CIMultiDict [object MultiDictObject, check_size ignore]:
        pass

    ctypedef class _multidict.CIMultiDictProxy [object MultiDictProxyObject, check_size ignore]:
        pass


cdef extern from "_multilib/istr.h":

    ctypedef struct istrobject:
        pass
    
    ctypedef class _multidict.istr [object istrobject, check_size ignore]:
        cdef object canonical
        pass
    
    

cdef extern from "_multilib/capsule.h":

    int MultiDict_GetAll(MultiDict self, object key, PyObject **ret)
    int MultiDict_GetOne(MultiDict self, object key, PyObject **ret)
    object MultiDict_Keys(MultiDict self)
    object MultiDict_Items(MultiDict self)
    object MultiDict_Values(MultiDict self)
    int MultiDict_Add(MultiDict self, object key, object value)

    PyObject* MultiDict_Clear(MultiDict self) except NULL
    PyObject* MultiDict_Extend(MultiDict self, tuple args, dict kwargs) except NULL
    PyObject* MultiDict_Copy(MultiDict self) except NULL
    PyObject* MultiDict_SetDefault(MultiDict self, object key, object value) except NULL

    int MultiDict_PopOne(MultiDict self, object key, PyObject** ret)
    int MultiDict_PopAll(MultiDict self, object key, PyObject** ret)
    object MultiDict_PopItem(MultiDict self)
    PyObject* MultiDict_Update(MultiDict self, tuple args, dict kwds) except NULL


    int CIMultiDict_GetAll(CIMultiDict self, object key, PyObject **ret)
    int CIMultiDict_GetOne(CIMultiDict self, object key, PyObject **ret)
    object CIMultiDict_Keys(CIMultiDict self)
    object CIMultiDict_Items(CIMultiDict self)
    object CIMultiDict_Values(CIMultiDict self)
    int CIMultiDict_Add(CIMultiDict self, object key, object value)

    PyObject* CIMultiDict_Clear(CIMultiDict self) except NULL
    PyObject* CIMultiDict_Extend(CIMultiDict self, tuple args, dict kwargs) except NULL
    PyObject* CIMultiDict_Copy(CIMultiDict self) except NULL
    PyObject* CIMultiDict_SetDefault(CIMultiDict self, object key, object value) except NULL

    int CIMultiDict_PopOne(CIMultiDict self, object key, PyObject** ret)
    int CIMultiDict_PopAll(CIMultiDict self, object key, PyObject** ret)
    object CIMultiDict_PopItem(CIMultiDict self)
    PyObject* CIMultiDict_Update(CIMultiDict self, tuple args, dict kwds) except NULL


    void MultiDict_IMPORT()

# NOTE: Make sure you import this before using anything 

cdef inline void import_multidict() noexcept:
    MultiDict_IMPORT


cdef inline object MultiDict_Get(MultiDict self, object key, PyObject* default):
    cdef PyObject* ret
    if MultiDict_GetOne(self, key, &ret) < 0:
        return <object>default if default != NULL else None
    Py_INCREF(ret)
    return <object>ret

# Initializes istr from another unicode object
cdef inline istr IStr_FromUnicode(str obj):
    return <istr>PyType_GenericNew_NoKwargs(istr, (obj,))


