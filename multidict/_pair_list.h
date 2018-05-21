#ifndef _PAIR_LIST_H
#define _PAIR_LIST_H

#ifdef __cplusplus 
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include "Python.h"

PyObject* pair_list_new(void);

Py_ssize_t pair_list_len(PyObject *list);

int pair_list_clear(PyObject *list);

int pair_list_add_with_hash(PyObject *list, PyObject *identity,
				PyObject *key, PyObject *value, Py_hash_t hash);

int _pair_list_next(PyObject *list, Py_ssize_t *ppos,
	    PyObject **pidentity,
	    PyObject **pkey, PyObject **pvalue, Py_hash_t *hash);

int pair_list_next(PyObject *list, Py_ssize_t *ppos,
	   PyObject **pidentity,
	   PyObject **pkey, PyObject **pvalue);


int pair_list_contains(PyObject *list, PyObject *identity);

PyObject* pair_list_get_one(PyObject *list, PyObject *identity, PyObject *key);
PyObject* pair_list_get_all(PyObject *list, PyObject *identity, PyObject *key);


int pair_list_del(PyObject *list, PyObject *identity, PyObject *key);
int pair_list_del_hash(PyObject *list, PyObject *identity, PyObject *key, Py_hash_t hash);

PyObject* pair_list_set_default(PyObject *list, PyObject *ident,
			 PyObject *key, PyObject *value);

PyObject* pair_list_pop_one(PyObject *list, PyObject *identity, PyObject *key);
PyObject* pair_list_pop_all(PyObject *list, PyObject *identity, PyObject *key);
PyObject* pair_list_pop_item(PyObject *list);


int pair_list_replace(PyObject *op, PyObject *identity, PyObject * key,
	  PyObject *value, Py_hash_t hash);


int pair_list_update(PyObject *op1, PyObject *op2);

uint64_t pair_list_version(PyObject *list);


int pair_list_init(void);

#ifdef __cplusplus
}
#endif
#endif
