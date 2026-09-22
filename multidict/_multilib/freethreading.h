#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_FREETHREADING_H
#define _MULTIDICT_FREETHREADING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <stdint.h>

#include "atomic_helpers.h"
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

/* Shared across multidicts, so no two ever report the same version. */
static inline uint64_t
next_version(mod_state* state)
{
    return atomic_fetch_add_uint64_relaxed(&state->global_version, 1) + 1;
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
   md's bookkeeping is consistent again. store_value() instead
   drops it on the spot. */
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

/* Replaces the value and drops the reference the entry held. */
static inline void
store_value(entry_t* entry, PyObject* value)
{
    PyObject* old = load_value(entry);
    publish_value(entry, value);
    Py_XDECREF(old);
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

/* Replaces the value and drops the reference the entry held. */
static inline void
store_value(entry_t* entry, PyObject* value)
{
    PyObject* old = load_value(entry);
    publish_value(entry, value);
    Py_XDECREF(old);
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

#endif /* Py_GIL_DISABLED */

#ifdef __cplusplus
}
#endif
#endif
