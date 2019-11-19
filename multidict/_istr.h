#ifndef _ISTR_H
#define _ISTR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Python.h"

typedef struct {
    PyUnicodeObject str;
    PyObject * canonical;
} istrobject;

#ifdef __cplusplus
}
#endif
#endif
