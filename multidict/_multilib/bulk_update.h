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

/* An entry on _md_update()'s chain with the key, not written by this
   batch yet, at `index`: the first gets the new key and value, the rest
   are doomed. -1 on error. */
static inline int
_md_update_matched(MultiDictObject* md, uint8_t kind, entry_t* entry,
                   Py_ssize_t index, bool* pfound, Py_hash_t hash,
                   PyObject* identity, PyObject* key, PyObject* value,
                   reflist_t* defer, update_marks_t* marks)
{
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

/* _md_update()'s walk of the hash chain: 1 if the key was there, 0 if
   not, 2 if not but this batch has added it already, -1 on error. One
   loop per kind. */
static inline int
_md_update_replace(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
                   PyObject* key, PyObject* value, reflist_t* defer,
                   update_marks_t* marks)
{
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
            if (_md_update_matched(md,
                                   KIND_COMPACT,
                                   entry,
                                   iter.index,
                                   &found,
                                   hash,
                                   identity,
                                   key,
                                   value,
                                   defer,
                                   marks) < 0) {
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
            if (_md_update_matched(md,
                                   KIND_COMPACT,
                                   entry,
                                   iter.index,
                                   &found,
                                   hash,
                                   identity,
                                   key,
                                   value,
                                   defer,
                                   marks) < 0) {
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
            if (_md_update_matched(md,
                                   KIND_ANYSTR,
                                   entry,
                                   iter.index,
                                   &found,
                                   hash,
                                   identity,
                                   key,
                                   value,
                                   defer,
                                   marks) < 0) {
                return -1;
            }
            if (!md->keys->maybe_dups) {
                break;  // the key's only entry
            }
        }
    }
    return found ? 1 : added ? 2 : 0;
}

/* Nothing here runs Python code or suspends the critical section: the
 * replaced key and value go to `defer`. */
static int
_md_update(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
           PyObject* key, PyObject* value, reflist_t* defer,
           update_marks_t* marks, bool fits)
{
    assert(fits == md_key_fits(md, key, identity));
    if (kind_is_compact(md->keys->kind) && UNLIKELY(!fits) &&
        md_to_anystr(md) < 0) {
        return -1;
    }
    if (update_marks_sync(marks, md) < 0) {
        return -1;
    }
    int found =
        _md_update_replace(md, hash, identity, key, value, defer, marks);
    if (found == 0 || found == 2) {
        return md_add_for_upd(md,
                              hash,
                              identity,
                              key,
                              value,
                              marks,
                              fits,
                              found == 2 ? MD_SLOT_CHECK : MD_SLOT_FIND);
    }
    return found < 0 ? -1 : 0;
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
_md_merge(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
          PyObject* key, PyObject* value, update_marks_t* marks, bool fits)
{
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
_md_post_update_delete(MultiDictObject* md, htkeys_t* keys, Py_ssize_t pos,
                       entry_t* entry, PyObject* identity, Py_hash_t hash,
                       reflist_t* defer, uint64_t version)
{
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
    if (_md_del_at_deferred(md, iter.slot, entry, defer) < 0) {
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
                _md_post_update_delete(md,                                  \
                                       keys,                                \
                                       pos,                                 \
                                       entry,                               \
                                       compact_entry_identity_##sfx(entry), \
                                       compact_entry_hash_##sfx(entry),     \
                                       defer,                               \
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
_md_post_update_pass(MultiDictObject* md, reflist_t* defer,
                     update_marks_t* marks)
{
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
        int del = _md_post_update_delete(md,
                                         keys,
                                         pos,
                                         &entry->base,
                                         entry->identity,
                                         entry->hash,
                                         defer,
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
   an out-of-memory fallback decref in _md_del_at_deferred() can run Python
   here; the walk then starts over, which each record leaving the set as it
   goes makes safe. */
COLD static int
_md_post_update_deleted(MultiDictObject* md, reflist_t* defer,
                        update_marks_t* marks)
{
    int ret = 0;
    int pass;
    do {
        if (update_marks_sync(marks, md) < 0) {
            return -1;
        }
        pass = _md_post_update_pass(md, defer, marks);
        if (pass != 0) {
            ret = -1;
        }
    } while (pass == _MD_POST_UPDATE_RESTART);
    return ret;
}

#undef _MD_POST_UPDATE_RESTART

/* Ends the batch; the caller holds md's critical section. */
static int
md_post_update(MultiDictObject* md, reflist_t* defer, update_marks_t* marks)
{
    int ret = 0;
    /* `defer` is NULL only for merge(), which never deletes. */
    if (defer != NULL) {
        ret = _md_post_update_deleted(md, defer, marks);
    }
    update_marks_end(md);
    bump_version(md);
    md_watch_record_simple(md, MultiDict_EVENT_BATCH_END);
    ASSERT_CONSISTENT(md);
    return ret;
}

// One item of an update(), extend() or merge() from another multidict.
static int
_md_update_item(MultiDictObject* md, UpdateOp op, Py_hash_t hash,
                PyObject* identity, PyObject* key, PyObject* value,
                reflist_t* defer, update_marks_t* marks, Py_ssize_t slot)
{
    // from another table, so not known from the identity
    bool fits = md_key_fits(md, key, identity);
    switch (op) {
        case Update:
            return _md_update(
                md, hash, identity, key, value, defer, marks, fits);
        case Extend:
            return md_add_with_hash(
                md, hash, identity, key, value, fits, slot);
        case Merge:
            return _md_merge(md, hash, identity, key, value, marks, fits);
    }
    Py_UNREACHABLE();
}

typedef struct _md_update_state {
    MultiDictObject* md;
    UpdateOp op;
    reflist_t* defer;
    update_marks_t* marks;
    bool same_class;
    Py_ssize_t slot;  // for an extend() of a key with other's identity
} md_update_state_t;

static int
_md_update_visit(void* user_data, PyObject* identity, Py_hash_t hash,
                 PyObject* key, PyObject* value)
{
    md_update_state_t* state = (md_update_state_t*)user_data;
    MultiDictObject* md = state->md;
    if (state->same_class) {
        // other's identities and hashes are md's own
        return _md_update_item(md,
                               state->op,
                               hash,
                               identity,
                               key,
                               value,
                               state->defer,
                               state->marks,
                               state->slot) < 0
                   ? -1
                   : 1;
    }
    PyObject* own;
    PyObject* stored;
    bool fits;
    if (md_calc_identity_hash_key(md, key, &own, &hash, &stored, &fits) < 0) {
        return -1;
    }
    int ret = _md_update_item(md,
                              state->op,
                              hash,
                              own,
                              stored,
                              value,
                              state->defer,
                              state->marks,
                              MD_SLOT_CHECK);
    Py_DECREF(own);
    Py_DECREF(stored);
    return ret < 0 ? -1 : 1;
}

static int
md_update_from_ht(MultiDictObject* md, MultiDictObject* other, UpdateOp op,
                  reflist_t* defer, update_marks_t* marks)
{
    if (other->used == 0) {
        return 0;
    }

    // callers handle md itself: md_extend_self(), or a no-op
    assert(md != other);

    if (md_reserve(md, other->used) < 0) {
        return -1;
    }

    /* A key of the other class gets md's identity and md's form: an istr
       for a CIMultiDict, other's istr as is for a MultiDict. */
    bool same_class = md->is_ci == other->is_ci;
    // keys repeated in other repeat here; unique ones stay unique in an empty
    // md
    Py_ssize_t slot = MD_SLOT_CHECK;
    if (op == Extend && same_class) {
        if (other->keys->maybe_dups) {
            md->keys->maybe_dups = 1;
        } else if (md->used == 0) {
            slot = MD_SLOT_FIND;
        }
    }
    md_update_state_t state = {md, op, defer, marks, same_class, slot};
    return md_walk_all(other, true, _md_update_visit, &state) < 0 ? -1 : 0;
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
md_update_from_dict(MultiDictObject* md, PyObject* kwds, UpdateOp op,
                    reflist_t* defer, update_marks_t* marks)
{
    Py_ssize_t pos = 0;
    PyObject* identity = NULL;
    PyObject* stored = NULL;
    PyObject* key;
    PyObject* value;

    assert(PyDict_CheckExact(kwds));
    /* Distinct exact str keys stay unique in an empty MultiDict; a str
       subclass's own __eq__ can let a dict hold an equal one too. */
    Py_ssize_t slot =
        !md->is_ci && md->used == 0 ? MD_SLOT_FIND : MD_SLOT_CHECK;

    // PyDict_Next returns borrowed refs, which kwds keeps alive
    while (PyDict_Next(kwds, &pos, &key, &value)) {
        bool fits;
        Py_hash_t hash;
        if (md_calc_identity_hash_key(
                md, key, &identity, &hash, &stored, &fits) < 0) {
            return -1;
        }
        switch (op) {
            case Update:
                if (_md_update(md,
                               hash,
                               identity,
                               stored,
                               value,
                               defer,
                               marks,
                               fits) < 0) {
                    goto fail;
                }
                break;
            case Extend:
                if (!PyUnicode_CheckExact(key)) {
                    slot = MD_SLOT_CHECK;
                }
                if (md_add_with_hash_steal_refs(md,
                                                hash,
                                                identity,
                                                stored,
                                                Py_NewRef(value),
                                                fits,
                                                slot) < 0) {
                    Py_DECREF(value);
                    goto fail;
                }
                identity = NULL;
                stored = NULL;
                break;
            case Merge:
                if (_md_merge(md, hash, identity, stored, value, marks, fits) <
                    0) {
                    goto fail;
                }
                break;
        }
        Py_CLEAR(identity);
        Py_CLEAR(stored);
    }
    return 0;
fail:
    Py_CLEAR(identity);
    Py_CLEAR(stored);
    return -1;
}

static int
md_update_from_kwnames(MultiDictObject* md, PyObject* const* args,
                       Py_ssize_t nargs, PyObject* kwnames)
{
    Py_ssize_t nkwargs = PyTuple_GET_SIZE(kwnames);
    // as in md_update_from_dict()
    Py_ssize_t slot =
        !md->is_ci && md->used == 0 ? MD_SLOT_FIND : MD_SLOT_CHECK;
    if (md_reserve(md, nkwargs) < 0) {
        return -1;
    }
    for (Py_ssize_t i = 0; i < nkwargs; i++) {
        PyObject* key = PyTuple_GET_ITEM(kwnames, i);  // borrowed
        assert(PyUnicode_Check(key));
        if (!PyUnicode_CheckExact(key)) {
            slot = MD_SLOT_CHECK;
        }
        PyObject* identity;
        Py_hash_t hash;
        bool fits;
        if (md_calc_identity_hash_key(md, key, &identity, &hash, &key, &fits) <
            0) {
            return -1;
        }
        PyObject* value = args[nargs + i];  // borrowed
        if (md_add_with_hash_steal_refs(
                md, hash, identity, key, Py_NewRef(value), fits, slot) < 0) {
            Py_DECREF(value);
            Py_DECREF(identity);
            Py_DECREF(key);
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
    PyObject* identity;
    PyObject* key;
    PyObject* value;
    Py_hash_t hash;
    bool fits;
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
_md_seq_next(MultiDictObject* md, seq_iter_t* it, Py_ssize_t i,
             seq_item_t* out)
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

    PyObject* stored;
    if (md_calc_identity_hash_key(
            md, out->key, &out->identity, &out->hash, &stored, &out->fits) <
        0) {
        goto fail;
    }
    // releasing the parsed key can run a str subclass's __del__
    Py_SETREF(out->key, stored);
    return 1;
fail:
    Py_DECREF(item);
    Py_DECREF(out->key);
    Py_DECREF(out->value);
    return -1;
}

static inline void
_seq_item_clear(seq_item_t* item)
{
    Py_DECREF(item->identity);
    Py_DECREF(item->key);
    Py_DECREF(item->value);
    Py_DECREF(item->pair);
}

static int
_md_update_from_seq_extend(MultiDictObject* md, PyObject* seq)
{
    seq_iter_t it;
    seq_item_t item;
    int ret;

    if (_md_seq_prepare(seq, &it) < 0) {
        return -1;
    }
    for (Py_ssize_t i = 0;; ++i) {
        ret = _md_seq_next(md, &it, i, &item);
        if (ret <= 0) {
            break;
        }
        ret = md_add_with_hash_steal_refs(md,
                                          item.hash,
                                          item.identity,
                                          item.key,
                                          item.value,
                                          item.fits,
                                          MD_SLOT_CHECK);
        if (ret < 0) {
            _seq_item_clear(&item);
            break;
        }
        Py_DECREF(item.pair);
    }
    Py_DECREF(it.obj);
    return ret;
}

static int
_md_update_from_seq_update(MultiDictObject* md, PyObject* seq,
                           reflist_t* defer, update_marks_t* marks)
{
    seq_iter_t it;
    seq_item_t item;
    int ret;

    if (_md_seq_prepare(seq, &it) < 0) {
        return -1;
    }
    for (Py_ssize_t i = 0;; ++i) {
        ret = _md_seq_next(md, &it, i, &item);
        if (ret <= 0) {
            break;
        }
        ret = _md_update(md,
                         item.hash,
                         item.identity,
                         item.key,
                         item.value,
                         defer,
                         marks,
                         item.fits);
        _seq_item_clear(&item);
        if (ret < 0) {
            break;
        }
    }
    Py_DECREF(it.obj);
    return ret;
}

static int
_md_update_from_seq_merge(MultiDictObject* md, PyObject* seq, reflist_t* defer,
                          update_marks_t* marks)
{
    seq_iter_t it;
    seq_item_t item;
    int ret;

    if (_md_seq_prepare(seq, &it) < 0) {
        return -1;
    }
    for (Py_ssize_t i = 0;; ++i) {
        ret = _md_seq_next(md, &it, i, &item);
        if (ret <= 0) {
            break;
        }
        ret = _md_merge(md,
                        item.hash,
                        item.identity,
                        item.key,
                        item.value,
                        marks,
                        item.fits);
        _seq_item_clear(&item);
        if (ret < 0) {
            break;
        }
    }
    Py_DECREF(it.obj);
    return ret;
}

static int
md_update_from_seq(MultiDictObject* md, PyObject* seq, UpdateOp op,
                   reflist_t* defer, update_marks_t* marks)
{
    switch (op) {
        case Extend:
            return _md_update_from_seq_extend(md, seq);
        case Update:
            return _md_update_from_seq_update(md, seq, defer, marks);
        case Merge:
            return _md_update_from_seq_merge(md, seq, defer, marks);
    }
    Py_UNREACHABLE();
}
#ifdef __cplusplus
}
#endif
#endif
