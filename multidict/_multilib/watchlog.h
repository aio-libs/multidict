#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_WATCHLOG_H
#define _MULTIDICT_WATCHLOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <stdbool.h>
#include <stdint.h>

#include "../multidict_capi_struct.h"
#include "compiler.h"

/* An ordered log of watcher events, recorded under a critical section and
 * delivered only after it ends. Same reasoning as reflist_t: running
 * arbitrary Python mid-mutation is unsafe, and a watcher callback is
 * arbitrary Python by definition. PyMem_Malloc() is all recording does.
 *
 * Unlike reflist_t this is heap-only and append-ordered. Heap-only because
 * a watched multidict is the rare case and an inline array would sit in
 * every mutating caller's frame, where stack size alone moves the inliner.
 * Append-ordered because delivery order is the whole point: reflist_t
 * prepends blocks, which is fine when only the set of refs matters. */
#define WATCHLOG_BLOCK 63

typedef struct {
    PyObject* identity;
    PyObject* key;
    PyObject* value;
    PyObject* old_value;
    Py_hash_t hash;
    uint8_t event;
} watch_record_t;

typedef struct _watchlog_block {
    struct _watchlog_block* next;  // older -> newer
    Py_ssize_t count;
    watch_record_t items[WATCHLOG_BLOCK];
} watchlog_block_t;

typedef struct {
    watchlog_block_t* head;  // oldest; NULL when empty
    watchlog_block_t* tail;  // newest, the one being filled
    bool overflowed;
} watchlog_t;

static inline void
watchlog_init(watchlog_t* log)
{
    log->head = NULL;
    log->tail = NULL;
    log->overflowed = false;
}

static inline bool
watchlog_empty(const watchlog_t* log)
{
    return log->head == NULL && !log->overflowed;
}

static inline bool
_watchlog_grow(watchlog_t* log)
{
    watchlog_block_t* block = PyMem_Malloc(sizeof(watchlog_block_t));
    if (block == NULL) {
        return false;
    }
    block->next = NULL;
    block->count = 0;
    if (log->tail == NULL) {
        log->head = block;
    } else {
        log->tail->next = block;
    }
    log->tail = block;
    return true;
}

/* Records one event, taking its own reference to each non-NULL object.
 * Cannot fail the caller: the mutation it describes has already happened
 * and cannot be rolled back, so an allocation failure just sets
 * `overflowed` and the flush reports a single MultiDict_EVENT_LOST in
 * place of everything recorded. Mirrors update_marks_t's `lost` flag. */
static inline void
watchlog_append(watchlog_t* log, MultiDict_WatchEvent event,
                PyObject* identity, Py_hash_t hash, PyObject* key,
                PyObject* value, PyObject* old_value)
{
    if (UNLIKELY(log->overflowed)) {
        return;
    }
    if (UNLIKELY(log->tail == NULL || log->tail->count == WATCHLOG_BLOCK)) {
        if (!_watchlog_grow(log)) {
            log->overflowed = true;
            return;
        }
    }
    watch_record_t* rec = &log->tail->items[log->tail->count++];
    rec->identity = Py_XNewRef(identity);
    rec->hash = hash;
    rec->key = Py_XNewRef(key);
    rec->value = Py_XNewRef(value);
    rec->old_value = Py_XNewRef(old_value);
    rec->event = (uint8_t)event;
}

/* Moves everything recorded so far into `out` and leaves `log` empty, so a
 * callback that mutates the same multidict refills a fresh log instead of
 * appending to the chain being walked. */
static inline void
watchlog_take(watchlog_t* log, watchlog_t* out)
{
    *out = *log;
    watchlog_init(log);
}

// Drops a taken snapshot without delivering it.
static inline void
watchlog_drain(watchlog_t* log)
{
    for (watchlog_block_t* block = log->head; block != NULL;) {
        for (Py_ssize_t i = 0; i < block->count; i++) {
            watch_record_t* rec = &block->items[i];
            Py_XDECREF(rec->identity);
            Py_XDECREF(rec->key);
            Py_XDECREF(rec->value);
            Py_XDECREF(rec->old_value);
        }
        watchlog_block_t* next = block->next;
        PyMem_Free(block);
        block = next;
    }
    watchlog_init(log);
}

#ifdef __cplusplus
}
#endif
#endif
