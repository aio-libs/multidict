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

/* What a batch does with one item: _md_extend(), _md_update() or
   _md_merge(). */
typedef int (*md_update_action_t)(md_update_state_t* st, Py_hash_t hash,
                                  PyObject* identity, PyObject* key,
                                  PyObject* value, bool fits);

/* One extend(), update() or merge() batch, made by md_update_state().
   `defer` is NULL except for update(), `marks` NULL for extend(). */
struct _md_update_state {
    MultiDictObject* md;
    UpdateOp op;
    md_update_action_t action;
    // as `action`, but owns `identity`, `key` and `value`, even on failure
    md_update_action_t steal;
    reflist_t* defer;
    update_marks_t* marks;
    Py_ssize_t slot;  // _md_extend()'s, set by _md_update_source()
};

/* An entry on _md_update()'s chain with the key, not written by this
   batch yet, at `index`: the first gets the new key and value, the rest
   are doomed. -1 on error. */
static inline int
_md_update_matched(md_update_state_t* st, uint8_t kind, entry_t* entry,
                   Py_ssize_t index, bool* pfound, Py_hash_t hash,
                   PyObject* identity, PyObject* key, PyObject* value)
{
    MultiDictObject* md = st->md;
    reflist_t* defer = st->defer;
    update_marks_t* marks = st->marks;
    if (*pfound) {
        return update_marks_doom(marks, index, entry);
    }
    *pfound = true;
    /* Marked first: nothing below can fail half-way after the entry has
       changed. An entry an earlier item of this batch doomed is reused,
       which keeps the key at its position. */
    if (bitmap_set(&marks->updated, index) < 0) {
        return -1;
    }
    bitmap_clear(&marks->deleted, index);
    // old_key/old_value decref deferred: see reflist_t
    PyObject* old_key = entry->key;
    PyObject* old_value = load_value(entry);
    if (kind_is_compact(kind)) {
        replace_compact_key(entry, Py_NewRef(key));
    } else {
        replace_anystr_key(as_anystr(entry), Py_NewRef(key));
    }
    publish_value(entry, Py_NewRef(value));
    md_watch_record(
        md, MultiDict_EVENT_REPLACED, identity, hash, key, value, old_value);
    /* Push both unconditionally, not with `||`: a failed first push
       already decref'd old_key itself (see reflist_push()'s doc comment),
       but short-circuiting past the second push would leak old_value --
       neither deferred nor decref'd. */
    int push_ret = reflist_push(defer, old_key);
    if (reflist_push(defer, old_value) < 0) {
        push_ret = -1;
    }
    return push_ret;
}

/* Nothing here runs Python code or suspends the critical section: the
 * replaced key and value go to `defer`. One loop per kind. */
static int
_md_update(md_update_state_t* st, Py_hash_t hash, PyObject* identity,
           PyObject* key, PyObject* value, bool fits)
{
    MultiDictObject* md = st->md;
    update_marks_t* marks = st->marks;
    assert(fits == md_key_fits(md, key, identity));
    if (kind_is_compact(md->keys->kind) && UNLIKELY(!fits) &&
        md_to_anystr(md) < 0) {
        return -1;
    }
    if (update_marks_sync(marks, md) < 0) {
        return -1;
    }
    bool ci = md->is_ci;
    bool found = false;
    bool added = false;
    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, md->keys, hash);

    entry_t* entry = NULL;
    if (kind_is_compact(md->keys->kind) && ci) {
        for (;;) {
            HTKEYSITER_FIND_COMPACT_CI(&iter, identity, hash, entry);
            if (entry == NULL) {
                break;
            }
            if (bitmap_test(&marks->updated, iter.index)) {
                added = true;
                continue;
            }
            if (_md_update_matched(st,
                                   KIND_COMPACT,
                                   entry,
                                   iter.index,
                                   &found,
                                   hash,
                                   identity,
                                   key,
                                   value) < 0) {
                return -1;
            }
            if (!md->keys->maybe_dups) {
                break;  // the key's only entry
            }
        }
    } else if (kind_is_compact(md->keys->kind)) {
        for (;;) {
            HTKEYSITER_FIND_COMPACT_CS(&iter, identity, hash, entry);
            if (entry == NULL) {
                break;
            }
            if (bitmap_test(&marks->updated, iter.index)) {
                added = true;
                continue;
            }
            if (_md_update_matched(st,
                                   KIND_COMPACT,
                                   entry,
                                   iter.index,
                                   &found,
                                   hash,
                                   identity,
                                   key,
                                   value) < 0) {
                return -1;
            }
            if (!md->keys->maybe_dups) {
                break;  // the key's only entry
            }
        }
    } else {
        for (;;) {
            HTKEYSITER_FIND_ANYSTR(&iter, identity, hash, entry);
            if (entry == NULL) {
                break;
            }
            if (bitmap_test(&marks->updated, iter.index)) {
                added = true;
                continue;
            }
            if (_md_update_matched(st,
                                   KIND_ANYSTR,
                                   entry,
                                   iter.index,
                                   &found,
                                   hash,
                                   identity,
                                   key,
                                   value) < 0) {
                return -1;
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
    entry_t* entry = NULL;
    if (kind_is_compact(md->keys->kind) && ci) {
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
    } else if (kind_is_compact(md->keys->kind)) {
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

COLD static int
_md_merge(md_update_state_t* st, Py_hash_t hash, PyObject* identity,
          PyObject* key, PyObject* value, bool fits)
{
    MultiDictObject* md = st->md;
    update_marks_t* marks = st->marks;
    if (update_marks_sync(marks, md) < 0) {
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

/* Deletes the live doomed entry at `pos` of md's table `keys`: 0 on
   success, -1 if the delete failed,
   _MD_POST_UPDATE_RESTART if it failed and moved the table. */
static inline int
_md_post_update_delete(md_update_state_t* st, htkeys_t* keys, Py_ssize_t pos,
                       entry_t* entry, PyObject* identity, Py_hash_t hash,
                       uint64_t version)
{
    MultiDictObject* md = st->md;
    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, keys, hash);
    while (iter.index != pos) {
        assert(iter.index != DKIX_EMPTY);
        HTKEYSITER_NEXT(&iter);
    }
    md_watch_record(md,
                    MultiDict_EVENT_DELETED,
                    identity,
                    hash,
                    entry->key,
                    entry->value,
                    NULL);
    if (md_del_at_deferred(md, iter.slot, entry, st->defer) < 0) {
        if (md->keys != keys || md->version != version) {
            return _MD_POST_UPDATE_RESTART;
        }
        return -1;
    }
    return 0;
}

/* _md_post_update_pass()'s loop over a KIND_COMPACT table, `sfx` being cs
   or ci as for compact_entry_identity_cs(). */
#define _MD_POST_UPDATE_COMPACT_LOOP(sfx)                                   \
    do {                                                                    \
        entry_t* entries = HTKEYS_COMPACT_ENTRIES(keys);                    \
        for (Py_ssize_t i = 0; i < marks->ndoomed; i++) {                   \
            Py_ssize_t pos = _md_post_update_take(marks, keys, i);          \
            if (pos < 0) {                                                  \
                continue;                                                   \
            }                                                               \
            entry_t* entry = entries + pos;                                 \
            if (compact_entry_is_hole(entry) ||                             \
                load_value(entry) != marks->doomed[i].value) {              \
                continue;                                                   \
            }                                                               \
            int del =                                                       \
                _md_post_update_delete(st,                                  \
                                       keys,                                \
                                       pos,                                 \
                                       entry,                               \
                                       compact_entry_identity_##sfx(entry), \
                                       compact_entry_hash_##sfx(entry),     \
                                       version);                            \
            if (del == _MD_POST_UPDATE_RESTART) {                           \
                return del;                                                 \
            }                                                               \
            if (del < 0) {                                                  \
                ret = -1;                                                   \
            }                                                               \
        }                                                                   \
    } while (0)

/* One pass of _md_post_update_deleted() over md's table, one loop per
   kind: 0 when done, -1 when done but a delete failed,
   _MD_POST_UPDATE_RESTART when a failed delete moved the table. */
static int
_md_post_update_pass(md_update_state_t* st)
{
    MultiDictObject* md = st->md;
    update_marks_t* marks = st->marks;
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
        if (anystr_entry_is_hole(entry) ||
            load_value(&entry->base) != marks->doomed[i].value) {
            continue;
        }
        int del = _md_post_update_delete(st,
                                         keys,
                                         pos,
                                         &entry->base,
                                         entry->identity,
                                         entry->hash,
                                         version);
        if (del == _MD_POST_UPDATE_RESTART) {
            return del;
        }
        if (del < 0) {
            ret = -1;
        }
    }
    return ret;
}

#undef _MD_POST_UPDATE_COMPACT_LOOP

/* Removes the entries update() doomed and nothing has written since. Only
   an out-of-memory fallback decref in md_del_at_deferred() can run Python
   here; the walk then starts over, which each record leaving the set as it
   goes makes safe. */
COLD static int
_md_post_update_deleted(md_update_state_t* st)
{
    int ret = 0;
    int pass;
    do {
        if (update_marks_sync(st->marks, st->md) < 0) {
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
    /* `defer` is NULL only for merge(), which never deletes. */
    if (st->defer != NULL) {
        ret = _md_post_update_deleted(st);
    }
    update_marks_end(md);
    bump_version(md);
    md_watch_record_simple(md, MultiDict_EVENT_BATCH_END);
    ASSERT_CONSISTENT(md);
    return ret;
}

static int
_md_extend_steal(md_update_state_t* st, Py_hash_t hash, PyObject* identity,
                 PyObject* key, PyObject* value, bool fits)
{
    // a str subclass's own __eq__ can make two keys equal
    if (st->slot == MD_SLOT_FIND && !PyUnicode_CheckExact(key)) {
        st->slot = MD_SLOT_CHECK;
    }
    if (md_add_with_hash_steal_refs(
            st->md, hash, identity, key, value, fits, st->slot) < 0) {
        Py_DECREF(identity);
        Py_DECREF(key);
        Py_DECREF(value);
        return -1;
    }
    return 0;
}

static int
_md_extend(md_update_state_t* st, Py_hash_t hash, PyObject* identity,
           PyObject* key, PyObject* value, bool fits)
{
    Py_INCREF(identity);
    Py_INCREF(key);
    Py_INCREF(value);
    return _md_extend_steal(st, hash, identity, key, value, fits);
}

static int
_md_update_steal(md_update_state_t* st, Py_hash_t hash, PyObject* identity,
                 PyObject* key, PyObject* value, bool fits)
{
    int ret = _md_update(st, hash, identity, key, value, fits);
    Py_DECREF(identity);
    Py_DECREF(key);
    Py_DECREF(value);
    return ret;
}

static int
_md_merge_steal(md_update_state_t* st, Py_hash_t hash, PyObject* identity,
                PyObject* key, PyObject* value, bool fits)
{
    int ret = _md_merge(st, hash, identity, key, value, fits);
    Py_DECREF(identity);
    Py_DECREF(key);
    Py_DECREF(value);
    return ret;
}

static inline md_update_state_t
md_update_state(MultiDictObject* md, UpdateOp op, reflist_t* defer,
                update_marks_t* marks)
{
    md_update_action_t action = NULL;
    md_update_action_t steal = NULL;
    switch (op) {
        case Extend:
            action = _md_extend;
            steal = _md_extend_steal;
            break;
        case Update:
            action = _md_update;
            steal = _md_update_steal;
            break;
        case Merge:
            action = _md_merge;
            steal = _md_merge_steal;
            break;
    }
    md_update_state_t st = {.md = md,
                            .op = op,
                            .action = action,
                            .steal = steal,
                            .defer = defer,
                            .marks = marks};
    return st;
}

/* One item of an update(), extend() or merge() from a multidict of md's
   class, as an md_item_visitor_t: `identity` and `hash` are md's own. */
static int
_md_update_visit_own(void* user_data, PyObject* identity, Py_hash_t hash,
                     PyObject* key, PyObject* value)
{
    md_update_state_t* st = (md_update_state_t*)user_data;
    // from another table, so not known from the identity
    bool fits = md_key_fits(st->md, key, identity);
    return st->action(st, hash, identity, key, value, fits) < 0 ? -1 : 1;
}

/* One item of an update(), extend() or merge(), as an md_item_visitor_t:
   `identity` and `hash` are ignored and computed from `key`. */
static inline int
_md_update_visit(void* user_data, PyObject* identity, Py_hash_t hash,
                 PyObject* key, PyObject* value)
{
    md_update_state_t* st = (md_update_state_t*)user_data;
    bool fits;
    PyObject* own = md_calc_identity_fits(st->md, key, &fits);
    if (own == NULL) {
        return -1;
    }
    int ret = -1;
    hash = unicode_hash(own);
    if (hash != -1) {
        ret = st->action(st, hash, own, key, value, fits);
    }
    Py_DECREF(own);
    return ret < 0 ? -1 : 1;
}

/* As _md_update_visit() for a source that hands over its own `key` and
   `value`, which go to md or are released. */
static inline int
_md_update_visit_steal(md_update_state_t* st, PyObject* key, PyObject* value)
{
    bool fits;
    PyObject* own = md_calc_identity_fits(st->md, key, &fits);
    if (own == NULL) {
        goto fail;
    }
    Py_hash_t hash = unicode_hash(own);
    if (hash == -1) {
        Py_DECREF(own);
        goto fail;
    }
    return st->steal(st, hash, own, key, value, fits);
fail:
    Py_DECREF(key);
    Py_DECREF(value);
    return -1;
}

/* A copy of `st` for one source: `slot` is MD_SLOT_FIND when its distinct
   exact str keys stay unique in md, an empty MultiDict. */
static inline md_update_state_t
_md_update_source(md_update_state_t* st, bool unique)
{
    md_update_state_t item = *st;
    item.slot = unique && st->md->used == 0 && !st->md->is_ci ? MD_SLOT_FIND
                                                              : MD_SLOT_CHECK;
    return item;
}

static int
md_update_from_ht(md_update_state_t* st, MultiDictObject* other)
{
    MultiDictObject* md = st->md;
    UpdateOp op = st->op;
    if (other->used == 0) {
        return 0;
    }

    // callers handle md itself: md_extend_self(), or a no-op
    assert(md != other);

    // update() and merge() reserved as md_reserve_batch() does for them
    if (op == Extend && md_reserve(md, other->used) < 0) {
        return -1;
    }

    /* The keys come materialized as other's: a CIMultiDict's istr keeps
       other's identity as its canonical, md's is the unlowered key. */
    bool same_class = md->is_ci == other->is_ci;
    /* Keys repeated in other repeat here. Unique ones stay unique in an
       empty MultiDict; a CIMultiDict's walk can build an istr, which can
       run a collection whose finalizers add to md. */
    bool unique = false;
    if (op == Extend && same_class) {
        if (other->keys->maybe_dups) {
            md->keys->maybe_dups = 1;
        } else {
            unique = true;
        }
    }
    md_update_state_t item = _md_update_source(st, unique);
    md_item_visitor_t visit =
        same_class ? _md_update_visit_own : _md_update_visit;
    return md_walk_all(other, true, visit, &item) < 0 ? -1 : 0;
}

/* md_extend_self()'s loop over a KIND_COMPACT table, `sfx` being cs or ci
   as for compact_entry_identity_cs(). */
#define _MD_EXTEND_SELF_COMPACT_LOOP(sfx)                               \
    do {                                                                \
        entry_t* entries = HTKEYS_COMPACT_ENTRIES(keys);                \
        for (Py_ssize_t pos = 0; pos < nentries; pos++) {               \
            entry_t* entry = entries + pos;                             \
            if (compact_entry_is_hole(entry)) {                         \
                continue;                                               \
            }                                                           \
            PyObject* identity = compact_entry_identity_##sfx(entry);   \
            if (md_add_with_hash(md,                                    \
                                 compact_entry_hash_##sfx(entry),       \
                                 identity,                              \
                                 entry->key,                            \
                                 entry->value,                          \
                                 md_key_fits(md, entry->key, identity), \
                                 MD_SLOT_CHECK) < 0) {                  \
                return -1;                                              \
            }                                                           \
            assert(md->keys == keys);                                   \
        }                                                               \
    } while (0)

/* d.extend(d) is rare. The loops walk the table they add to: md_reserve()
   leaves room for every entry and md's own keys always fit, so it is
   never replaced, which the kind chosen up front relies on. */
static int
md_extend_self(MultiDictObject* md)
{
    if (md_reserve(md, md->keys->nentries) < 0) {
        return -1;
    }
    if (md->used > 0) {
        md->keys->maybe_dups = 1;  // every key gets a second entry
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
        if (anystr_entry_is_hole(entry)) {
            continue;
        }
        if (md_add_with_hash(md,
                             entry->hash,
                             entry->identity,
                             entry->base.key,
                             entry->base.value,
                             md_key_fits(md, entry->base.key, entry->identity),
                             MD_SLOT_CHECK) < 0) {
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
    md_update_state_t item = _md_update_source(st, true);
    // PyDict_Next returns borrowed refs, which kwds keeps alive
    while (PyDict_Next(kwds, &pos, &key, &value)) {
        if (_md_update_visit(&item, NULL, -1, key, value) < 0) {
            return -1;
        }
    }
    return 0;
}

static int
md_update_from_kwnames(md_update_state_t* st, PyObject* const* args,
                       Py_ssize_t nargs, PyObject* kwnames)
{
    Py_ssize_t nkwargs = PyTuple_GET_SIZE(kwnames);
    assert(st->op == Extend);
    if (md_reserve(st->md, nkwargs) < 0) {
        return -1;
    }
    md_update_state_t item = _md_update_source(st, true);
    for (Py_ssize_t i = 0; i < nkwargs; i++) {
        PyObject* key = PyTuple_GET_ITEM(kwnames, i);  // borrowed
        assert(PyUnicode_Check(key));
        if (_md_update_visit_steal(
                &item, Py_NewRef(key), Py_NewRef(args[nargs + i])) < 0) {
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

    if (!PySequence_Check(item)) {
        _err_not_sequence(i);
        goto fail;
    }
    n = PySequence_Size(item);
    if (n != 2) {
        _err_bad_length(i, n);
        goto fail;
    }
    *pkey = PySequence_ITEM(item, 0);
    if (*pkey == NULL) {
        _err_cannot_fetch(i, "key");
        goto fail;
    }
    *pvalue = PySequence_ITEM(item, 1);
    if (*pvalue == NULL) {
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
        if (it->obj == NULL) {
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
            if (_list_item_gone(item)) {
                return -1;
            }
            break;
        case SEQ_TUPLE:
            if (i >= it->size) {
                return 0;
            }
            item = PyTuple_GET_ITEM(it->obj, i);
            if (item == NULL) {
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
    if (_md_parse_item(i, item, &out->key, &out->value) < 0) {
        Py_DECREF(item);
        return -1;
    }
    return 1;
}

static int
md_update_from_seq(md_update_state_t* st, PyObject* seq)
{
    seq_iter_t it;
    seq_item_t item;
    int ret;

    if (_md_seq_prepare(seq, &it) < 0) {
        return -1;
    }
    md_update_state_t source = _md_update_source(st, false);
    for (Py_ssize_t i = 0;; ++i) {
        ret = _md_seq_next(&it, i, &item);
        if (ret <= 0) {
            break;
        }
        ret = _md_update_visit_steal(&source, item.key, item.value);
        Py_DECREF(item.pair);
        if (ret < 0) {
            break;
        }
    }
    Py_DECREF(it.obj);
    return ret < 0 ? -1 : 0;
}
#ifdef __cplusplus
}
#endif
#endif
