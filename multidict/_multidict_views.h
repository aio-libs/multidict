#ifndef _MULTIDICT_VIEWS_H
#define _MULTIDICT_VIEWS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Python.h"

PyObject *multidict_items_view_new(PyObject *impl);
PyObject *multidict_keys_view_new(PyObject *impl);
PyObject *multidict_values_view_new(PyObject *impl);

int multidict_views_init();

#ifdef __cplusplus
}
#endif

#endif
