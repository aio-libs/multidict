# cython: language_level = 3, freethreading_compatible = True

from cpython.object cimport Py_TYPE, PyObject


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

# AnyNP is short for any Non Proxy MultiDict
ctypedef fused AnyNPMultiDict:
    MultiDict
    CIMultiDict

ctypedef fused AnyMultiDictProxy:
    MultiDictProxy
    CIMultiDictProxy


cdef extern from "_multilib/istr.h":

    ctypedef struct istrobject:
        pass
    
    ctypedef class _multidict.istr [object istrobject, check_size ignore]:
        cdef object canonical
        pass
    
    

cdef extern from "_multilib/capsule.h":

    # ==================== MultiDict / CIMultiDict Functions ====================

    int MultiDict_GetAll(AnyNPMultiDict self, object key, PyObject **ret)
    int MultiDict_GetOne(AnyNPMultiDict self, object key, PyObject **ret)
    object MultiDict_Keys(AnyNPMultiDict self)
    object MultiDict_Items(AnyNPMultiDict self)
    object MultiDict_Values(AnyNPMultiDict self)
    int MultiDict_Add(AnyNPMultiDict self, object key, object value)

    PyObject* MultiDict_Clear(AnyNPMultiDict self) except NULL
    PyObject* MultiDict_Extend(AnyNPMultiDict self, tuple args, dict kwargs) except NULL
    AnyNPMultiDict MultiDict_Copy(AnyNPMultiDict self)
    PyObject* MultiDict_SetDefault(AnyNPMultiDict self, object key, object value) except NULL

    int MultiDict_PopOne(AnyNPMultiDict self, object key, PyObject** ret)
    int MultiDict_PopAll(AnyNPMultiDict self, object key, PyObject** ret)
    object MultiDict_PopItem(AnyNPMultiDict self)
    PyObject* MultiDict_Update(AnyNPMultiDict self, tuple args, dict kwds) except NULL

    # ==================== MultiDictProxy / CIMultiDictProxy Functions ====================

    int MultiDictProxy_GetAll(AnyMultiDictProxy self, object key, PyObject **ret)
    int MultiDictProxy_GetOne(AnyMultiDictProxy self, object key, PyObject **ret)
    int MultiDictProxy_Keys(AnyMultiDictProxy self)
    int MultiDictProxy_Values(AnyMultiDictProxy self)
    int MutliDictProxy_Items(AnyMultiDictProxy self)
    AnyMultiDictProxy MultiDictProxy_Copy(AnyMultiDictProxy self)


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


