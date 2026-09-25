#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_WATCH_H
#define _MULTIDICT_WATCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include "../multidict_capi_struct.h"
#include "compiler.h"
#include "dict.h"
#include "state.h"
#include "watchlog.h"

#ifdef _MSC_VER
#include <intrin.h>
#endif

/* Per-multidict watcher state, allocated on the first MultiDict_Watch()
 * and freed at dealloc. Unwatching only clears the bit: freeing here would
 * mean draining the log, and draining decrefs, which must not happen under
 * the critical section MultiDict_Unwatch() holds.
 *
 * `bits` is the occupancy mask CPython keeps in _ma_watcher_tag, moved in
 * here because with the record heap-allocated it is free here and an
 * in-object copy would be redundant.
 *
 * `user_data` holds `size` slots, one past the highest watcher ID ever
 * attached, so a multidict watched through ID 0 alone pays for one slot
 * rather than MULTIDICT_MAX_WATCHERS. A slot is only meaningful while its
 * bit is set, which also keeps every set bit below `size`. */
struct _md_watch {
    uint32_t bits;
    int size;
    watchlog_t log;
    void* user_data[];
};

static inline int
_watch_ctz(uint32_t bits)
{
    assert(bits != 0);
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ctz(bits);
#elif defined(_MSC_VER)
    unsigned long idx;
    _BitScanForward(&idx, bits);
    return (int)idx;
#else
    int n = 0;
    while (!(bits & 1)) {
        bits >>= 1;
        n++;
    }
    return n;
#endif
}

static const char*
_md_watch_event_name(MultiDict_WatchEvent event)
{
    switch (event) {
        case MultiDict_EVENT_ADDED:
            return "MultiDict_EVENT_ADDED";
        case MultiDict_EVENT_REPLACED:
            return "MultiDict_EVENT_REPLACED";
        case MultiDict_EVENT_DELETED:
            return "MultiDict_EVENT_DELETED";
        case MultiDict_EVENT_CLEARED:
            return "MultiDict_EVENT_CLEARED";
        case MultiDict_EVENT_CLONED:
            return "MultiDict_EVENT_CLONED";
        case MultiDict_EVENT_DEALLOCATED:
            return "MultiDict_EVENT_DEALLOCATED";
        case MultiDict_EVENT_BATCH_BEGIN:
            return "MultiDict_EVENT_BATCH_BEGIN";
        case MultiDict_EVENT_BATCH_END:
            return "MultiDict_EVENT_BATCH_END";
        case MultiDict_EVENT_LOST:
            return "MultiDict_EVENT_LOST";
    }
    Py_UNREACHABLE();
}

/* Names the multidict by type and address rather than passing it along:
   the hook would repr() it, which can run arbitrary code, and `self` is
   at refcount 0 on a DEALLOCATED event. Same as CPython's
   _PyDict_SendEvent(). */
COLD static void
_md_watch_report(const MultiDict_WatchInfo* info)
{
    const char* event = _md_watch_event_name(info->event);
    const char* type = Py_TYPE(info->self)->tp_name;
#if PY_VERSION_HEX >= 0x030d0000
    PyErr_FormatUnraisable(
        "Exception ignored in %s watcher callback for <%.200s object at %p>",
        event,
        type,
        (void*)info->self);
#else
    /* This one prefixes "Exception ignored ". Not %p: MSVC prints that
       without a 0x, which PyErr_FormatUnraisable() adds above. */
    char msg[320];
    PyOS_snprintf(msg,
                  sizeof(msg),
                  "in %s watcher callback for <%.200s object at 0x%" PRIxPTR
                  ">",
                  event,
                  type,
                  (uintptr_t)info->self);
    _PyErr_WriteUnraisableMsg(msg, NULL);
#endif
}

/* Callbacks run here: after the operation finished, outside every lock. */
static void
_md_watch_call(mod_state* state, uint32_t bits, void* const* user_data,
               const MultiDict_WatchInfo* info)
{
    for (; bits != 0; bits &= bits - 1) {
        int id = _watch_ctz(bits);
        MultiDict_WatchCallback callback = state->watchers[id];
        if (callback == NULL) {
            // MultiDict_ClearWatcher() left the bit behind; skip it
            continue;
        }
        if (callback(state->watcher_data[id], user_data[id], info) < 0 ||
            PyErr_Occurred()) {
            /* The mutation already happened and cannot be undone, so a
               failing callback can only be reported, never propagated.
               Same rule as CPython's _PyDict_SendEvent(). */
            _md_watch_report(info);
        }
    }
}

/* Who to call, read afresh for each event: a callback that unwatches must
   not be called again, and its user_data must not outlive the unwatch.
   The same freshness lets a watcher attached from inside a callback join
   part-way through a bulk operation, so BATCH_BEGIN/BATCH_END pair per
   operation but not per watcher; that is the accepted trade, documented
   under MultiDict_EVENT_BATCH_BEGIN in docs/capi.rst. */
static uint32_t
_md_watch_recipients(MultiDictObject* md, void** user_data)
{
    uint32_t bits = 0;
    Py_BEGIN_CRITICAL_SECTION(md);
    if (md->watch != NULL) {
        bits = md->watch->bits;
        // only the slots _md_watch_call() will read
        for (uint32_t rest = bits; rest != 0; rest &= rest - 1) {
            int id = _watch_ctz(rest);
            user_data[id] = md->watch->user_data[id];
        }
    }
    Py_END_CRITICAL_SECTION();
    return bits;
}

COLD static void
_md_watch_deliver(MultiDictObject* md, watchlog_t* snapshot)
{
    mod_state* state = md->state;
    void* user_data[MULTIDICT_MAX_WATCHERS];
    MultiDict_WatchInfo info;
    info.self = (PyObject*)md;
    if (UNLIKELY(snapshot->overflowed)) {
        /* Recording ran out of memory partway, so the stream is no longer
           a faithful description of what changed; say so once and drop it
           rather than handing over a half-truth. */
        watchlog_drain(snapshot);
        info.event = MultiDict_EVENT_LOST;
        info.identity = NULL;
        info.hash = -1;
        info.key = NULL;
        info.value = NULL;
        info.old_value = NULL;
        _md_watch_call(
            state, _md_watch_recipients(md, user_data), user_data, &info);
        return;
    }
    for (watchlog_block_t* block = snapshot->head; block != NULL;) {
        for (Py_ssize_t i = 0; i < block->count; i++) {
            watch_record_t* rec = &block->items[i];
            info.event = (MultiDict_WatchEvent)rec->event;
            info.identity = rec->identity;
            info.hash = rec->hash;
            info.key = rec->key;
            info.value = rec->value;
            info.old_value = rec->old_value;
            _md_watch_call(
                state, _md_watch_recipients(md, user_data), user_data, &info);
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

/* True when _md_watch_flush() would have something to do. Read under the
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
_md_watch_flush(MultiDictObject* md)
{
    /* Several callers flush on their failure path too, so an exception
       can already be in flight here. Park it: otherwise the first
       callback would be blamed for it below, and reporting it as
       unraisable would clear the one the caller is about to return. */
    PyObject *exc_type, *exc_value, *exc_tb;
    PyErr_Fetch(&exc_type, &exc_value, &exc_tb);
    for (;;) {
        watchlog_t snapshot;
        bool taken = false;
        Py_BEGIN_CRITICAL_SECTION(md);
        if (md->watch != NULL && !watchlog_empty(&md->watch->log)) {
            watchlog_take(&md->watch->log, &snapshot);
            taken = true;
        }
        Py_END_CRITICAL_SECTION();
        if (!taken) {
            break;
        }
        _md_watch_deliver(md, &snapshot);
    }
    PyErr_Restore(exc_type, exc_value, exc_tb);
}

/* Paired with md_watch_pending(), which the caller reads under the lock it
   is about to drop. */
static inline void
md_watch_flush_if(MultiDictObject* md, bool pending)
{
    if (UNLIKELY(pending)) {
        _md_watch_flush(md);
    }
}

COLD static void
_md_watch_record(MultiDictObject* md, MultiDict_WatchEvent event,
                 PyObject* identity, Py_hash_t hash, PyObject* key,
                 PyObject* value, PyObject* old_value)
{
    if (md->watch->bits == 0) {
        // every watcher unwatched; the record itself lives until dealloc
        return;
    }
    watchlog_append(
        &md->watch->log, event, identity, hash, key, value, old_value);
}

/* The only thing an unwatched multidict pays: one pointer test per
   mutation, off a field in the same cache line as `keys`. */
static inline void
md_watch_record(MultiDictObject* md, MultiDict_WatchEvent event,
                PyObject* identity, Py_hash_t hash, PyObject* key,
                PyObject* value, PyObject* old_value)
{
    if (UNLIKELY(md->watch != NULL)) {
        _md_watch_record(md, event, identity, hash, key, value, old_value);
    }
}

static inline void
md_watch_record_simple(MultiDictObject* md, MultiDict_WatchEvent event)
{
    md_watch_record(md, event, NULL, -1, NULL, NULL, NULL);
}

COLD static int
md_watch_attach(MultiDictObject* md, int watcher_id, void* user_data)
{
    Py_BUILD_ASSERT(MULTIDICT_MAX_WATCHERS <= 32);
    md_watch_t* watch = md->watch;
    if (watch == NULL || watcher_id >= watch->size) {
        /* Every access to the record happens under this critical section
           or in dealloc, so nothing can be holding the old address. */
        bool fresh = watch == NULL;
        int size = watcher_id + 1;
        watch = PyMem_Realloc(
            watch, sizeof(md_watch_t) + sizeof(void*) * (size_t)size);
        if (watch == NULL) {
            PyErr_NoMemory();
            return -1;
        }
        if (fresh) {
            watch->bits = 0;
            watchlog_init(&watch->log);
        }
        watch->size = size;
        md->watch = watch;
    }
    md->watch->bits |= 1u << watcher_id;
    md->watch->user_data[watcher_id] = user_data;
    return 0;
}

COLD static void
md_watch_detach(MultiDictObject* md, int watcher_id)
{
    if (md->watch == NULL || watcher_id >= md->watch->size) {
        return;
    }
    md->watch->bits &= ~(1u << watcher_id);
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
       _md_watch_flush(); the assert is the cheapest way to catch that. */
    assert(watchlog_empty(&watch->log));
    watchlog_drain(&watch->log);

    if (watch->bits != 0) {
        MultiDict_WatchInfo info;
        info.event = MultiDict_EVENT_DEALLOCATED;
        info.self = (PyObject*)md;
        info.identity = NULL;
        info.hash = -1;
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
