#ifndef _MULTIDICT_ISTR_OBJECT_H
#define _MULTIDICT_ISTR_OBJECT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>

/* Apart from istr.h so that htkeys.h, which state.h and hence istr.h
   include, can read a compact CIMultiDict entry's canonical form. */
typedef struct {
    PyUnicodeObject str;
    PyObject* canonical;
    /* canonical's hash, set with it: a compact CIMultiDict entry keeps
       none, so it is read from the key (see compact_key_hash_ci()). */
    Py_hash_t canonical_hash;
} istrobject;

static inline PyObject*
istr_canonical(PyObject* key)
{
    return ((istrobject*)key)->canonical;
}

#ifdef __cplusplus
}
#endif

#endif
