#ifndef _C_PAIR_LIST_H
#define _C_PAIR_LIST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "Python.h"

typedef struct pair {
    PyObject  *identity;  // 8
    PyObject  *key;       // 8
    PyObject  *value;     // 8
    Py_hash_t  hash;      // 8 ssize_t
} pair_t;

typedef struct pair_list {
    pair_t *pairs;
    Py_ssize_t  capacity;
    Py_ssize_t  size;
    Py_ssize_t  version;
} pair_list_t;


PyObject * pair_list_new(void);

Py_ssize_t pair_list_len(PyObject *list);

void pair_list_free(PyObject *list);

int pair_list_add(PyObject *list, PyObject *identity, PyObject *key,
		  PyObject *value, Py_hash_t hash);

int pair_list_at(PyObject *list, size_t idx, pair_t *pair);


int pair_list_del(PyObject *list, PyObject *identity);
int pair_list_del_hash(PyObject *list, PyObject *identity, Py_hash_t hash);


#endif
