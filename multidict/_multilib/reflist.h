#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_REFLIST_H
#define _MULTIDICT_REFLIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <string.h>

#include "compiler.h"

/* Strong refs collected under a critical section, turned into a list
   only after it ends: PyList_New() can run a GC, and its finalizers
   could mutate the table mid-walk. PyMem_Malloc() never runs Python
   code. Self-referential: never copy after init. */
#define REFLIST_INLINE 16

typedef struct _reflist {
    PyObject** items;
    Py_ssize_t size;
    Py_ssize_t capacity;
    PyObject* inline_items[REFLIST_INLINE];
} reflist_t;

static inline void
reflist_init(reflist_t* lst)
{
    lst->items = lst->inline_items;
    lst->size = 0;
    lst->capacity = REFLIST_INLINE;
}

COLD static int
_reflist_grow(reflist_t* lst)
{
    if (lst->capacity > PY_SSIZE_T_MAX / 2 / (Py_ssize_t)sizeof(PyObject*)) {
        PyErr_NoMemory();
        return -1;
    }
    Py_ssize_t capacity = lst->capacity * 2;
    PyObject** items = PyMem_Malloc((size_t)capacity * sizeof(PyObject*));
    if (items == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    memcpy(items, lst->items, (size_t)lst->size * sizeof(PyObject*));
    if (lst->items != lst->inline_items) {
        PyMem_Free(lst->items);
    }
    lst->items = items;
    lst->capacity = capacity;
    return 0;
}

/* Steals `obj`. On failure decrefs it right away, so the caller must
   still hold another reference (e.g. the table entry). */
static inline int
reflist_push(reflist_t* lst, PyObject* obj)
{
    if (UNLIKELY(lst->size == lst->capacity)) {
        if (_reflist_grow(lst) < 0) {
            Py_DECREF(obj);
            return -1;
        }
    }
    lst->items[lst->size++] = obj;
    return 0;
}

static inline void
_reflist_free(reflist_t* lst)
{
    if (lst->items != lst->inline_items) {
        PyMem_Free(lst->items);
    }
    reflist_init(lst);
}

static inline void
reflist_clear(reflist_t* lst)
{
    for (Py_ssize_t i = 0; i < lst->size; i++) {
        Py_DECREF(lst->items[i]);
    }
    _reflist_free(lst);
}

// Moves every ref into a new list; `lst` is empty afterwards either way
static inline PyObject*
reflist_to_list(reflist_t* lst)
{
    PyObject* ret = PyList_New(lst->size);
    if (ret == NULL) {
        reflist_clear(lst);
        return NULL;
    }
    for (Py_ssize_t i = 0; i < lst->size; i++) {
        PyList_SET_ITEM(ret, i, lst->items[i]);
    }
    _reflist_free(lst);
    return ret;
}

#ifdef __cplusplus
}
#endif
#endif
