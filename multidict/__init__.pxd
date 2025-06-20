# cython: language_level = 3, freethreading_compatible = True

from cpython.object cimport PyObject


cdef extern from "Python.h":
    void Py_INCREF(PyObject* o)
    bint Py_IS_TYPE(object, type)
    bint PyObject_TypeCheck(object, type)

 
cdef extern from "_multilib/dict.h":

    ctypedef struct MultiDictObject:
        pass

    ctypedef struct MultiDictProxyObject:
        pass
     
    ctypedef class multidict.MultiDict [object MultiDictObject, check_size ignore]:
        pass

    ctypedef class multidict.MultiDictProxy [object MultiDictProxyObject, check_size ignore]:
        cdef MultiDict md
    
    ctypedef class multidict.CIMultiDict [object MultiDictObject, check_size ignore]:
        pass

    ctypedef class multidict.CIMultiDictProxy [object MultiDictProxyObject, check_size ignore]:
        pass


cdef extern from "_multilib/istr.h":
    """
/* From multidict.__init__.pxd for _multilib/istr.h  */

/* To ensure IStr_CheckExact works as if it were a CPython function 
 * A Simple Hack was required to bypass this issue */


    """

    ctypedef struct istrobject:
        pass
    
    ctypedef class multidict.istr [object istrobject, check_size ignore]:
        cdef object canonical
        pass
    
   
    

cdef extern from "_multilib/capsule.h":

    # ==================== MultiDict Functions ====================

    int MultiDict_GetAll(MultiDict self, object key, PyObject **ret) except -1
    int MultiDict_GetOne(MultiDict self, object key, PyObject **ret) except -1
    object MultiDict_Keys(MultiDict self)
    object MultiDict_Items(MultiDict self)
    object MultiDict_Values(MultiDict self)
    int MultiDict_Add(MultiDict self, object key, object value)
    
    PyObject* MultiDict_Clear(MultiDict self) except NULL
    PyObject* MultiDict_Extend(MultiDict self, tuple args, dict kwargs) except NULL
    MultiDict MultiDict_Copy(MultiDict self)
    PyObject* MultiDict_SetDefault(MultiDict self, object key, object value) except NULL

    int MultiDict_PopOne(MultiDict self, object key, PyObject** ret) except -1
    int MultiDict_PopAll(MultiDict self, object key, PyObject** ret) except -1
    object MultiDict_PopItem(MultiDict self)
    PyObject* MultiDict_Update(MultiDict self, tuple args, dict kwds) except NULL

    # ==================== CIMultiDict Functions ====================

    int CIMultiDict_GetAll "MultiDict_GetAll" (CIMultiDict self, object key, PyObject **ret)
    int CIMultiDict_GetOne "MultiDict_GetOne" (CIMultiDict self, object key, PyObject **ret)
    object CIMultiDict_Keys "MultiDict_Keys" (CIMultiDict self)
    object CIMultiDict_Items "MultiDict_Items" (CIMultiDict self)
    object CIMultiDict_Values "MultiDict_Values" (CIMultiDict self)
    int CIMultiDict_Add "MultiDict_Add" (CIMultiDict self, object key, object value)

    PyObject* CIMultiDict_Clear "MultiDict_Clear" (CIMultiDict self) except NULL
    PyObject* CIMultiDict_Extend "MultiDict_Extend" (CIMultiDict self, tuple args, dict kwargs) except NULL
    MultiDict CIMultiDict_Copy "MultiDict_Copy" (CIMultiDict self)
    PyObject* CIMultiDict_SetDefault "MultiDict_SetDefault" (CIMultiDict self, object key, object value) except NULL

    int CIMultiDict_PopOne "MultiDict_PopOne" (CIMultiDict self, object key, PyObject** ret)
    int CIMultiDict_PopAll "MultiDict_PopAll" (CIMultiDict self, object key, PyObject** ret)
    object CIMultiDict_PopItem "MultiDict_PopItem" (CIMultiDict self)
    PyObject* CIMultiDict_Update "MultiDict_Update" (CIMultiDict self, tuple args, dict kwds) except NULL

    # ==================== MultiDictProxy Functions ====================

    int MultiDictProxy_GetAll(MultiDictProxy self, object key, PyObject **ret)  except -1
    int MultiDictProxy_GetOne(MultiDictProxy self, object key, PyObject **ret) except -1
    object MultiDictProxy_Keys(MultiDictProxy self)
    object MultiDictProxy_Values(MultiDictProxy self)
    object MultiDictProxy_Items(MultiDictProxy self)
    MultiDictProxy MultiDictProxy_Copy(MultiDictProxy self)

    # ==================== CIMultiDictProxy Functions ====================
    
    int CIMultiDictProxy_GetAll "MultiDictProxy_GetAll" (MultiDictProxy self, object key, PyObject **ret) except -1
    int CIMultiDictProxy_GetOne "MultiDictProxy_GetOne"(MultiDictProxy self, object key, PyObject **ret) except -1
    object CIMultiDictProxy_Keys "MultiDictProxy_Keys" (MultiDictProxy self)
    object CIMultiDictProxy_Values "MultiDictProxy_Values" (MultiDictProxy self)
    object CIMutliDictProxy_Items "MultiDictProxy_Items" (MultiDictProxy self)
    CIMultiDictProxy CIMultiDictProxy_Copy "MultiDictProxy_Copy"(CIMultiDictProxy self)

    # NOTE: Make sure you import this before using anything 
    int MultiDict_IMPORT() except -1
    int import_multidict "MultiDict_IMPORT" () except -1


cdef inline object MultiDict_Get(MultiDict self, object key, object default = None):
    cdef PyObject* ret
    if MultiDict_GetOne(self, key, &ret) < 0:
        return default
    Py_INCREF(ret)
    return <object>ret

# There is not currently good api to use for istr, 
# so we just have to recreate what was in istr.h
cdef inline bint IStr_CheckExact (object obj):
    return Py_IS_TYPE(obj, istr)

cdef inline bint IStr_Check (object obj):
    return IStr_CheckExact(obj) or PyObject_TypeCheck(obj, istr)

