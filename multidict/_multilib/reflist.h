#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_REFLIST_H
#define _MULTIDICT_REFLIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <stdbool.h>

#include "compiler.h"

/* Strong refs parked under a critical section and dealt with only after
 * it ends, either by decref'ing them (reflist_clear()) or by moving them
 * into a list (reflist_to_list()). Both ways of draining exist for the
 * same reason: running arbitrary Python mid-mutation is unsafe. A decref
 * of a replaced entry can call a __del__ that suspends the critical
 * section (or releases the GIL on a GIL build, see #1489), exposing a
 * half-updated entry to another thread; PyList_New() can run a GC whose
 * finalizers mutate the table mid-walk. PyMem_Malloc() does neither.
 *
 * The first refs go into the inline array, which covers the common call;
 * past it every overflow prepends a heap block, so each block but the
 * newest is full and the inline array is the oldest storage of all. The
 * inline array is deliberately small: it sits in the caller's frame, and
 * every path that touches a block is kept out of line, so a caller that
 * never overflows pays about what a plain array costs. Blocks are much
 * bigger, since by then the call is an outlier and the allocations are
 * what to save. Self-referential: never copy after init. */
#define REFLIST_INLINE 15
#define REFLIST_BLOCK 127

typedef struct _reflist_block {
    PyObject* items[REFLIST_BLOCK];
    struct _reflist_block* next;
} reflist_block_t;

typedef struct _reflist {
    /* The storage being filled: `inline_items` until it overflows, the
       newest block's items after that, with `current` NULL until then. */
    PyObject** slots;
    Py_ssize_t count;
    Py_ssize_t capacity;
    reflist_block_t* current;
    PyObject* inline_items[REFLIST_INLINE];
} reflist_t;

static inline void
reflist_init(reflist_t* lst)
{
    lst->slots = lst->inline_items;
    lst->count = 0;
    lst->capacity = REFLIST_INLINE;
    lst->current = NULL;
}

COLD static int
_reflist_grow(reflist_t* lst)
{
    reflist_block_t* block = PyMem_Malloc(sizeof(reflist_block_t));
    if (block == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    block->next = lst->current;
    lst->current = block;
    lst->slots = block->items;
    lst->count = 0;
    lst->capacity = REFLIST_BLOCK;
    return 0;
}

/* Steals the reference: takes ownership of `obj` until the list is
   drained. `obj` may be NULL (a no-op). On the (exceedingly unlikely --
   it takes hundreds of collected refs in a single call to get here)
   allocation failure growing past the inline array, decrefs `obj`
   immediately instead (nobody else will) and returns -1; the caller is
   already unwinding via PyErr_NoMemory() at that point, so the tiny
   reopened window is confined to an already-failing allocation, not the
   normal path. */
static inline int
reflist_push(reflist_t* lst, PyObject* obj)
{
    if (obj == NULL) {
        return 0;
    }
    if (UNLIKELY(lst->count == lst->capacity)) {
        if (_reflist_grow(lst) < 0) {
            Py_DECREF(obj);
            return -1;
        }
    }
    lst->slots[lst->count++] = obj;
    return 0;
}

/* Grows `lst` only when its current storage is exactly full, so the very
 * next reflist_push_reserved() call can't fail. Only ever grows at that
 * exact boundary -- never early -- so it can't strand a block short of
 * REFLIST_BLOCK items, which draining assumes every block but the newest
 * has. PyMem_Malloc() itself never suspends a critical section (#1469),
 * so this is always safe to call before mutating an entry; a failure here
 * leaves `lst` untouched. Callers that must null out an entry's field
 * before pushing it (a half-deletion mid-mutation) need this: pushing the
 * normal way risks reflist_push()'s OOM fallback decref'ing while the
 * entry sits half torn down, exposing it to a concurrent reader -- see
 * #1491 review. Guarantees only the next single push; call again before
 * each subsequent reserved push. */
static inline int
_reflist_reserve_one(reflist_t* lst)
{
    if (lst->count == lst->capacity) {
        return _reflist_grow(lst);
    }
    return 0;
}

/* Steals the reference like reflist_push(), but assumes capacity was
 * already reserved via _reflist_reserve_one() -- never fails, so it never
 * needs the immediate-decref fallback. `obj` may be NULL. */
static inline void
reflist_push_reserved(reflist_t* lst, PyObject* obj)
{
    if (obj != NULL) {
        assert(lst->count < lst->capacity);
        lst->slots[lst->count++] = obj;
    }
}

static inline bool
reflist_empty(reflist_t* lst)
{
    return lst->count == 0 && lst->current == NULL;
}

// Every block but the newest is full, and they all follow the inline array
COLD static Py_ssize_t
_reflist_spilled_size(reflist_t* lst)
{
    Py_ssize_t size = REFLIST_INLINE + lst->count;
    for (reflist_block_t* block = lst->current->next; block != NULL;
         block = block->next) {
        size += REFLIST_BLOCK;
    }
    return size;
}

static inline Py_ssize_t
reflist_size(reflist_t* lst)
{
    if (UNLIKELY(lst->current != NULL)) {
        return _reflist_spilled_size(lst);
    }
    return lst->count;
}

COLD static void
_reflist_clear_spilled(reflist_t* lst)
{
    Py_ssize_t n = lst->count;
    for (reflist_block_t* block = lst->current; block != NULL;) {
        for (Py_ssize_t i = 0; i < n; i++) {
            Py_DECREF(block->items[i]);
        }
        reflist_block_t* next = block->next;
        PyMem_Free(block);
        block = next;
        n = REFLIST_BLOCK;
    }
    for (Py_ssize_t i = 0; i < REFLIST_INLINE; i++) {
        Py_DECREF(lst->inline_items[i]);
    }
    reflist_init(lst);
}

// Decrefs every collected ref; `lst` is empty again afterwards
static inline void
reflist_clear(reflist_t* lst)
{
    if (UNLIKELY(lst->current != NULL)) {
        _reflist_clear_spilled(lst);
        return;
    }
    for (Py_ssize_t i = 0; i < lst->count; i++) {
        Py_DECREF(lst->inline_items[i]);
    }
    lst->count = 0;
}

/* The blocks run newest first and the inline array is older than all of
   them, so the list is filled back to front. */
COLD static PyObject*
_reflist_spilled_to_list(reflist_t* lst)
{
    PyObject* ret = PyList_New(_reflist_spilled_size(lst));
    if (ret == NULL) {
        _reflist_clear_spilled(lst);
        return NULL;
    }
    Py_ssize_t i = PyList_GET_SIZE(ret);
    Py_ssize_t n = lst->count;
    for (reflist_block_t* block = lst->current; block != NULL;) {
        while (n > 0) {
            PyList_SET_ITEM(ret, --i, block->items[--n]);
        }
        reflist_block_t* next = block->next;
        PyMem_Free(block);
        block = next;
        n = REFLIST_BLOCK;
    }
    while (i > 0) {
        i--;
        PyList_SET_ITEM(ret, i, lst->inline_items[i]);
    }
    reflist_init(lst);
    return ret;
}

/* Moves every ref into a new list, in push order; `lst` is empty again
   afterwards either way. */
static inline PyObject*
reflist_to_list(reflist_t* lst)
{
    if (UNLIKELY(lst->current != NULL)) {
        return _reflist_spilled_to_list(lst);
    }
    PyObject* ret = PyList_New(lst->count);
    if (ret == NULL) {
        reflist_clear(lst);
        return NULL;
    }
    for (Py_ssize_t i = 0; i < lst->count; i++) {
        PyList_SET_ITEM(ret, i, lst->inline_items[i]);
    }
    lst->count = 0;
    return ret;
}

#ifdef __cplusplus
}
#endif
#endif
