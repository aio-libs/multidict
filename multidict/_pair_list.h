#ifndef _PAIR_LIST_H
#define _PAIR_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include "Python.h"

// INTERNAL structures

typedef PyObject * (*calc_identity_func)(PyObject *key);

typedef struct pair {
    PyObject  *identity;  // 8
    PyObject  *key;       // 8
    PyObject  *value;     // 8
    Py_hash_t  hash;      // 8
} pair_t;

/* Note about the structure size
With 29 pairs the MultiDict object size is slightly less than 1KiB
(1000-1008 bytes depending on Python version,
plus extra 12 bytes for memory allocator internal structures).
As the result the max reserved size is 1020 bytes at most.

To fit into 512 bytes, the structure can contain only 13 pairs
which is too small, e.g. https://www.python.org returns 16 headers
(9 of them are caching proxy information though).

The embedded buffer intention is to fit the vast majority of possible
HTTP headers into the buffer without allocating an extra memory block.
*/

#if (PY_VERSION_HEX < 0x03080000)
#define EMBEDDED_CAPACITY 28
#else
#define EMBEDDED_CAPACITY 29
#endif

typedef struct pair_list {  // 40
    Py_ssize_t  capacity;   // 8
    Py_ssize_t  size;       // 8
    uint64_t  version;      // 8
    calc_identity_func calc_identity;  // 8
    pair_t *pairs;          // 8
    pair_t buffer[EMBEDDED_CAPACITY];
} pair_list_t;


// INTERNAL API

int pair_list_init(pair_list_t *list);
int ci_pair_list_init(pair_list_t *list);

Py_ssize_t pair_list_len(pair_list_t *list);

int pair_list_clear(pair_list_t *list);

int pair_list_add(pair_list_t *list, PyObject *key, PyObject *value);

int _pair_list_next(pair_list_t *list, Py_ssize_t *ppos,
                    PyObject **pidentity,
                    PyObject **pkey, PyObject **pvalue, Py_hash_t *hash);

int pair_list_next(pair_list_t *list, Py_ssize_t *ppos,
                   PyObject **pidentity,
                   PyObject **pkey, PyObject **pvalue);

int pair_list_contains(pair_list_t *list, PyObject *key);

PyObject* pair_list_get_one(pair_list_t *list, PyObject *key);
PyObject* pair_list_get_all(pair_list_t *list, PyObject *key);

int pair_list_del(pair_list_t *list, PyObject *key);

PyObject* pair_list_set_default(pair_list_t *list, PyObject *key, PyObject *value);

PyObject* pair_list_pop_one(pair_list_t *list, PyObject *key);
PyObject* pair_list_pop_all(pair_list_t *list, PyObject *key);
PyObject* pair_list_pop_item(pair_list_t *list);

int pair_list_replace(pair_list_t *op, PyObject *key, PyObject *value);

int pair_list_update(pair_list_t *op1, pair_list_t *op2);
int pair_list_update_from_seq(pair_list_t *op1, PyObject *op2);

int pair_list_eq_to_mapping(pair_list_t *list, PyObject *other);

uint64_t pair_list_version(pair_list_t *list);

int pair_list_traverse(pair_list_t *op, visitproc visit, void *arg);
int pair_list_clear(pair_list_t *op);
void pair_list_dealloc(pair_list_t *list);


int pair_list_global_init(PyObject * istr_type);

#ifdef __cplusplus
}
#endif
#endif
