#ifndef _MULTIDICT_C_H
#define _MULTIDICT_C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include "_pair_list.h"

typedef struct {  // 16 or 24 for GC prefix
    PyObject_HEAD  // 16
    pair_list_t pairs;
} MultiDictObject;

typedef struct {
    PyObject_HEAD
    MultiDictObject *md;
} MultiDictProxyObject;

#ifdef __cplusplus
}
#endif

#endif
