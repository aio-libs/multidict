#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_FREETHREADING_H
#define _MULTIDICT_FREETHREADING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <stdint.h>

#include "atomic_helpers.h"
#include "compiler.h"
#include "dict.h"
#include "htkeys.h"
#include "state.h"

/* Each field uses one memory order throughout, so the names do not
   carry one. */

/* 3.13t is the only free-threaded build without PyUnstable_TryIncRef(),
   and it is unsupported since 6.8.0; the lock-free readers below assume
   it away. */
#if defined(Py_GIL_DISABLED) && PY_VERSION_HEX < 0x030e0000
#error "the free-threaded build requires CPython 3.14 or newer"
#endif

/*
entry->identity is the "slot is populated" signal for a lock-free walk,
so insertion publishes it last and deletion clears it first, and it
never goes from one non-NULL identity to another. That orders the
fields but does not keep the objects alive: a concurrent delete decrefs
on the spot, so reading identity's or value's contents needs
try_get_ref().
*/

#ifdef Py_GIL_DISABLED

/* getversion() reads this without md's critical section. */
static inline uint64_t
load_version(const MultiDictObject* md)
{
    return atomic_load_uint64_relaxed(&md->version);
}

static inline void
store_version(MultiDictObject* md, uint64_t version)
{
    atomic_store_uint64_relaxed(&md->version, version);
}

/* md_len() reads this lock-free. */
static inline Py_ssize_t
load_used(const MultiDictObject* md)
{
    return atomic_load_ssize_relaxed(&md->used);
}

static inline void
store_used(MultiDictObject* md, Py_ssize_t used)
{
    atomic_store_ssize_relaxed(&md->used, used);
}

static inline void
add_used(MultiDictObject* md, Py_ssize_t delta)
{
    atomic_fetch_add_ssize_relaxed(&md->used, delta);
}

/* Pairs with _md_reader_enter()'s atomic_load_ptr(). A plain store
   would race it, and on weak memory models would carry no ordering
   against the num_active_readers check retirement depends on. */
static inline void
store_keys(MultiDictObject* md, htkeys_t* keys)
{
    atomic_store_ptr((void**)&md->keys, keys);
}

/* Shared across multidicts, so no two ever report the same version.
   Process-wide rather than in mod_state, so a batch outliving its
   module state cannot overlap a newer one. Each thread reserves
   VERSION_BATCH at a time: a fetch-add per mutation bounced one cache
   line between every mutating thread. Versions stay unique but are
   unordered across threads, which getversion() never promised. */
#define VERSION_BATCH 256

static uint64_t global_version;
static THREAD_LOCAL uint64_t version_next;
static THREAD_LOCAL uint64_t version_end;

static COLD void
_refill_versions(void)
{
    version_next =
        atomic_fetch_add_uint64_relaxed(&global_version, VERSION_BATCH);
    version_end = version_next + VERSION_BATCH;
}

static inline uint64_t
next_version(mod_state* state)
{
    (void)state;
    if (UNLIKELY(version_next == version_end)) {
        _refill_versions();
    }
    return ++version_next;
}

static inline PyObject*
load_identity(entry_t* entry)
{
    return (PyObject*)atomic_load_ptr((void* const*)&entry->identity);
}

/* The GIL arm skips the marking: a no-op there, but a real call. */
static inline void
publish_identity(entry_t* entry, PyObject* identity)
{
    PyUnstable_EnableTryIncRef(identity);
    atomic_store_ptr((void**)&entry->identity, identity);
}

/* Leaves the old reference to the caller, which decrefs it only once
   md's bookkeeping is consistent again. */
static inline void
reset_identity(entry_t* entry)
{
    atomic_store_ptr((void**)&entry->identity, NULL);
}

static inline PyObject*
load_value(entry_t* entry)
{
    return (PyObject*)atomic_load_ptr((void* const*)&entry->value);
}

/* See publish_identity(). */
static inline void
publish_value(entry_t* entry, PyObject* value)
{
    PyUnstable_EnableTryIncRef(value);
    atomic_store_ptr((void**)&entry->value, value);
}

/* See reset_identity(). */
static inline void
reset_value(entry_t* entry)
{
    atomic_store_ptr((void**)&entry->value, NULL);
}

/* _md_replace()/_md_update() overwrite hash in place on a live entry,
   so a reader's plain read would race it. Relaxed is enough: it is
   read only after the identity check has ordered the rest. */
static inline Py_hash_t
load_hash(entry_t* entry)
{
    return (Py_hash_t)atomic_load_ssize_relaxed((Py_ssize_t*)&entry->hash);
}

static inline void
store_hash(entry_t* entry, Py_hash_t hash)
{
    atomic_store_ssize_relaxed((Py_ssize_t*)&entry->hash, (Py_ssize_t)hash);
}

/* NULL means the caller must fall back to the critical section, which
   includes the case of the field legitimately being NULL. */
static inline PyObject*
try_get_ref(PyObject** addr)
{
    PyObject* value = (PyObject*)atomic_load_ptr((void* const*)addr);
    if (value == NULL) {
        return NULL;
    }
    if (!PyUnstable_TryIncRef(value)) {
        return NULL;
    }
    if ((PyObject*)atomic_load_ptr((void* const*)addr) != value) {
        Py_DECREF(value);
        return NULL;
    }
    return value;
}

/* A registration is written under watcher_mutex and read lock-free by
   delivery. Clearing bumps the generation before it empties the slot, and
   registering stores the data before the callback, so a reader that loads
   the callback, then the data, then finds the generation it expects has a
   pair from one registration: anything newer would have shown it the
   bump. */
static inline MultiDict_WatchCallback
load_watcher(mod_state* state, int watcher_id)
{
    return (MultiDict_WatchCallback)atomic_load_ptr(
        (void* const*)&state->watchers[watcher_id]);
}

static inline void*
load_watcher_data(mod_state* state, int watcher_id)
{
    return atomic_load_ptr(&state->watcher_data[watcher_id]);
}

static inline uint64_t
load_watcher_generation(mod_state* state, int watcher_id)
{
    return atomic_load_uint64_relaxed(&state->watcher_generation[watcher_id]);
}

static inline void
publish_watcher(mod_state* state, int watcher_id,
                MultiDict_WatchCallback callback, void* watcher_data)
{
    atomic_store_ptr(&state->watcher_data[watcher_id], watcher_data);
    atomic_store_ptr((void**)&state->watchers[watcher_id], (void*)callback);
}

static inline void
retire_watcher(mod_state* state, int watcher_id)
{
    atomic_store_uint64_relaxed(&state->watcher_generation[watcher_id],
                                state->watcher_generation[watcher_id] + 1);
    atomic_store_ptr((void**)&state->watchers[watcher_id], NULL);
    atomic_store_ptr(&state->watcher_data[watcher_id], NULL);
}

#else /* Py_GIL_DISABLED */

static inline uint64_t
load_version(const MultiDictObject* md)
{
    return md->version;
}

static inline void
store_version(MultiDictObject* md, uint64_t version)
{
    md->version = version;
}

static inline Py_ssize_t
load_used(const MultiDictObject* md)
{
    return md->used;
}

static inline void
store_used(MultiDictObject* md, Py_ssize_t used)
{
    md->used = used;
}

static inline void
add_used(MultiDictObject* md, Py_ssize_t delta)
{
    md->used += delta;
}

static inline void
store_keys(MultiDictObject* md, htkeys_t* keys)
{
    md->keys = keys;
}

static inline uint64_t
next_version(mod_state* state)
{
    return ++state->global_version;
}

static inline PyObject*
load_identity(entry_t* entry)
{
    return entry->identity;
}

static inline void
publish_identity(entry_t* entry, PyObject* identity)
{
    entry->identity = identity;
}

static inline void
reset_identity(entry_t* entry)
{
    entry->identity = NULL;
}

static inline PyObject*
load_value(entry_t* entry)
{
    return entry->value;
}

static inline void
publish_value(entry_t* entry, PyObject* value)
{
    entry->value = value;
}

static inline void
reset_value(entry_t* entry)
{
    entry->value = NULL;
}

static inline Py_hash_t
load_hash(entry_t* entry)
{
    return entry->hash;
}

static inline void
store_hash(entry_t* entry, Py_hash_t hash)
{
    entry->hash = hash;
}

static inline MultiDict_WatchCallback
load_watcher(mod_state* state, int watcher_id)
{
    return state->watchers[watcher_id];
}

static inline void*
load_watcher_data(mod_state* state, int watcher_id)
{
    return state->watcher_data[watcher_id];
}

static inline uint64_t
load_watcher_generation(mod_state* state, int watcher_id)
{
    return state->watcher_generation[watcher_id];
}

static inline void
publish_watcher(mod_state* state, int watcher_id,
                MultiDict_WatchCallback callback, void* watcher_data)
{
    state->watcher_data[watcher_id] = watcher_data;
    state->watchers[watcher_id] = callback;
}

static inline void
retire_watcher(mod_state* state, int watcher_id)
{
    state->watcher_generation[watcher_id]++;
    state->watchers[watcher_id] = NULL;
    state->watcher_data[watcher_id] = NULL;
}

#endif /* Py_GIL_DISABLED */

#ifdef __cplusplus
}
#endif
#endif
