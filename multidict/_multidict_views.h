#ifndef _MULTIDICT_VIEWS_H
#define _MULTIDICT_VIEWS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Python.h"

PyObject *multidict_view_items_new(PyObject *impl);
PyObject *multidict_view_keys_new(PyObject *impl);
PyObject *multidict_view_values_new(PyObject *impl);

int multidict_views_init();

#ifdef __cplusplus
}
#endif

#endif
