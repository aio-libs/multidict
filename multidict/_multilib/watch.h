#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_WATCH_H
#define _MULTIDICT_WATCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../multidict_capi_struct.h"
#include "compiler.h"
#include "dict.h"
#include "state.h"
#include "watchlog.h"

/* Per-multidict watcher state, allocated on the first MultiDict_Watch()
 * and freed at dealloc. Unwatching only clears the bit: freeing here would
 * mean draining the log, and draining decrefs, which must not happen under
 * the critical section MultiDict_Unwatch() holds.
 *
 * `bits` is the same 8-slot occupancy mask CPython keeps in
 * _ma_watcher_tag, moved in here because with the record heap-allocated it
 * is free here and an in-object copy would be redundant. */
struct _md_watch {
    uint8_t bits;
    void* user_data[MULTIDICT_MAX_WATCHERS];
    watchlog_t log;
};

/* Callbacks run here: after the operation finished, outside every lock. */
static void
_md_watch_call(mod_state* state, uint8_t bits, void* const* user_data,
               const MultiDict_WatchInfo* info)
{
    for (int id = 0; bits != 0; id++, bits = (uint8_t)(bits >> 1)) {
        if ((bits & 1) == 0) {
            continue;
        }
        MultiDict_WatchCallback callback = state->watchers[id];
        if (callback == NULL) {
            // MultiDict_ClearWatcher() left the bit behind; skip it
            continue;
        }
        if (callback(state->watcher_data[id], user_data[id], info) < 0 ||
            PyErr_Occurred()) {
            /* The mutation already happened and cannot be undone, so a
               failing callback can only be reported, never propagated.
               Same rule as CPython's _PyDict_SendEvent(). `self` is at
               refcount 0 on a DEALLOCATED event and WriteUnraisable()
               increfs what it is given, so pass nothing there. */
            PyErr_WriteUnraisable(info->event == MultiDict_EVENT_DEALLOCATED
                                      ? NULL
                                      : info->self);
        }
    }
}

COLD static void
_md_watch_deliver(MultiDictObject* md, uint8_t bits, void* const* user_data,
                  watchlog_t* snapshot)
{
    mod_state* state = md->state;
    MultiDict_WatchInfo info;
    info.self = (PyObject*)md;
    if (UNLIKELY(snapshot->overflowed)) {
        /* Recording ran out of memory partway, so the stream is no longer
           a faithful description of what changed; say so once and drop it
           rather than handing over a half-truth. */
        watchlog_drain(snapshot);
        info.event = MultiDict_EVENT_LOST;
        info.identity = NULL;
        info.key = NULL;
        info.value = NULL;
        info.old_value = NULL;
        _md_watch_call(state, bits, user_data, &info);
        return;
    }
    for (watchlog_block_t* block = snapshot->head; block != NULL;) {
        for (Py_ssize_t i = 0; i < block->count; i++) {
            watch_record_t* rec = &block->items[i];
            info.event = (MultiDict_WatchEvent)rec->event;
            info.identity = rec->identity;
            info.key = rec->key;
            info.value = rec->value;
            info.old_value = rec->old_value;
            _md_watch_call(state, bits, user_data, &info);
            Py_XDECREF(rec->identity);
            Py_XDECREF(rec->key);
            Py_XDECREF(rec->value);
            Py_XDECREF(rec->old_value);
        }
        watchlog_block_t* next = block->next;
        PyMem_Free(block);
        block = next;
    }
    watchlog_init(snapshot);
}

/* True when md_watch_flush() would have something to do. Read under the
   caller's own critical section, so the flush itself needs no atomics. */
static inline bool
md_watch_pending(MultiDictObject* md)
{
    return UNLIKELY(md->watch != NULL) && !watchlog_empty(&md->watch->log);
}

/* Delivers everything recorded during the operation that just finished.
 * Must be called with no lock on `md` held. The log is swapped out under
 * the lock rather than walked in place, so a callback that mutates `md`
 * fills a fresh log instead of extending the chain being walked; the loop
 * then picks that up on the next pass. */
COLD static void
md_watch_flush(MultiDictObject* md)
{
    /* Several callers flush on their failure path too, so an exception
       can already be in flight here. Park it: otherwise the first
       callback would be blamed for it below, and reporting it as
       unraisable would clear the one the caller is about to return. */
    PyObject *exc_type, *exc_value, *exc_tb;
    PyErr_Fetch(&exc_type, &exc_value, &exc_tb);
    for (;;) {
        watchlog_t snapshot;
        uint8_t bits = 0;
        void* user_data[MULTIDICT_MAX_WATCHERS];
        bool taken = false;
        Py_BEGIN_CRITICAL_SECTION(md);
        if (md->watch != NULL && !watchlog_empty(&md->watch->log)) {
            watchlog_take(&md->watch->log, &snapshot);
            bits = md->watch->bits;
            /* Copied out: a callback may MultiDict_Unwatch() or even
               drop the last reference to `md` while we iterate. */
            memcpy(user_data, md->watch->user_data, sizeof(user_data));
            taken = true;
        }
        Py_END_CRITICAL_SECTION();
        if (!taken) {
            break;
        }
        _md_watch_deliver(md, bits, user_data, &snapshot);
    }
    PyErr_Restore(exc_type, exc_value, exc_tb);
}

/* Paired with md_watch_pending(), which the caller reads under the lock it
   is about to drop. */
static inline void
md_watch_flush_if(MultiDictObject* md, bool pending)
{
    if (UNLIKELY(pending)) {
        md_watch_flush(md);
    }
}

COLD static void
_md_watch_record(MultiDictObject* md, MultiDict_WatchEvent event,
                 PyObject* identity, PyObject* key, PyObject* value,
                 PyObject* old_value)
{
    if (md->watch->bits == 0) {
        // every watcher unwatched; the record itself lives until dealloc
        return;
    }
    watchlog_append(&md->watch->log, event, identity, key, value, old_value);
}

/* The only thing an unwatched multidict pays: one pointer test per
   mutation, off a field in the same cache line as `keys`. */
static inline void
md_watch_record(MultiDictObject* md, MultiDict_WatchEvent event,
                PyObject* identity, PyObject* key, PyObject* value,
                PyObject* old_value)
{
    if (UNLIKELY(md->watch != NULL)) {
        _md_watch_record(md, event, identity, key, value, old_value);
    }
}

static inline void
md_watch_record_simple(MultiDictObject* md, MultiDict_WatchEvent event)
{
    md_watch_record(md, event, NULL, NULL, NULL, NULL);
}

COLD static int
md_watch_attach(MultiDictObject* md, int watcher_id, void* user_data)
{
    if (md->watch == NULL) {
        md_watch_t* watch = PyMem_Malloc(sizeof(md_watch_t));
        if (watch == NULL) {
            PyErr_NoMemory();
            return -1;
        }
        watch->bits = 0;
        memset(watch->user_data, 0, sizeof(watch->user_data));
        watchlog_init(&watch->log);
        md->watch = watch;
    }
    md->watch->bits |= (uint8_t)(1u << watcher_id);
    md->watch->user_data[watcher_id] = user_data;
    return 0;
}

COLD static void
md_watch_detach(MultiDictObject* md, int watcher_id)
{
    if (md->watch == NULL) {
        return;
    }
    md->watch->bits &= (uint8_t)~(1u << watcher_id);
    md->watch->user_data[watcher_id] = NULL;
}

/* Runs from tp_dealloc, where `md` is at refcount 0 and unreachable by
   any other thread, so no critical section is taken. */
COLD static void
md_watch_on_dealloc(MultiDictObject* md)
{
    md_watch_t* watch = md->watch;
    // detach first: nothing a callback does can find its way back in here
    md->watch = NULL;

    PyObject *exc_type, *exc_value, *exc_tb;
    PyErr_Fetch(&exc_type, &exc_value, &exc_tb);

    /* A non-empty log here means some mutation entry point skipped its
       md_watch_flush(); the assert is the cheapest way to catch that. */
    assert(watchlog_empty(&watch->log));
    watchlog_drain(&watch->log);

    if (watch->bits != 0) {
        MultiDict_WatchInfo info;
        info.event = MultiDict_EVENT_DEALLOCATED;
        info.self = (PyObject*)md;
        info.identity = NULL;
        info.key = NULL;
        info.value = NULL;
        info.old_value = NULL;
        _md_watch_call(md->state, watch->bits, watch->user_data, &info);
    }
    PyMem_Free(watch);

    PyErr_Restore(exc_type, exc_value, exc_tb);
}

#ifdef __cplusplus
}
#endif
#endif
