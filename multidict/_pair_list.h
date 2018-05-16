#ifndef _PAIR_LIST_H
#define _PAIR_LIST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "Python.h"

#ifdef MULTIDICT_EXPORT
#  define MD_API_FUNC(RTYPE) __declspec(dllexport) RTYPE
#else
#  define MD_API_FUNC(RTYPE) __declspec(dllimport) RTYPE
#endif

MD_API_FUNC(PyObject*) pair_list_new(void);

MD_API_FUNC(Py_ssize_t) pair_list_len(PyObject *list);

MD_API_FUNC(int) pair_list_clear(PyObject *list);

MD_API_FUNC(int) pair_list_add_with_hash(PyObject *list, PyObject *identity,
					PyObject *key, PyObject *value, Py_hash_t hash);

MD_API_FUNC(int) _pair_list_next(PyObject *list, Py_ssize_t *ppos,
		    PyObject **pidentity,
		    PyObject **pkey, PyObject **pvalue, Py_hash_t *hash);

MD_API_FUNC(int) pair_list_next(PyObject *list, Py_ssize_t *ppos,
		   PyObject **pidentity,
		   PyObject **pkey, PyObject **pvalue);


MD_API_FUNC(int) pair_list_contains(PyObject *list, PyObject *identity);

MD_API_FUNC(PyObject*) pair_list_get_one(PyObject *list, PyObject *identity);
MD_API_FUNC(PyObject*( pair_list_get_all(PyObject *list, PyObject *identity);


MD_API_FUNC(int) pair_list_del(PyObject *list, PyObject *identity);
MD_API_FUNC(int) pair_list_del_hash(PyObject *list, PyObject *identity, Py_hash_t hash);

MD_API_FUNC(PyObject*) pair_list_set_default(PyObject *list, PyObject *ident,
				 PyObject *key, PyObject *value);

MD_API_FUNC(PyObject*) pair_list_pop_one(PyObject *list, PyObject *identity);
MD_API_FUNC(PyObject*) pair_list_pop_all(PyObject *list, PyObject *identity);
MD_API_FUNC(PyObject*) pair_list_pop_item(PyObject *list);


MD_API_FUNC(int) pair_list_replace(PyObject *op, PyObject *identity, PyObject * key,
		  PyObject *value, Py_hash_t hash);

MD_API_FUNC(uint64_t) pair_list_version(PyObject *list);

#endif
