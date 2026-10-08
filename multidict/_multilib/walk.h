#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_WALK_H
#define _MULTIDICT_WALK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "compiler.h"
#include "dict.h"
#include "htkeys.h"
#include "identity.h"
#include "str_cmp.h"

/* Visitor for md_walk().

   `identity`, `key` and `value` are borrowed: the walk holds a reference to
   each for the duration of the call and releases it right after, so none can
   be freed under the visitor even on Py_GIL_DISABLED. `key` is NULL unless
   the walk was started with `with_keys`; `identity` and its `hash` are always
   passed, since both walks have them in hand anyway.

   Return > 0 to continue the walk, 0 to stop it, < 0 to abort it with the
   exception the visitor has set. */
typedef int (*md_item_visitor_t)(void* user_data, PyObject* identity,
                                 Py_hash_t hash, PyObject* key,
                                 PyObject* value);

/* Hands md_walk_all()'s visitor one live entry: 1 to go on, 0 if the
   visitor stopped the walk, -1 with an exception set. */
static inline int
_md_walk_visit(MultiDictObject* md, entry_t* entry, PyObject* identity,
               Py_hash_t hash, bool with_keys, md_item_visitor_t visitor,
               void* user_data, uint64_t version)
{
    Py_INCREF(identity);
    PyObject* value = Py_NewRef(entry->value);
    PyObject* key = NULL;
    if (with_keys) {
        key = md_ensure_key(md, entry);  // last entry access
        if (key == NULL) {
            Py_DECREF(value);
            Py_DECREF(identity);
            return -1;
        }
    }
    int ret = visitor(user_data, identity, hash, key, value);
    Py_XDECREF(key);
    Py_DECREF(value);
    Py_DECREF(identity);
    if (ret < 0) {
        assert(PyErr_Occurred());
        return -1;
    }
    /* md_ensure_key() and the visitor can both run Python code. */
    if (md_check_version(md, version) < 0) {
        return -1;
    }
    return ret == 0 ? 0 : 1;
}

/* Calls `visitor` once for every live entry, in insertion order. Returns how
   many entries were visited, or -1 with an exception set. The caller holds
   md's critical section.

   The linear scan cannot reach an entry twice, so unlike md_walk() it has
   no repeats to skip.

   `visitor` must not call back into `md`, for the reason md_walk() gives. */
static Py_ssize_t
md_walk_all(MultiDictObject* md, bool with_keys, md_item_visitor_t visitor,
            void* user_data)
{
    bool ci = md->is_ci;
    uint64_t version = md->version;
    // the version check in _md_walk_visit() keeps the table in place
    htkeys_t* keys = md->keys;
    Py_ssize_t count = 0;
    int ret = 1;
    if (kind_is_compact(keys->kind) && ci) {
        entry_t* entry = HTKEYS_COMPACT_ENTRIES(keys);
        for (entry_t* end = entry + keys->nentries; ret > 0 && entry < end;
             entry++) {
            if (!compact_entry_is_hole(entry)) {
                count++;
                ret = _md_walk_visit(md,
                                     entry,
                                     compact_entry_identity_ci(entry),
                                     compact_entry_hash_ci(entry),
                                     with_keys,
                                     visitor,
                                     user_data,
                                     version);
            }
        }
    } else if (kind_is_compact(keys->kind)) {
        entry_t* entry = HTKEYS_COMPACT_ENTRIES(keys);
        for (entry_t* end = entry + keys->nentries; ret > 0 && entry < end;
             entry++) {
            if (!compact_entry_is_hole(entry)) {
                count++;
                ret = _md_walk_visit(md,
                                     entry,
                                     compact_entry_identity_cs(entry),
                                     compact_entry_hash_cs(entry),
                                     with_keys,
                                     visitor,
                                     user_data,
                                     version);
            }
        }
    } else {
        anystr_entry_t* entry = HTKEYS_ANYSTR_ENTRIES(keys);
        for (anystr_entry_t* end = entry + keys->nentries;
             ret > 0 && entry < end;
             entry++) {
            if (!anystr_entry_is_hole(entry)) {
                count++;
                ret = _md_walk_visit(md,
                                     &entry->base,
                                     entry->identity,
                                     entry->hash,
                                     with_keys,
                                     visitor,
                                     user_data,
                                     version);
            }
        }
    }
    return ret < 0 ? -1 : count;
}

/* A matched entry of md_walk_with_hash()'s chain: 1 once visited, 0 if
   the visitor stopped the walk, -1 with an exception set. */
static inline int
_md_walk_matched(MultiDictObject* md, entry_t* entry, PyObject* identity,
                 Py_hash_t hash, bool with_keys, md_item_visitor_t visitor,
                 void* user_data, uint64_t version)
{
    PyObject* value = Py_NewRef(entry->value);
    PyObject* key = NULL;
    if (with_keys) {
        key = md_ensure_key(md, entry);  // last entry access
        if (key == NULL) {
            Py_DECREF(value);
            return -1;
        }
    }
    int ret = visitor(user_data, identity, hash, key, value);
    Py_XDECREF(key);
    Py_DECREF(value);
    if (ret < 0) {
        assert(PyErr_Occurred());
        return -1;
    }
    /* md_ensure_key() and the visitor can both run Python code. */
    if (md_check_version(md, version) < 0) {
        return -1;
    }
    return ret == 0 ? 0 : 1;
}

/* Calls `visitor` once for every entry whose identity is `identity`, in
   insertion order; `hash` is that identity's hash. Returns how many entries
   were visited, or -1 with an exception set. The caller holds md's critical
   section.

   HTKEYSITER_NEXT() can repeat a slot (see its doc comment), but every
   entry went into the first slot of its probe sequence that was empty at
   the time, and a slot never becomes empty again: a rebuild starts over
   and reinserts in entry order. So the matches are first reached in
   increasing entry index, and one whose index is not above the last match
   is a repeat.

   `visitor` gets `identity` and `hash` themselves rather than the matched
   entry's own; both always compare equal, since that is the match
   predicate.

   `visitor` must not call back into `md`: the version stamped at the start is
   rechecked after every visitor call, so a reentrant mutation ends the walk
   with "MultiDict is changed during iteration" instead of walking a table
   that moved. */
static Py_ssize_t
md_walk_with_hash(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                  bool with_keys, md_item_visitor_t visitor, void* user_data)
{
    bool ci = md->is_ci;
    uint64_t version = md->version;
    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, md->keys, hash);

    Py_ssize_t count = 0;
    Py_ssize_t last = -1;
    int ret = 1;
    entry_t* entry = NULL;
    if (kind_is_compact(md->keys->kind) && ci) {
        while (ret > 0) {
            HTKEYSITER_FIND_COMPACT_CI(&iter, identity, hash, entry);
            if (entry == NULL) {
                break;
            }
            if (iter.index <= last) {
                continue;
            }
            last = iter.index;
            ret = _md_walk_matched(md,
                                   entry,
                                   identity,
                                   hash,
                                   with_keys,
                                   visitor,
                                   user_data,
                                   version);
            count += ret >= 0;
        }
    } else if (kind_is_compact(md->keys->kind)) {
        while (ret > 0) {
            HTKEYSITER_FIND_COMPACT_CS(&iter, identity, hash, entry);
            if (entry == NULL) {
                break;
            }
            if (iter.index <= last) {
                continue;
            }
            last = iter.index;
            ret = _md_walk_matched(md,
                                   entry,
                                   identity,
                                   hash,
                                   with_keys,
                                   visitor,
                                   user_data,
                                   version);
            count += ret >= 0;
        }
    } else {
        while (ret > 0) {
            HTKEYSITER_FIND_ANYSTR(&iter, identity, hash, entry);
            if (entry == NULL) {
                break;
            }
            if (iter.index <= last) {
                continue;
            }
            last = iter.index;
            ret = _md_walk_matched(md,
                                   entry,
                                   identity,
                                   hash,
                                   with_keys,
                                   visitor,
                                   user_data,
                                   version);
            count += ret >= 0;
        }
    }
    return ret < 0 ? -1 : count;
}

/* md_walk_with_hash() for callers that have no hash at hand yet. */
static Py_ssize_t
md_walk(MultiDictObject* md, PyObject* identity, bool with_keys,
        md_item_visitor_t visitor, void* user_data)
{
    Py_hash_t hash = unicode_hash(identity);
    if (hash == -1) {
        return -1;
    }
    return md_walk_with_hash(
        md, identity, hash, with_keys, visitor, user_data);
}

#ifdef __cplusplus
}
#endif

#endif
