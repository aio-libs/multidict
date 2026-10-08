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

#include "bitmap.h"
#include "compiler.h"
#include "dict.h"
#include "htkeys.h"
#include "identity.h"
#include "str_cmp.h"

/* Matches kept in the short list before starting the bitmap: MD_SEEN_FEW
   when the bitmap fits its inline buffer, MD_SEEN_MANY when it would need
   a heap allocation, which a longer list scan still beats. */
#define MD_SEEN_FEW 8
#define MD_SEEN_MANY 32

/* Entry indices already handed to the visitor by one walk. */
typedef struct _md_seen {
    /* Most keys have a handful of values, so the first matches go in a
       short list and the bitmap only starts past it. */
    Py_ssize_t few[MD_SEEN_MANY];
    Py_ssize_t nfew;
    bitmap_t bitmap;
} md_seen_t;

/* Past this many matches, the short list is moved into the bitmap. */
COLD static int
_md_seen_spill(md_seen_t* seen, MultiDictObject* md)
{
    bitmap_init(&seen->bitmap, md->keys, md->keys->nentries);
    Py_ssize_t n = seen->nfew;
    seen->nfew = MD_SEEN_MANY + 1;
    for (Py_ssize_t i = 0; i < n; i++) {
        if (bitmap_set(&seen->bitmap, seen->few[i]) < 0) {
            return -1;
        }
    }
    return 0;
}

/* 1 if `index` was already returned by this walk, 0 if not (it is now
   recorded), -1 on error. */
static int
_md_seen_test_and_add(md_seen_t* seen, MultiDictObject* md, Py_ssize_t index)
{
    Py_ssize_t n = seen->nfew;
    if (n <= MD_SEEN_MANY) {
        /* A repeat is most often the slot just returned, which the next
           step re-examines, so the list is scanned from its end. */
        for (Py_ssize_t i = n - 1; i >= 0; i--) {
            if (seen->few[i] == index) {
                return 1;
            }
        }
        if (n < MD_SEEN_FEW ||
            (n < MD_SEEN_MANY && md->keys->nentries > BITMAP_INLINE_BITS)) {
            seen->few[n] = index;
            seen->nfew = n + 1;
            return 0;
        }
        if (_md_seen_spill(seen, md) < 0) {
            return -1;
        }
    }
    return bitmap_test_and_set(&seen->bitmap, index);
}

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

   The linear scan cannot reach an entry twice, so unlike md_walk() there is
   no seen set to keep.

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
    if (kind_is_compact(keys->kind)) {
        entry_t* entry = HTKEYS_COMPACT_ENTRIES(keys);
        for (entry_t* end = entry + keys->nentries; ret > 0 && entry < end;
             entry++) {
            if (!compact_entry_is_hole(entry)) {
                count++;
                ret = _md_walk_visit(md,
                                     entry,
                                     compact_entry_identity(ci, entry),
                                     compact_entry_hash(ci, entry),
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

#define _MD_WALK_SEEN 2

/* A matched entry at `index` of md_walk_with_hash()'s chain: 1 once
   visited, 0 if the visitor stopped the walk, _MD_WALK_SEEN if the walk
   visited it already, -1 with an exception set. */
static inline int
_md_walk_matched(MultiDictObject* md, entry_t* entry, Py_ssize_t index,
                 md_seen_t* seen, PyObject* identity, Py_hash_t hash,
                 bool with_keys, md_item_visitor_t visitor, void* user_data,
                 uint64_t version)
{
    /* HTKEYSITER_NEXT() can repeat a slot already seen in this scan
       (see its doc comment), and this scan never marks the table. */
    int seen_before = _md_seen_test_and_add(seen, md, index);
    if (seen_before < 0) {
        return -1;
    }
    if (seen_before) {
        return _MD_WALK_SEEN;
    }

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

/* Calls `visitor` once for every entry whose identity is `identity`, in probe
   order; `hash` is that identity's hash. Returns how many entries were
   visited, or -1 with an exception set. The caller holds md's critical
   section.

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

    /* Not zero-initialized: the bitmap's inline buffer is 4 KB.
       The release below only needs `nfew`. */
    md_seen_t seen;
    seen.nfew = 0;

    Py_ssize_t count = 0;
    int ret = 1;
    entry_t* entry = NULL;
    if (kind_is_compact(md->keys->kind)) {
        while (ret > 0) {
            HTKEYSITER_FIND_COMPACT(&iter, ci, identity, hash, entry);
            if (entry == NULL) {
                break;
            }
            ret = _md_walk_matched(md,
                                   entry,
                                   iter.index,
                                   &seen,
                                   identity,
                                   hash,
                                   with_keys,
                                   visitor,
                                   user_data,
                                   version);
            count += ret == 0 || ret == 1;
        }
    } else {
        while (ret > 0) {
            HTKEYSITER_FIND_ANYSTR(&iter, identity, hash, entry);
            if (entry == NULL) {
                break;
            }
            ret = _md_walk_matched(md,
                                   entry,
                                   iter.index,
                                   &seen,
                                   identity,
                                   hash,
                                   with_keys,
                                   visitor,
                                   user_data,
                                   version);
            count += ret == 0 || ret == 1;
        }
    }
    if (seen.nfew > MD_SEEN_MANY) {
        bitmap_release(&seen.bitmap);
    }
    return ret < 0 ? -1 : count;
}

#undef _MD_WALK_SEEN

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
