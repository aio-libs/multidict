#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_FREELIST_H
#define _MULTIDICT_FREELIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <stdbool.h>
#include <stdint.h>

#include "compiler.h"

/* A bounded cache of raw blocks, all of one size, held in the module
   state so every object reaches it through its cached `state` pointer.

   Blocks carry nothing: a caller hands one over only once it has
   released whatever the block held, and gets one back uninitialized.
   A full pool hands the block straight back to be freed and an empty
   one hands back NULL to be allocated, so every user keeps its
   ordinary allocate/free path as the fallback.

   Per module state rather than per process: under a per-interpreter
   GIL each interpreter has its own arenas, so a block must be freed by
   the interpreter that allocated it, and a file-scope pool would let
   one free into another's.

   A pool that was never handed a capacity misses every time, so a new
   pool is usable straight out of the zeroed module state.

   Nothing is pooled on a free-threaded build. A shared pool needs an
   atomic exchange to pop and another to push, and on x86 an exchange
   is a locked operation whatever memory order it asks for, which
   measured at a flat ~41 cycles per round trip against mimalloc's
   30-52: mimalloc already gives each thread its own freelist, so the
   pool was a pessimisation below a 1456-byte block and inside the
   run-to-run spread above it. On a GIL build the same pool costs ~10
   cycles against pymalloc's 26-55 and libc malloc's 117-158. */

/* MULTIDICT_NO_FREELIST=1 at build time turns every pool into a miss.
   The ASan build needs it: a pooled block never reaches free(), so ASan can
   neither poison it nor report a use-after-free on it.

   A --with-trace-refs interpreter is off for the same reason it can't
   have freelists at all: an object shell is reused without ever being
   freed, so it is never taken off the all-objects list that reusing it
   would put it back on. */
#if defined(Py_GIL_DISABLED) || defined(MULTIDICT_NO_FREELIST) || \
    defined(Py_TRACE_REFS)
#define POOL_ENABLED 0
#else
#define POOL_ENABLED 1
#endif

#define POOL_MAX_DEPTH 32

typedef struct _pool {
#if POOL_ENABLED
    void* items[POOL_MAX_DEPTH];
    uint8_t used;
    uint8_t capacity;
#else
    char unused;
#endif
} pool_t;

#if POOL_ENABLED

/* `capacity` is clamped rather than asserted: it is a tuning knob, and
   a caller asking for more than the array holds wants "as deep as
   possible", not a crash. */
static inline void
pool_init(pool_t* pool, uint8_t capacity)
{
    pool->used = 0;
    pool->capacity =
        capacity > POOL_MAX_DEPTH ? (uint8_t)POOL_MAX_DEPTH : capacity;
}

/* A block, or NULL for the caller to allocate one. */
static inline void*
pool_pop(pool_t* pool)
{
    if (pool->used == 0) {
        return NULL;
    }
    return pool->items[--pool->used];
}

/* True once `block` is parked, false to leave the caller to free it. */
static inline bool
pool_push(pool_t* pool, void* block)
{
    if (pool->used >= pool->capacity) {
        return false;
    }
    pool->items[pool->used++] = block;
    return true;
}

/* `release` is whatever the pool's user frees a block with, since the
   two kinds pooled here do not come from the same allocator: a raw
   buffer from PyMem_Malloc(), an object shell from PyObject_GC_Del(),
   which needs the shell's type to find the start of its allocation.
   Idempotent, because the GC can run m_clear more than once. */
static inline void
pool_clear(pool_t* pool, void (*release)(void*))
{
    while (pool->used > 0) {
        release(pool->items[--pool->used]);
    }
}

#else

static inline void
pool_init(pool_t* pool, uint8_t capacity)
{
    (void)pool;
    (void)capacity;
}

static inline void*
pool_pop(pool_t* pool)
{
    (void)pool;
    return NULL;
}

static inline bool
pool_push(pool_t* pool, void* block)
{
    (void)pool;
    (void)block;
    return false;
}

static inline void
pool_clear(pool_t* pool, void (*release)(void*))
{
    (void)pool;
    (void)release;
}

#endif

#ifdef __cplusplus
}
#endif
#endif
