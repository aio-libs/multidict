#ifndef _PAIR_LIST_H
#define _PAIR_LIST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "Python.h"

PyObject * pair_list_new(void);

Py_ssize_t pair_list_len(PyObject *list);

int pair_list_clear(PyObject *list);

int pair_list_add(PyObject *list, PyObject *identity, PyObject *key,
		  PyObject *value, Py_hash_t hash);

// int pair_list_at(PyObject *list, size_t idx, pair_t *pair);


int pair_list_del(PyObject *list, PyObject *identity);
int pair_list_del_hash(PyObject *list, PyObject *identity, Py_hash_t hash);

uint64_t pair_list_version(PyObject *list);

#endif
