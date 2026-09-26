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

/* Versions on free-threaded builds.

   Every mutation stores a new md->version, and no two multidicts may ever
   report the same one: getversion() users key caches on (id(md), version),
   and an id is reused once its multidict is freed.

   One process-wide fetch-add per mutation bounced a cache line between
   every mutating thread, and a thread-local counter costs a
   __tls_get_addr() call per mutation in a shared object. So versions are
   handed out at three levels:

     global_version  process-wide atomic; a thread reserves VERSION_BATCH
                     versions from it at a time.
     version_block   thread-local; the thread splits its batch into blocks
                     of VERSION_BLOCK, one per multidict that needs one.
     md->version     the multidict's cursor into its own block, advanced
                     by bump_version() under md's critical section.

   A mutation is therefore a plain increment of a field md already holds.
   Thread-local storage is touched once per block: when a multidict is
   created, then every VERSION_BLOCK - 1 mutations. global_version is
   touched once per VERSION_BATCH / VERSION_BLOCK blocks.

   No block's base, a multiple of VERSION_BLOCK, is ever handed out as a
   version, and no batch's base, a multiple of VERSION_BATCH, as a block.
   That keeps a spent block, a spent batch and a new multidict (version 0)
   recognizable with one mask test each; see bump_version().

   A block belongs to one multidict and a batch to one thread, so versions
   are unique. They are not ordered: a multidict's next block can come from
   an older batch of another thread, and getversion() only ever promised
   equality. global_version is process-wide rather than in mod_state, so a
   batch outliving its module state cannot overlap a newer one. The GIL
   build keeps a single plain counter. */
#define VERSION_BATCH 65536
#define VERSION_BLOCK 256

static uint64_t global_version;
/* The last block this thread handed out. No block starts at a multiple
   of VERSION_BATCH, so reaching one means the batch is spent; starting
   one block short of it makes the first call refill. */
static THREAD_LOCAL uint64_t version_block = VERSION_BATCH - VERSION_BLOCK;

static COLD uint64_t
_refill_version_batch(void)
{
    uint64_t base =
        atomic_fetch_add_uint64_relaxed(&global_version, VERSION_BATCH);
    version_block = base + VERSION_BLOCK;
    return version_block;
}

/* One read-modify-write, so one TLS lookup. */
static COLD uint64_t
_new_version_block(void)
{
    uint64_t block = version_block += VERSION_BLOCK;
    if (UNLIKELY((block & (VERSION_BATCH - 1)) == 0)) {
        block = _refill_version_batch();
    }
    return block;
}

/* md->version is the cursor into md's own block, advanced under md's
   critical section, so a mutation touches neither thread-local storage
   nor a shared counter. A block's own base is never handed out: low
   bits of 0 after the increment mean the block is spent, and low bits
   of 1 mean md never had one (a new multidict starts at version 0). */
static inline uint64_t
bump_version(MultiDictObject* md)
{
    uint64_t version = md->version + 1;
    if (UNLIKELY((version & (VERSION_BLOCK - 1)) <= 1)) {
        version = _new_version_block() + 1;
    }
    store_version(md, version);
    return version;
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
bump_version(MultiDictObject* md)
{
    uint64_t version = ++md->state->global_version;
    md->version = version;
    return version;
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
