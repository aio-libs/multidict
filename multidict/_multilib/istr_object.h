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
    /* canonical's hash, set with it: a lock-free reader of a compact
       CIMultiDict gets it from the key in one load (see load_hash()). */
    Py_hash_t canonical_hash;
} istrobject;

#ifdef __cplusplus
}
#endif

#endif
