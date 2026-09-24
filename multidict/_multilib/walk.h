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
static inline int
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

static inline void
_md_seen_release(md_seen_t* seen)
{
    if (seen->nfew > MD_SEEN_MANY) {
        bitmap_release(&seen->bitmap);
    }
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

/* Calls `visitor` once for every live entry, in insertion order. Returns how
   many entries were visited, or -1 with an exception set. The caller holds
   md's critical section.

   The linear scan cannot reach an entry twice, so unlike md_walk() there is
   no seen set to keep.

   `visitor` must not call back into `md`, for the reason md_walk() gives. */
ALWAYS_INLINE static inline Py_ssize_t
md_walk_all(MultiDictObject* md, bool with_keys, md_item_visitor_t visitor,
            void* user_data)
{
    uint64_t version = md->version;
    htkeys_t* keys = md->keys;
    entry_t* entries = htkeys_entries(keys);

    Py_ssize_t count = 0;
    for (Py_ssize_t pos = 0; pos < keys->nentries; pos++) {
        entry_t* entry = entries + pos;
        if (entry->identity == NULL) {
            continue;
        }

        PyObject* identity = Py_NewRef(entry->identity);
        Py_hash_t hash = entry->hash;
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
        count++;
        int ret = visitor(user_data, identity, hash, key, value);
        Py_XDECREF(key);
        Py_DECREF(value);
        Py_DECREF(identity);
        if (ret < 0) {
            assert(PyErr_Occurred());
            return -1;
        }
        /* md_ensure_key() and the visitor can both run Python code. */
        if (keys != md->keys || version != md->version) {
            PyErr_SetString(PyExc_RuntimeError,
                            "MultiDict is changed during iteration");
            return -1;
        }
        if (ret == 0) {
            break;
        }
    }
    return count;
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
ALWAYS_INLINE static inline Py_ssize_t
md_walk_with_hash(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                  bool with_keys, md_item_visitor_t visitor, void* user_data)
{
    uint64_t version = md->version;
    htkeys_t* keys = md->keys;
    entry_t* entries = htkeys_entries(keys);
    htkeysiter_t iter;
    htkeysiter_init(&iter, keys, hash);

    /* Not zero-initialized: the bitmap's inline buffer is 4 KB.
       _md_seen_release() only needs `nfew`. */
    md_seen_t seen;
    seen.nfew = 0;

    Py_ssize_t count = 0;
    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t* entry = entries + iter.index;
        if (entry->hash != hash) {
            continue;
        }
        if (!str_cmp(identity, entry->identity)) {
            continue;
        }

        /* htkeysiter_next() can repeat a slot already seen in this scan
           (see its doc comment), and this scan never marks the table. */
        int seen_before = _md_seen_test_and_add(&seen, md, iter.index);
        if (seen_before < 0) {
            goto fail;
        }
        if (seen_before) {
            continue;
        }

        PyObject* value = Py_NewRef(entry->value);
        PyObject* key = NULL;
        if (with_keys) {
            key = md_ensure_key(md, entry);  // last entry access
            if (key == NULL) {
                Py_DECREF(value);
                goto fail;
            }
        }
        count++;
        int ret = visitor(user_data, identity, hash, key, value);
        Py_XDECREF(key);
        Py_DECREF(value);
        if (ret < 0) {
            assert(PyErr_Occurred());
            goto fail;
        }
        /* md_ensure_key() and the visitor can both run Python code. */
        if (keys != md->keys || version != md->version) {
            PyErr_SetString(PyExc_RuntimeError,
                            "MultiDict is changed during iteration");
            goto fail;
        }
        if (ret == 0) {
            break;
        }
    }
    _md_seen_release(&seen);
    return count;
fail:
    _md_seen_release(&seen);
    return -1;
}

/* md_walk_with_hash() for callers that have no hash at hand yet. */
ALWAYS_INLINE static inline Py_ssize_t
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
