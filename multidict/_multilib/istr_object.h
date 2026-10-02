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
} istrobject;

#ifdef __cplusplus
}
#endif

#endif
