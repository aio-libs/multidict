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


PyObject* istr_init(void);

#ifdef __cplusplus
}
#endif
#endif
