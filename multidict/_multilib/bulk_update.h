#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_BULK_UPDATE_H
#define _MULTIDICT_BULK_UPDATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bitmap.h"
#include "compiler.h"
#include "debug.h"
#include "dict.h"
#include "freethreading.h"
#include "hashtable.h"
#include "htkeys.h"
#include "identity.h"
#include "reflist.h"
#include "unpack.h"
#include "update_marks.h"
#include "walk.h"
#include "watch.h"

typedef enum _UpdateOp {
    Extend,
    Update,
    Merge,
} UpdateOp;

typedef struct _md_update_state md_update_state_t;

/* What a batch does with one item: _md_update() or _md_merge(). */
typedef int (*md_update_action_t)(md_update_state_t* st, Py_hash_t hash,
                                  PyObject* identity, PyObject* key,
                                  PyObject* value, bool fits);

/* One update() or merge() batch, set up by md_update_state_init() and,
   under md's critical section, update_marks_init(); torn down by
   md_update_state_release(). extend() has none. */
struct _md_update_state {
    MultiDictObject* md;
    md_update_action_t action;
    update_marks_t marks;
    /* Only update() defers decrefs: merge() never drops a reference
       mid-scan, it either finds the key or inserts a new entry. */
    reflist_t defer;
};

/* Marks the entry at `index` as written: the first on _md_update()'s
   chain with the key that this batch has not written yet, which gets the
   new key and value from the caller. -1 on error. */
static inline int
_md_update_claim(update_marks_t* marks, Py_ssize_t index)
{
    /* Marked first: nothing below can fail half-way after the entry has
       changed. An entry an earlier item of this batch doomed is reused,
       which keeps the key at its position. */
    if (UNLIKELY(bitmap_set(&marks->updated, index) < 0)) {
        return -1;
    }
    bitmap_clear(&marks->deleted, index);
    return 0;
}

/* Gives the entry _md_update_claim() marked its new value, once the caller
   has replaced `old_key` with `key`. -1 on error. */
static inline int
_md_update_store(md_update_state_t* st, entry_t* entry, PyObject* old_key,
                 Py_hash_t hash, PyObject* identity, PyObject* key,
                 PyObject* value)
{
    // old_key/old_value decref deferred: see reflist_t
    PyObject* old_value = load_value(entry);
    publish_value(entry, Py_NewRef(value));
    MD_WATCH_RECORD(st->md,
                    MultiDict_EVENT_REPLACED,
                    identity,
                    hash,
                    key,
                    value,
                    old_value);
    return reflist_push2(&st->defer, old_key, old_value);
}

/* Nothing here runs Python code or suspends the critical section: the
 * replaced key and value go to `defer`. One loop per kind. */
static int
_md_update(md_update_state_t* st, Py_hash_t hash, PyObject* identity,
           PyObject* key, PyObject* value, bool fits)
{
    MultiDictObject* md = st->md;
    update_marks_t* marks = &st->marks;
    assert(fits == md_key_fits(md, key, identity));
    if (kind_is_compact(md->keys->kind) && UNLIKELY(!fits) &&
        UNLIKELY(md_to_anystr(md) < 0)) {
        return -1;
    }
    if (UNLIKELY(update_marks_sync(marks, md) < 0)) {
        return -1;
    }
    bool ci = md->is_ci;
    bool found = false;
    bool added = false;
    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, md->keys, hash);

    if (kind_is_compact(md->keys->kind)) {
        entry_t* entry = NULL;
        if (ci) {
            for (;;) {
                HTKEYSITER_FIND_COMPACT_CI(&iter, identity, hash, entry);
                if (entry == NULL) {
                    break;
                }
                if (bitmap_test(&marks->updated, iter.index)) {
                    added = true;
                    continue;
                }
                if (found) {
                    if (UNLIKELY(update_marks_doom(marks, iter.index, entry) <
                                 0)) {
                        return -1;
                    }
                } else {
                    if (UNLIKELY(_md_update_claim(marks, iter.index) < 0)) {
                        return -1;
                    }
                    found = true;
                    PyObject* old_key = entry->key;
                    replace_compact_key(entry, Py_NewRef(key));
                    if (UNLIKELY(_md_update_store(st,
                                                  entry,
                                                  old_key,
                                                  hash,
                                                  identity,
                                                  key,
                                                  value) < 0)) {
                        return -1;
                    }
                }
                if (!md->keys->maybe_dups) {
                    break;  // the key's only entry
                }
            }
        } else {
            for (;;) {
                HTKEYSITER_FIND_COMPACT_CS(&iter, identity, hash, entry);
                if (entry == NULL) {
                    break;
                }
                if (bitmap_test(&marks->updated, iter.index)) {
                    added = true;
                    continue;
                }
                if (found) {
                    if (update_marks_doom(marks, iter.index, entry) < 0) {
                        return -1;
                    }
                } else {
                    if (_md_update_claim(marks, iter.index) < 0) {
                        return -1;
                    }
                    found = true;
                    PyObject* old_key = entry->key;
                    replace_compact_key(entry, Py_NewRef(key));
                    if (_md_update_store(
                            st, entry, old_key, hash, identity, key, value) <
                        0) {
                        return -1;
                    }
                }
                if (!md->keys->maybe_dups) {
                    break;  // the key's only entry
                }
            }
        }
    } else {
        entry_t* entry = NULL;
        for (;;) {
            HTKEYSITER_FIND_ANYSTR(&iter, identity, hash, entry);
            if (entry == NULL) {
                break;
            }
            if (bitmap_test(&marks->updated, iter.index)) {
                added = true;
                continue;
            }
            if (found) {
                if (update_marks_doom(marks, iter.index, entry) < 0) {
                    return -1;
                }
            } else {
                if (_md_update_claim(marks, iter.index) < 0) {
                    return -1;
                }
                found = true;
                PyObject* old_key = entry->key;
                replace_anystr_key(as_anystr(entry), Py_NewRef(key));
                if (_md_update_store(
                        st, entry, old_key, hash, identity, key, value) < 0) {
                    return -1;
                }
            }
            if (!md->keys->maybe_dups) {
                break;  // the key's only entry
            }
        }
    }
    if (found) {
        return 0;
    }
    return md_add_for_upd(md,
                          hash,
                          identity,
                          key,
                          value,
                          marks,
                          fits,
                          added ? MD_SLOT_CHECK : MD_SLOT_FIND);
}

/* Whether md has `identity` from before the batch: the insert's `slot`
   if not, MD_SLOT_FIND or, when this batch has added the key already,
   MD_SLOT_CHECK; MD_SLOT_PRESENT if so. One loop per kind. */
#define MD_SLOT_PRESENT ((Py_ssize_t) - 3)

static inline Py_ssize_t
_md_merge_present(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
                  update_marks_t* marks)
{
    bool ci = md->is_ci;
    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, md->keys, hash);
    Py_ssize_t slot = MD_SLOT_FIND;

    /* An entry this batch added doesn't count as already present. */
    if (kind_is_compact(md->keys->kind)) {
        entry_t* entry = NULL;
        if (ci) {
            for (;;) {
                HTKEYSITER_FIND_COMPACT_CI(&iter, identity, hash, entry);
                if (entry == NULL) {
                    return slot;
                }
                if (!bitmap_test(&marks->updated, iter.index)) {
                    return MD_SLOT_PRESENT;
                }
                slot = MD_SLOT_CHECK;
            }
        } else {
            for (;;) {
                HTKEYSITER_FIND_COMPACT_CS(&iter, identity, hash, entry);
                if (entry == NULL) {
                    return slot;
                }
                if (!bitmap_test(&marks->updated, iter.index)) {
                    return MD_SLOT_PRESENT;
                }
                slot = MD_SLOT_CHECK;
            }
        }
    } else {
        entry_t* entry = NULL;
        for (;;) {
            HTKEYSITER_FIND_ANYSTR(&iter, identity, hash, entry);
            if (entry == NULL) {
                return slot;
            }
            if (!bitmap_test(&marks->updated, iter.index)) {
                return MD_SLOT_PRESENT;
            }
            slot = MD_SLOT_CHECK;
        }
    }
}

COLD static int
_md_merge(md_update_state_t* st, Py_hash_t hash, PyObject* identity,
          PyObject* key, PyObject* value, bool fits)
{
    MultiDictObject* md = st->md;
    update_marks_t* marks = &st->marks;
    if (UNLIKELY(update_marks_sync(marks, md) < 0)) {
        return -1;
    }
    Py_ssize_t slot = _md_merge_present(md, hash, identity, marks);
    if (slot == MD_SLOT_PRESENT) {
        return 0;
    }
    return md_add_for_upd(md, hash, identity, key, value, marks, fits, slot);
}

#define _MD_POST_UPDATE_RESTART 2

/* The index of the i-th doomed entry if it is still to be deleted, which
   takes it off the set, else -1. */
static inline Py_ssize_t
_md_post_update_take(update_marks_t* marks, htkeys_t* keys, Py_ssize_t i)
{
    Py_ssize_t pos = marks->doomed[i].index;
    // revived by a later item of this batch, or handled before a restart
    if (!bitmap_test(&marks->deleted, pos)) {
        return -1;
    }
    assert(pos < keys->nentries);
    bitmap_clear(&marks->deleted, pos);
    return pos;
}

/* The slot of the live doomed entry at `pos` of md's table `keys`, its
   delete recorded for the watchers. */
static inline size_t
_md_post_update_slot(MultiDictObject* md, htkeys_t* keys, Py_ssize_t pos,
                     entry_t* entry, PyObject* identity, Py_hash_t hash)
{
    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, keys, hash);
    while (iter.index != pos) {
        assert(iter.index != DKIX_EMPTY);
        HTKEYSITER_NEXT(&iter);
    }
    MD_WATCH_RECORD(md,
                    MultiDict_EVENT_DELETED,
                    identity,
                    hash,
                    entry->key,
                    entry->value,
                    NULL);
    return iter.slot;
}

/* After a failed delete: -1, or _MD_POST_UPDATE_RESTART if it moved the
   table. */
static inline int
_md_post_update_failed(MultiDictObject* md, htkeys_t* keys, uint64_t version)
{
    if (md->keys != keys || md->version != version) {
        return _MD_POST_UPDATE_RESTART;
    }
    return -1;
}

/* _md_post_update_pass()'s loop over a KIND_COMPACT table, `sfx` being cs
   or ci as for compact_key_identity_cs(). */
#define _MD_POST_UPDATE_COMPACT_LOOP(sfx)                                    \
    do {                                                                     \
        entry_t* entries = HTKEYS_COMPACT_ENTRIES(keys);                     \
        for (Py_ssize_t i = 0; i < marks->ndoomed; i++) {                    \
            Py_ssize_t pos = _md_post_update_take(marks, keys, i);           \
            if (pos < 0) {                                                   \
                continue;                                                    \
            }                                                                \
            entry_t* entry = entries + pos;                                  \
            if (entry_is_hole(entry) ||                                      \
                load_value(entry) != marks->doomed[i].value) {               \
                continue;                                                    \
            }                                                                \
            size_t slot =                                                    \
                _md_post_update_slot(md,                                     \
                                     keys,                                   \
                                     pos,                                    \
                                     entry,                                  \
                                     compact_key_identity_##sfx(entry->key), \
                                     compact_key_hash_##sfx(entry->key));    \
            if (UNLIKELY(md_del_at_deferred_compact(                         \
                             md, slot, entry, &st->defer) < 0)) {            \
                ret = _md_post_update_failed(md, keys, version);             \
                if (ret == _MD_POST_UPDATE_RESTART) {                        \
                    return ret;                                              \
                }                                                            \
            }                                                                \
        }                                                                    \
    } while (0)

/* One pass of _md_post_update_deleted() over md's table, one loop per
   kind: 0 when done, -1 when done but a delete failed,
   _MD_POST_UPDATE_RESTART when a failed delete moved the table. */
static int
_md_post_update_pass(md_update_state_t* st)
{
    MultiDictObject* md = st->md;
    update_marks_t* marks = &st->marks;
    int ret = 0;
    htkeys_t* keys = md->keys;
    uint64_t version = md->version;
    // Python code run between items may have removed or rewritten each one
    if (kind_is_compact(keys->kind)) {
        if (md->is_ci) {
            _MD_POST_UPDATE_COMPACT_LOOP(ci);
        } else {
            _MD_POST_UPDATE_COMPACT_LOOP(cs);
        }
        return ret;
    }
    anystr_entry_t* entries = HTKEYS_ANYSTR_ENTRIES(keys);
    for (Py_ssize_t i = 0; i < marks->ndoomed; i++) {
        Py_ssize_t pos = _md_post_update_take(marks, keys, i);
        if (pos < 0) {
            continue;
        }
        anystr_entry_t* entry = entries + pos;
        if (entry_is_hole((const entry_t*)entry) ||
            load_value(&entry->base) != marks->doomed[i].value) {
            continue;
        }
        size_t slot = _md_post_update_slot(
            md, keys, pos, &entry->base, entry->identity, entry->hash);
        if (UNLIKELY(md_del_at_deferred_anystr(md, slot, entry, &st->defer) <
                     0)) {
            ret = _md_post_update_failed(md, keys, version);
            if (ret == _MD_POST_UPDATE_RESTART) {
                return ret;
            }
        }
    }
    return ret;
}

#undef _MD_POST_UPDATE_COMPACT_LOOP

/* Removes the entries update() doomed and nothing has written since. Only
   an out-of-memory fallback decref in md_del_at_deferred_compact() or
   md_del_at_deferred_anystr() can run Python here; the walk then starts
   over, which each record leaving the set as it goes makes safe. */
COLD static int
_md_post_update_deleted(md_update_state_t* st)
{
    int ret = 0;
    int pass;
    do {
        if (UNLIKELY(update_marks_sync(&st->marks, st->md) < 0)) {
            return -1;
        }
        pass = _md_post_update_pass(st);
        if (pass != 0) {
            ret = -1;
        }
    } while (pass == _MD_POST_UPDATE_RESTART);
    return ret;
}

#undef _MD_POST_UPDATE_RESTART

/* Ends the batch; the caller holds md's critical section. */
static int
md_post_update(md_update_state_t* st)
{
    MultiDictObject* md = st->md;
    int ret = 0;
    // merge() never deletes
    if (st->action == _md_update) {
        ret = _md_post_update_deleted(st);
    }
    update_marks_end(md);
    bump_version(md);
    MD_WATCH_RECORD_SIMPLE(md, MultiDict_EVENT_BATCH_END);
    ASSERT_CONSISTENT(md);
    return ret;
}

static inline void
md_update_state_init(md_update_state_t* st, MultiDictObject* md, UpdateOp op)
{
    assert(op != Extend);
    st->md = md;
    st->action = op == Update ? _md_update : _md_merge;
    reflist_init(&st->defer);
}

static inline void
md_update_state_release(md_update_state_t* st)
{
    update_marks_release(&st->marks);
    reflist_clear(&st->defer);
}

/* One item of an update() or merge() from a multidict of md's class, as
   an md_item_visitor_t: `identity` and `hash` are md's own. */
static int
_md_update_visit_own(void* user_data, PyObject* identity, Py_hash_t hash,
                     PyObject* key, PyObject* value)
{
    md_update_state_t* st = (md_update_state_t*)user_data;
    // from another table, so not known from the identity
    bool fits = md_key_fits(st->md, key, identity);
    return st->action(st, hash, identity, key, value, fits) < 0 ? -1 : 1;
}

/* One item of an update() or merge(); identity and hash are computed from
   `key`. */
static int
_md_update_key(md_update_state_t* st, PyObject* key, PyObject* value)
{
    MultiDictObject* md = st->md;
    PyObject* identity = md_identity_or_new(md, key);
    if (identity == NULL) {
        return -1;
    }
    int ret = -1;
    Py_hash_t hash = unicode_hash(identity);
    if (hash != -1) {
        ret = st->action(
            st, hash, identity, key, value, md_key_fits(md, key, identity));
    }
    md_identity_release(md, key, identity);
    return ret < 0 ? -1 : 0;
}

/* _md_update_key() as an md_item_visitor_t: `identity` and `hash` are
   other's, so ignored. */
static int
_md_update_visit(void* user_data, PyObject* identity, Py_hash_t hash,
                 PyObject* key, PyObject* value)
{
    md_update_state_t* st = (md_update_state_t*)user_data;
    return _md_update_key(st, key, value) < 0 ? -1 : 1;
}

static int
md_update_from_ht(md_update_state_t* st, MultiDictObject* other)
{
    if (other->used == 0) {
        return 0;
    }

    // callers handle md itself: a no-op
    assert(st->md != other);

    /* The keys come materialized as other's: a CIMultiDict's istr keeps
       other's identity as its canonical, md's is the unlowered key. */
    md_item_visitor_t visit = st->md->is_ci == other->is_ci
                                  ? _md_update_visit_own
                                  : _md_update_visit;
    return md_walk_all(other, true, visit, st) < 0 ? -1 : 0;
}

/* One extend() item from a source that hands over `key` and `value`,
   which go to md or are released. */
static int
_md_extend_item(MultiDictObject* md, PyObject* key, PyObject* value,
                Py_ssize_t slot)
{
    bool fits;
    PyObject* identity = md_calc_identity_fits(md, key, &fits);
    if (identity == NULL) {
        goto fail;
    }
    Py_hash_t hash = unicode_hash(identity);
    if (hash == -1) {
        goto fail_identity;
    }
    if (md_add_with_hash_steal_refs(
            md, hash, identity, key, value, fits, slot) < 0) {
        goto fail_identity;
    }
    return 0;
fail_identity:
    Py_DECREF(identity);
fail:
    Py_DECREF(key);
    Py_DECREF(value);
    return -1;
}

/* MD_SLOT_FIND if distinct exact str keys stay unique in md: it is an
   empty MultiDict. */
static inline Py_ssize_t
_md_extend_slot(MultiDictObject* md)
{
    return !md->is_ci && md->used == 0 ? MD_SLOT_FIND : MD_SLOT_CHECK;
}

// a str subclass's own __eq__ can make two keys equal
static inline Py_ssize_t
_md_extend_key_slot(Py_ssize_t slot, PyObject* key)
{
    return PyUnicode_CheckExact(key) ? slot : MD_SLOT_CHECK;
}

/* md_extend_from_ht()'s visitors, `user_data` being md: one for a
   multidict of md's class, whose identities and hashes are md's own, and
   one with md's slot for keys known to be new. */
static inline int
_md_extend_own(void* user_data, PyObject* identity, Py_hash_t hash,
               PyObject* key, PyObject* value, Py_ssize_t slot)
{
    MultiDictObject* md = (MultiDictObject*)user_data;
    // from another table, so not known from the identity
    bool fits = md_key_fits(md, key, identity);
    return md_add_with_hash(md, hash, identity, key, value, fits, slot) < 0
               ? -1
               : 1;
}

static int
_md_extend_visit_own(void* user_data, PyObject* identity, Py_hash_t hash,
                     PyObject* key, PyObject* value)
{
    return _md_extend_own(
        user_data, identity, hash, key, value, MD_SLOT_CHECK);
}

static int
_md_extend_visit_new(void* user_data, PyObject* identity, Py_hash_t hash,
                     PyObject* key, PyObject* value)
{
    return _md_extend_own(user_data, identity, hash, key, value, MD_SLOT_FIND);
}

// md_extend_from_ht()'s visitor for a multidict of the other class
static int
_md_extend_visit(void* user_data, PyObject* identity, Py_hash_t hash,
                 PyObject* key, PyObject* value)
{
    return _md_extend_item((MultiDictObject*)user_data,
                           Py_NewRef(key),
                           Py_NewRef(value),
                           MD_SLOT_CHECK) < 0
               ? -1
               : 1;
}

static int
md_extend_from_ht(MultiDictObject* md, MultiDictObject* other)
{
    if (other->used == 0) {
        return 0;
    }

    // callers handle md itself: md_extend_self()
    assert(md != other);

    if (UNLIKELY(md_reserve(md, other->used) < 0)) {
        return -1;
    }

    /* The keys come materialized as other's: a CIMultiDict's istr keeps
       other's identity as its canonical, md's is the unlowered key. */
    md_item_visitor_t visit = _md_extend_visit;
    /* Keys repeated in other repeat here. Unique ones stay unique in an
       empty MultiDict; a CIMultiDict's walk can build an istr, which can
       run a collection whose finalizers add to md. */
    if (md->is_ci == other->is_ci) {
        visit = _md_extend_visit_own;
        if (other->keys->maybe_dups) {
            htkeys_set_maybe_dups(md->keys);
        } else if (_md_extend_slot(md) == MD_SLOT_FIND) {
            visit = _md_extend_visit_new;
        }
    }
    return md_walk_all(other, true, visit, md) < 0 ? -1 : 0;
}

/* md_extend_self()'s loop over a KIND_COMPACT table, `sfx` being cs or ci
   as for compact_key_identity_cs(). */
#define _MD_EXTEND_SELF_COMPACT_LOOP(sfx)                                   \
    do {                                                                    \
        entry_t* entries = HTKEYS_COMPACT_ENTRIES(keys);                    \
        for (Py_ssize_t pos = 0; pos < nentries; pos++) {                   \
            entry_t* entry = entries + pos;                                 \
            if (entry_is_hole(entry)) {                                     \
                continue;                                                   \
            }                                                               \
            PyObject* identity = compact_key_identity_##sfx(entry->key);    \
            if (UNLIKELY(                                                   \
                    md_add_with_hash(md,                                    \
                                     compact_key_hash_##sfx(entry->key),    \
                                     identity,                              \
                                     entry->key,                            \
                                     entry->value,                          \
                                     md_key_fits(md, entry->key, identity), \
                                     MD_SLOT_CHECK) < 0)) {                 \
                return -1;                                                  \
            }                                                               \
            assert(md->keys == keys);                                       \
        }                                                                   \
    } while (0)

/* d.extend(d) is rare. The loops walk the table they add to: md_reserve()
   leaves room for every entry and md's own keys always fit, so it is
   never replaced, which the kind chosen up front relies on. */
static int
md_extend_self(MultiDictObject* md)
{
    if (UNLIKELY(md_reserve(md, md->keys->nentries) < 0)) {
        return -1;
    }
    if (md->used > 0) {
        htkeys_set_maybe_dups(md->keys);  // every key gets a second entry
    }
    htkeys_t* keys = md->keys;
    Py_ssize_t nentries = keys->nentries;
    if (kind_is_compact(keys->kind)) {
        if (md->is_ci) {
            _MD_EXTEND_SELF_COMPACT_LOOP(ci);
        } else {
            _MD_EXTEND_SELF_COMPACT_LOOP(cs);
        }
        return 0;
    }
    anystr_entry_t* entries = HTKEYS_ANYSTR_ENTRIES(keys);
    for (Py_ssize_t pos = 0; pos < nentries; pos++) {
        anystr_entry_t* entry = entries + pos;
        if (entry_is_hole((const entry_t*)entry)) {
            continue;
        }
        if (UNLIKELY(md_add_with_hash(
                         md,
                         entry->hash,
                         entry->identity,
                         entry->base.key,
                         entry->base.value,
                         md_key_fits(md, entry->base.key, entry->identity),
                         MD_SLOT_CHECK) < 0)) {
            return -1;
        }
        assert(md->keys == keys);
    }
    return 0;
}

#undef _MD_EXTEND_SELF_COMPACT_LOOP

static int
md_update_from_dict(md_update_state_t* st, PyObject* kwds)
{
    Py_ssize_t pos = 0;
    PyObject* key = NULL;
    PyObject* value = NULL;

    assert(PyDict_CheckExact(kwds));
    // PyDict_Next returns borrowed refs, which kwds keeps alive
    while (PyDict_Next(kwds, &pos, &key, &value)) {
        if (UNLIKELY(_md_update_key(st, key, value) < 0)) {
            return -1;
        }
    }
    return 0;
}

static int
md_extend_from_dict(MultiDictObject* md, PyObject* kwds)
{
    Py_ssize_t pos = 0;
    PyObject* key = NULL;
    PyObject* value = NULL;

    assert(PyDict_CheckExact(kwds));
    Py_ssize_t slot = _md_extend_slot(md);
    // PyDict_Next returns borrowed refs, which kwds keeps alive
    while (PyDict_Next(kwds, &pos, &key, &value)) {
        slot = _md_extend_key_slot(slot, key);
        if (UNLIKELY(_md_extend_item(
                         md, Py_NewRef(key), Py_NewRef(value), slot) < 0)) {
            return -1;
        }
    }
    return 0;
}

static int
md_extend_from_kwnames(MultiDictObject* md, PyObject* const* args,
                       Py_ssize_t nargs, PyObject* kwnames)
{
    Py_ssize_t nkwargs = PyTuple_GET_SIZE(kwnames);
    Py_ssize_t slot = _md_extend_slot(md);
    if (UNLIKELY(md_reserve(md, nkwargs) < 0)) {
        return -1;
    }
    for (Py_ssize_t i = 0; i < nkwargs; i++) {
        PyObject* key = PyTuple_GET_ITEM(kwnames, i);  // borrowed
        assert(PyUnicode_Check(key));
        slot = _md_extend_key_slot(slot, key);
        if (UNLIKELY(_md_extend_item(md,
                                     Py_NewRef(key),
                                     Py_NewRef(args[nargs + i]),
                                     slot) < 0)) {
            return -1;
        }
    }
    return 0;
}

COLD static void
_err_not_sequence(Py_ssize_t i)
{
    PyErr_Format(PyExc_TypeError,
                 "multidict cannot convert sequence element #%zd"
                 " to a sequence",
                 i);
}

COLD static void
_err_bad_length(Py_ssize_t i, Py_ssize_t n)
{
    PyErr_Format(PyExc_ValueError,
                 "multidict update sequence element #%zd "
                 "has length %zd; 2 is required",
                 i,
                 n);
}

COLD static void
_err_cannot_fetch(Py_ssize_t i, const char* name)
{
    PyErr_Format(PyExc_ValueError,
                 "multidict update sequence element #%zd's "
                 "%s could not be fetched",
                 i,
                 name);
}

static inline int
_md_parse_item(Py_ssize_t i, PyObject* item, PyObject** pkey,
               PyObject** pvalue)
{
    Py_ssize_t n;

    switch (unpack_pair(item, pkey, pvalue, &n)) {
        case UNPACK_OK:
            return 0;
        case UNPACK_LENGTH:
            _err_bad_length(i, n);
            goto fail;
        case UNPACK_ERROR:
            goto fail;
        case UNPACK_OTHER:
            break;
    }

    if (UNLIKELY(!PySequence_Check(item))) {
        _err_not_sequence(i);
        goto fail;
    }
    n = PySequence_Size(item);
    if (n != 2) {
        _err_bad_length(i, n);
        goto fail;
    }
    *pkey = PySequence_ITEM(item, 0);
    if (UNLIKELY(*pkey == NULL)) {
        _err_cannot_fetch(i, "key");
        goto fail;
    }
    *pvalue = PySequence_ITEM(item, 1);
    if (UNLIKELY(*pvalue == NULL)) {
        _err_cannot_fetch(i, "value");
        goto fail;
    }
    return 0;
fail:
    Py_CLEAR(*pkey);
    Py_CLEAR(*pvalue);
    return -1;
}

typedef enum { SEQ_LIST, SEQ_TUPLE, SEQ_ITER } seq_kind_t;

typedef struct {
    seq_kind_t kind;
    PyObject* obj;    // owned list, tuple or iterator
    Py_ssize_t size;  // SEQ_TUPLE only
} seq_iter_t;

typedef struct {
    PyObject* pair;  // seq[i], freed after the operation like in pure Python
    PyObject* key;
    PyObject* value;
} seq_item_t;

static int
_md_seq_prepare(PyObject* seq, seq_iter_t* it)
{
    PyObject* items = NULL;

    if (!PyList_CheckExact(seq) && !PyTuple_CheckExact(seq)) {
        items = PyMapping_Items(seq);
        if (items != NULL) {
            seq = items;
        } else {
            if (!PyErr_ExceptionMatches(PyExc_AttributeError) &&
                !PyErr_ExceptionMatches(PyExc_TypeError)) {
                // propagate MemoryError / KeyboardInterrupt / etc.
                return -1;
            }
            // seq is not a mapping; fall back to treating it as a sequence
            PyErr_Clear();
        }
    }

    if (PyList_CheckExact(seq)) {
        it->kind = SEQ_LIST;
        it->obj = items != NULL ? items : Py_NewRef(seq);
    } else if (PyTuple_CheckExact(seq)) {
        it->kind = SEQ_TUPLE;
        it->size = PyTuple_GET_SIZE(seq);
        it->obj = items != NULL ? items : Py_NewRef(seq);
    } else {
        it->kind = SEQ_ITER;
        it->obj = PyObject_GetIter(seq);
        Py_XDECREF(items);
        if (UNLIKELY(it->obj == NULL)) {
            return -1;
        }
    }
    return 0;
}

static int
_md_seq_next(seq_iter_t* it, Py_ssize_t i, seq_item_t* out)
{
    PyObject* item = NULL;

    switch (it->kind) {
        case SEQ_LIST:
            /* Re-read the length every iteration.  Parsing an item, or
               dropping it afterwards, can run arbitrary Python (an item's
               __getitem__(), a __del__), which may shrink seq; a stale cached
               size would let PyList_GET_ITEM read past the end. */
            if (i >= PyList_GET_SIZE(it->obj)) {
                return 0;
            }
            item = list_getitem_ref(it->obj, i);
            if (LIST_ITEM_GONE(item)) {
                return -1;
            }
            break;
        case SEQ_TUPLE:
            if (i >= it->size) {
                return 0;
            }
            item = PyTuple_GET_ITEM(it->obj, i);
            if (UNLIKELY(item == NULL)) {
                return -1;
            }
            Py_INCREF(item);
            break;
        case SEQ_ITER: {
            int res = PyIter_NextItem(it->obj, &item);
            if (res <= 0) {
                return res;
            }
            break;
        }
    }

    out->pair = item;
    out->key = NULL;
    out->value = NULL;
    if (UNLIKELY(_md_parse_item(i, item, &out->key, &out->value) < 0)) {
        Py_DECREF(item);
        return -1;
    }
    return 1;
}

/* The one loop over a sequence's pairs: an update() or merge() with
   `st`, an extend() of md without. */
static int
_md_seq_walk(MultiDictObject* md, md_update_state_t* st, PyObject* seq)
{
    seq_iter_t it;
    seq_item_t item;
    int ret;

    if (UNLIKELY(_md_seq_prepare(seq, &it) < 0)) {
        return -1;
    }
    for (Py_ssize_t i = 0;; ++i) {
        ret = _md_seq_next(&it, i, &item);
        if (ret <= 0) {
            break;
        }
        if (st == NULL) {
            ret = _md_extend_item(md, item.key, item.value, MD_SLOT_CHECK);
        } else {
            ret = _md_update_key(st, item.key, item.value);
            Py_DECREF(item.key);
            Py_DECREF(item.value);
        }
        Py_DECREF(item.pair);
        if (UNLIKELY(ret < 0)) {
            break;
        }
    }
    Py_DECREF(it.obj);
    return ret;
}

static int
md_update_from_seq(md_update_state_t* st, PyObject* seq)
{
    return _md_seq_walk(st->md, st, seq);
}

static int
md_extend_from_seq(MultiDictObject* md, PyObject* seq)
{
    return _md_seq_walk(md, NULL, seq);
}
#ifdef __cplusplus
}
#endif
#endif
