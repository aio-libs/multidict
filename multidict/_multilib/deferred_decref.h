#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_DEFERRED_DECREF_H
#define _MULTIDICT_DEFERRED_DECREF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>

#include "compiler.h"

/* Defers decref of replaced/removed entry refs until the mutation fully
 * finishes: an early decref's __del__ could suspend the critical section
 * (or release the GIL on a GIL build, see #1489), exposing a half-updated
 * entry to another thread. Storage is a list of fixed-size blocks, newest
 * first: the inline block (4 KiB on the stack) is always the last one and
 * covers any realistic call; each overflow prepends a heap block, so every
 * block past `current` is full. Self-referential: never copy after init. */
#define DEFERRED_DECREF_BLOCK 511

typedef struct _deferred_decref_block {
    PyObject* items[DEFERRED_DECREF_BLOCK];
    struct _deferred_decref_block* next;
} deferred_decref_block_t;

typedef struct _deferred_decref {
    deferred_decref_block_t* current;
    Py_ssize_t count;
    deferred_decref_block_t inline_block;
} deferred_decref_t;

/* `current == NULL` means untouched: no block is wired up yet, and
 * `count`/`inline_block` are not meaningful until the first push lazily
 * initializes them. Keeps the common "nothing to defer" call cheap to
 * just this one store. */
static inline void
deferred_decref_init(deferred_decref_t* defer)
{
    defer->current = NULL;
}

COLD static int
_deferred_decref_grow(deferred_decref_t* defer)
{
    deferred_decref_block_t* block =
        PyMem_Malloc(sizeof(deferred_decref_block_t));
    if (block == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    block->next = defer->current;
    defer->current = block;
    defer->count = 0;
    return 0;
}

/* Steals the reference: takes ownership of `obj`, to be decref'd only
   once deferred_decref_release() runs. `obj` may be NULL (a no-op).
   On the (exceedingly unlikely -- it takes many duplicate entries for
   one identity in a single call to get here) allocation failure
   growing past the inline buffer, decrefs `obj` immediately instead
   (nobody else will) and returns -1; the caller is already unwinding
   via PyErr_NoMemory() at that point, so the tiny reopened window is
   confined to an already-failing allocation, not the normal path. */
static inline int
deferred_decref_push(deferred_decref_t* defer, PyObject* obj)
{
    if (obj == NULL) {
        return 0;
    }
    if (UNLIKELY(defer->current == NULL)) {
        // first push ever: wire up the inline block now, not on init()
        defer->inline_block.next = NULL;
        defer->current = &defer->inline_block;
        defer->count = 0;
    }
    if (UNLIKELY(defer->count == DEFERRED_DECREF_BLOCK)) {
        if (_deferred_decref_grow(defer) < 0) {
            Py_DECREF(obj);
            return -1;
        }
    }
    defer->current->items[defer->count++] = obj;
    return 0;
}

/* Grows `defer` only when its current block is exactly full, so the very
 * next deferred_decref_push_reserved() call can't fail. Only ever grows
 * at that exact boundary -- never early -- so it can't strand a retired
 * block short of DEFERRED_DECREF_BLOCK items, which release() assumes
 * every non-current block has. PyMem_Malloc() itself never suspends a
 * critical section (#1469), so this is always safe to call before mutating
 * an entry; a failure here leaves `defer` untouched. Callers that must null
 * out an entry's field before pushing it (a half-deletion mid-mutation)
 * need this: pushing the normal way risks deferred_decref_push()'s OOM
 * fallback decref'ing while the entry sits half torn down, exposing it to
 * a concurrent reader -- see #1491 review. Guarantees only the next single
 * push; call again before each subsequent reserved push. */
static inline int
_deferred_decref_reserve_one(deferred_decref_t* defer)
{
    if (UNLIKELY(defer->current == NULL)) {
        defer->inline_block.next = NULL;
        defer->current = &defer->inline_block;
        defer->count = 0;
        return 0;
    }
    if (defer->count == DEFERRED_DECREF_BLOCK) {
        if (_deferred_decref_grow(defer) < 0) {
            return -1;
        }
    }
    return 0;
}

/* Steals the reference like deferred_decref_push(), but assumes capacity
 * was already reserved via _deferred_decref_reserve() -- never fails, so
 * it never needs the immediate-decref fallback. `obj` may be NULL. */
static inline void
deferred_decref_push_reserved(deferred_decref_t* defer, PyObject* obj)
{
    if (obj != NULL) {
        assert(defer->count < DEFERRED_DECREF_BLOCK);
        defer->current->items[defer->count++] = obj;
    }
}

// Untouched defer (current == NULL): nothing was ever pushed, skip the walk
static inline void
deferred_decref_release(deferred_decref_t* defer)
{
    if (defer->current == NULL) {
        return;
    }
    deferred_decref_block_t* block = defer->current;
    Py_ssize_t n = defer->count;
    while (block != NULL) {
        for (Py_ssize_t i = 0; i < n; i++) {
            Py_DECREF(block->items[i]);
        }
        deferred_decref_block_t* next = block->next;
        if (block != &defer->inline_block) {
            PyMem_Free(block);
        }
        block = next;
        n = DEFERRED_DECREF_BLOCK;
    }
}

#ifdef __cplusplus
}
#endif
#endif
