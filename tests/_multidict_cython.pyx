# cython: language_level = 3
# setuptools: include_dirs = MULTIDICT_HEADER_PATH

from multidict cimport *
# Always remember to import_multidict or your script WILL FAIL
import_multidict()
 
# NOTE: will use this check if the _multidict c module remained the same
Cython_MultiDict = MultiDict
Cython_MultiDictProxy = MultiDictProxy
Cython_CIMultiDict = CIMultiDict
Cython_CIMultiDictProxy = CIMultiDictProxy 




def multidict_create():
    cdef MultiDict md = MultiDict()
    return md

def cimultidict_create():
    cdef CIMultiDict md = CIMultiDict()
    return md

def multidictproxy_create(object inner):
    cdef MultiDictProxy md = MultiDictProxy(inner) # type: ignore
    return md

def cimultidictproxy_create(object inner):
    cdef CIMultiDictProxy md = CIMultiDictProxy(inner) # type: ignore
    return md

def multidict_add(MultiDict md):
    MultiDict_Add(md, "a", 1)
    MultiDict_Add(md, "b", 2)

def multidict_update(MultiDict md, *args, **kwargs):
    MultiDict_Update(md, args, kwargs)

def multidict_copy(MultiDict md):
    return MultiDict_Copy(md)

def multidict_popitem(MultiDict md):
    return MultiDict_PopItem(md)

def multidict_get(MultiDict md, str key):
    return <object>MultiDict_Get(md, key)


def istr_FromUnicode(str data):
    return istr(data) # type: ignore

def istr_check(object data):
    return IStr_Check(data)

def istr_checkexact(object data):
    return IStr_CheckExact(data)


