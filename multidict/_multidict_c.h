#ifndef _MULTIDICT_C_H
#define _MULTIDICT_C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>

typedef struct {
    PyObject_HEAD
    PyObject *impl;
} MultiDictObject;

#ifdef __cplusplus
}
#endif

#endif
