#ifndef _MULTIDICT_REFCOUNT_H
#define _MULTIDICT_REFCOUNT_H

#include <Python.h>

#include "compiler.h"

#ifdef Py_GIL_DISABLED
/* Py_NewRef() as a forced inline: GCC runs out of inlining budget on FT
   and calls CPython's _Py_NewRef() out of line from hot paths. */
ALWAYS_INLINE static inline PyObject*
_md_newref(PyObject* op)
{
    Py_INCREF(op);
    return op;
}
#define md_newref(op) _md_newref((PyObject*)(op))
#else
#define md_newref(op) Py_NewRef(op)
#endif

#endif
