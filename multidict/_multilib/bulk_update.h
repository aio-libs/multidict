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
#include "watch.h"

typedef enum _UpdateOp {
    Extend,
    Update,
    Merge,
} UpdateOp;

static inline int
_md_update(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
           PyObject* key, PyObject* value, reflist_t* defer,
           update_marks_t* marks)
{
    bool found = false;
    update_marks_sync(marks, md);

    // See _md_replace() on the retry/deferred-decref shape used here.
    for (;;) {
        htkeysiter_t iter;
        htkeysiter_init(&iter, md->keys, hash);
        /* A retry may have lost the mark on the entry this call already
           wrote; equal keys sit on their chain in insertion order, so it
           is the first unmarked match. */
        bool skip_first = found;
        bool stale = false;

        for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
            if (iter.index < 0) {
                continue;
            }
#ifdef Py_GIL_DISABLED
            htkeys_t* keys_before = md->keys;
            uint64_t version_before = md->version;
#endif
            entry_t* entries = htkeys_entries(md->keys);
            entry_t* entry = entries + iter.index;
            if (hash != entry->hash ||
                bitmap_test(&marks->updated, iter.index) ||
                !str_cmp(identity, entry->identity)) {
                continue;
            }
            if (skip_first) {
                skip_first = false;
                if (bitmap_set(&marks->updated, iter.index) < 0) {
                    goto fail;
                }
                continue;
            }
            if (!found) {
                found = true;
                /* Marked first: nothing below can fail half-way after
                   the entry has changed. */
                if (bitmap_set(&marks->updated, iter.index) < 0) {
                    goto fail;
                }
                if (entry->key == NULL) {
                    /* Half-deleted by an earlier item of this batch; reusing
                       it keeps the key at its original position. */
                    assert(entry->value == NULL);
                    bitmap_clear(&marks->deleted, iter.index);
                    entry->key = Py_NewRef(key);
                    publish_value(entry, Py_NewRef(value));
                    md_watch_record(md,
                                    MultiDict_EVENT_ADDED,
                                    identity,
                                    hash,
                                    key,
                                    value,
                                    NULL);
                } else {
                    // old_key/old_value decref deferred: see reflist_t
                    PyObject* old_key = entry->key;
                    PyObject* old_value = load_value(entry);
                    entry->key = Py_NewRef(key);
                    publish_value(entry, Py_NewRef(value));
                    md_watch_record(md,
                                    MultiDict_EVENT_REPLACED,
                                    identity,
                                    hash,
                                    key,
                                    value,
                                    old_value);
                    /* Push both unconditionally, not with `||`: a
                       failed first push already decref'd old_key itself
                       (see reflist_push()'s doc comment), but
                       short-circuiting past the second push would leak
                       old_value -- neither deferred nor decref'd. */
                    int push_ret = reflist_push(defer, old_key);
                    if (reflist_push(defer, old_value) < 0) {
                        push_ret = -1;
                    }
                    if (push_ret < 0) {
                        goto fail;
                    }
                }
            } else {
                if (bitmap_test(&marks->deleted, iter.index)) {
                    continue;
                }
                if (bitmap_set(&marks->deleted, iter.index) < 0) {
                    goto fail;
                }
                /* Read before the half-delete nulls them, recorded
                   after it succeeds: its first reservation can fail with
                   the entry still in place, and a DELETED event for an
                   entry that is still there is worse than none. The
                   objects stay alive in `defer` across the call. */
                PyObject* gone_identity = entry->identity;
                PyObject* gone_key = entry->key;
                PyObject* gone_value = entry->value;
                if (md_half_delete_for_upd(md, entry, defer) < 0) {
                    goto fail;
                }
                md_watch_record(md,
                                MultiDict_EVENT_DELETED,
                                gone_identity,
                                hash,
                                gone_key,
                                gone_value,
                                NULL);
            }
#ifdef Py_GIL_DISABLED
            /* See _md_replace()'s comment on why both the pointer and
               the version are checked. */
            if (md->keys != keys_before || md->version != version_before) {
                stale = true;
                break;
            }
#endif
        }
        if (stale) {
            /* Whatever moved the table also invalidated the marks. */
            update_marks_sync(marks, md);
            continue;
        }
        break;
    }

    if (!found) {
        if (md_add_for_upd(md, hash, identity, key, value, marks) < 0) {
            goto fail;
        }
    }
    marks->version = md->version;
    return 0;
fail:
    marks->version = md->version;
    return -1;
}

static inline int
_md_merge(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
          PyObject* key, PyObject* value, update_marks_t* marks)
{
    update_marks_sync(marks, md);
    htkeysiter_t iter;
    htkeysiter_init(&iter, md->keys, hash);
    entry_t* entries = htkeys_entries(md->keys);

    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t* entry = entries + iter.index;
        /* An entry this batch added doesn't count as already present. */
        if (hash != entry->hash || bitmap_test(&marks->updated, iter.index)) {
            continue;
        }
        if (str_cmp(identity, entry->identity)) {
            return 0;
        }
    }

    int ret = md_add_for_upd(md, hash, identity, key, value, marks);
    marks->version = md->version;
    return ret;
}

/* Finishes off one half-deleted entry: `slot` must index it. */
static inline int
_md_post_update_del(MultiDictObject* md, htkeys_t* keys, size_t slot,
                    entry_t* entry, reflist_t* defer)
{
    assert(entry->key == NULL);
    PyObject* old_identity = load_identity(entry);
    reset_identity(entry);
    htkeys_set_index(keys, slot, DKIX_DUMMY);
    add_used(md, -1);
    return reflist_push(defer, old_identity);
}

/* The fallback when the `deleted` marks can't be trusted: every half-deleted
   entry still has a NULL key, so a full sweep finds them all. */
COLD static int
_md_post_update_sweep(MultiDictObject* md, reflist_t* defer)
{
    int ret = 0;
    for (;;) {
        htkeys_t* keys = md->keys;
#ifdef Py_GIL_DISABLED
        uint64_t version_before = md->version;
#endif
        size_t num_slots = htkeys_nslots(keys);
        entry_t* entries = htkeys_entries(keys);
        bool stale = false;
        for (size_t slot = 0; slot < num_slots; slot++) {
            Py_ssize_t index = htkeys_get_index(keys, slot);
            if (index >= 0 && entries[index].key == NULL) {
                if (_md_post_update_del(
                        md, keys, slot, entries + index, defer) < 0) {
                    ret = -1;
                }
#ifdef Py_GIL_DISABLED
                if (md->keys != keys || md->version != version_before) {
                    stale = true;
                    break;
                }
#endif
            }
        }
        if (!stale) {
            return ret;
        }
    }
}

static inline int
_md_post_update_deleted(MultiDictObject* md, reflist_t* defer,
                        update_marks_t* marks)
{
    update_marks_sync(marks, md);
    if (marks->lost) {
        return _md_post_update_sweep(md, defer);
    }
    int ret = 0;
    htkeys_t* keys = md->keys;
#ifdef Py_GIL_DISABLED
    uint64_t version_before = md->version;
#endif
    entry_t* entries = htkeys_entries(keys);
    for (Py_ssize_t pos = bitmap_next(&marks->deleted, 0); pos >= 0;
         pos = bitmap_next(&marks->deleted, pos + 1)) {
        entry_t* entry = entries + pos;
        /* Another thread's update(), run while this one's critical section
           was suspended, can revive or finish off one of these in place
           without moving anything. */
        if (entry->identity == NULL || entry->key != NULL) {
            continue;
        }
        htkeysiter_t iter;
        htkeysiter_init(&iter, keys, entry->hash);
        while (iter.index != pos && iter.index != DKIX_EMPTY) {
            htkeysiter_next(&iter);
        }
        assert(iter.index == pos);
        if (iter.index != pos) {
            continue;
        }
        if (_md_post_update_del(md, keys, iter.slot, entry, defer) < 0) {
            ret = -1;
        }
#ifdef Py_GIL_DISABLED
        /* Only an out-of-memory fallback decref above can run Python. */
        if (md->keys != keys || md->version != version_before) {
            if (_md_post_update_sweep(md, defer) < 0) {
                ret = -1;
            }
            break;
        }
#endif
    }
    return ret;
}

static inline int
md_post_update(MultiDictObject* md, reflist_t* defer, update_marks_t* marks)
{
    int ret = 0;
    /* `defer` is NULL only for merge(), which never half-deletes. */
    if (defer != NULL) {
        ret = _md_post_update_deleted(md, defer, marks);
    }
    store_version(md, next_version(md->state));
    md_watch_record_simple(md, MultiDict_EVENT_BATCH_END);
    ASSERT_CONSISTENT(md, false);
    return ret;
}

NOINLINE static int
md_update_from_ht(MultiDictObject* md, MultiDictObject* other, UpdateOp op,
                  reflist_t* defer, update_marks_t* marks)
{
    Py_ssize_t pos;
    Py_hash_t hash;
    PyObject* identity = NULL;
    PyObject* key = NULL;
    PyObject* value = NULL;
    bool recalc_identity = md->is_ci != other->is_ci;

    if (other->used == 0) {
        return 0;
    }

    if (md == other && op != Extend) {
        /* update(self) and merge(self) leave the dict unchanged: every key
           already maps to its own values.  Short-circuit -- doing the work in
           place would soft-delete and reinsert the very entries we iterate. */
        return 0;
    }

    /* Pre-allocate room for other's items so the inserts below cannot trigger
       a resize of md->keys.  This is what makes extend(self) (md IS other,
       e.g. ``d.extend(d)``) safe: a resize would free the very entries array
       we iterate here, a use-after-free.  Reserving up front also lets us
       snapshot the entry count so self-extension does not reprocess the
       entries it just appended. */
    if (md_reserve_for_upd(md, other->used, marks) < 0) {
        return -1;
    }

    entry_t* entries = htkeys_entries(other->keys);
    Py_ssize_t nentries = other->keys->nentries;

    for (pos = 0; pos < nentries; pos++) {
        entry_t* entry = entries + pos;
        if (entry->identity == NULL) {
            continue;
        }
        if (recalc_identity) {
            /* lower() on a str subclass key runs Python code that can mutate
               other and free entry, so hold our own refs. */
            key = Py_NewRef(entry->key);
            value = Py_NewRef(entry->value);
            identity = md_calc_identity(md, key);
            if (identity == NULL) {
                goto fail;
            }
            hash = unicode_hash(identity);
            if (hash == -1) {
                goto fail;
            }
            /* materialize key */
            Py_SETREF(key, md_calc_key(other, key, identity));
            if (key == NULL) {
                goto fail;
            }
        } else {
            identity = entry->identity;
            hash = entry->hash;
            key = entry->key;
            value = entry->value;
        }
        switch (op) {
            case Update:
                if (_md_update(md, hash, identity, key, value, defer, marks) <
                    0) {
                    goto fail;
                }
                break;
            case Extend:
                if (md_add_with_hash(md, hash, identity, key, value) < 0) {
                    goto fail;
                }
                break;
            case Merge:
                if (_md_merge(md, hash, identity, key, value, marks) < 0) {
                    goto fail;
                }
                break;
        }
        if (recalc_identity) {
            Py_CLEAR(identity);
            Py_CLEAR(key);
            Py_CLEAR(value);
            /* Both lower() and a finalizer run by the decrefs above can
               replace other's table. */
            entries = htkeys_entries(other->keys);
            if (nentries > other->keys->nentries) {
                nentries = other->keys->nentries;
            }
        }
    }
    return 0;
fail:
    if (recalc_identity) {
        Py_CLEAR(identity);
        Py_CLEAR(key);
        Py_CLEAR(value);
    }
    return -1;
}

NOINLINE static int
md_extend_self(MultiDictObject* md)
{
    if (md_reserve(md, md->keys->nentries) < 0) {
        return -1;
    }

    Py_ssize_t nentries = md->keys->nentries;
    entry_t* entries = htkeys_entries(md->keys);
    for (Py_ssize_t pos = 0; pos < nentries; pos++) {
        entry_t* entry = entries + pos;
        if (entry->identity != NULL) {
            if (md_add_with_hash(md,
                                 entry->hash,
                                 entry->identity,
                                 entry->key,
                                 entry->value) < 0) {
                return -1;
            }
        }
    }
    return 0;
}

NOINLINE static int
md_update_from_dict(MultiDictObject* md, PyObject* kwds, UpdateOp op,
                    reflist_t* defer, update_marks_t* marks)
{
    Py_ssize_t pos = 0;
    PyObject* identity = NULL;
    PyObject* key = NULL;
    PyObject* value = NULL;
    bool owned = false;

    assert(PyDict_CheckExact(kwds));

    // PyDict_Next returns borrowed refs
    while (PyDict_Next(kwds, &pos, &key, &value)) {
        /* Only lower() on a str subclass key runs Python code here, and it
           can clear kwds and free both; any other key keeps them alive
           through kwds. */
        owned = md->is_ci && !PyUnicode_CheckExact(key) &&
                !IStr_CheckExact(md->state, key);
        if (UNLIKELY(owned)) {
            Py_INCREF(key);
            Py_INCREF(value);
        }
        identity = md_calc_identity(md, key);
        if (identity == NULL) {
            goto fail;
        }
        Py_hash_t hash = unicode_hash(identity);
        if (hash == -1) {
            goto fail;
        }
        switch (op) {
            case Update:
                if (_md_update(md, hash, identity, key, value, defer, marks) <
                    0) {
                    goto fail;
                }
                break;
            case Extend:
                if (!owned) {
                    Py_INCREF(key);
                    Py_INCREF(value);
                    owned = true;
                }
                if (md_add_with_hash_steal_refs(
                        md, hash, identity, key, value) < 0) {
                    goto fail;
                }
                identity = NULL;
                owned = false;
                break;
            case Merge:
                if (_md_merge(md, hash, identity, key, value, marks) < 0) {
                    goto fail;
                }
                break;
        }
        Py_CLEAR(identity);
        if (owned) {
            Py_DECREF(key);
            Py_DECREF(value);
        }
    }
    return 0;
fail:
    Py_CLEAR(identity);
    if (owned) {
        Py_DECREF(key);
        Py_DECREF(value);
    }
    return -1;
}

static inline int
md_update_from_kwnames(MultiDictObject* md, PyObject* const* args,
                       Py_ssize_t nargs, PyObject* kwnames)
{
    Py_ssize_t nkwargs = PyTuple_GET_SIZE(kwnames);
    if (md_reserve(md, nkwargs) < 0) {
        return -1;
    }
    for (Py_ssize_t i = 0; i < nkwargs; i++) {
        PyObject* key = PyTuple_GET_ITEM(kwnames, i);  // borrowed
        assert(PyUnicode_Check(key));
        Py_INCREF(key);
        PyObject* identity = md_calc_identity(md, key);
        if (identity == NULL) {
            Py_DECREF(key);
            return -1;
        }
        Py_hash_t hash = unicode_hash(identity);
        if (hash == -1) {
            Py_DECREF(identity);
            Py_DECREF(key);
            return -1;
        }
        PyObject* value = args[nargs + i];  // borrowed
        if (md_add_with_hash_steal_refs(
                md, hash, identity, key, Py_NewRef(value)) < 0) {
            Py_DECREF(value);
            Py_DECREF(identity);
            Py_DECREF(key);
            return -1;
        }
    }
    return 0;
}

static inline void
_err_not_sequence(Py_ssize_t i)
{
    PyErr_Format(PyExc_TypeError,
                 "multidict cannot convert sequence element #%zd"
                 " to a sequence",
                 i);
}

static inline void
_err_bad_length(Py_ssize_t i, Py_ssize_t n)
{
    PyErr_Format(PyExc_ValueError,
                 "multidict update sequence element #%zd "
                 "has length %zd; 2 is required",
                 i,
                 n);
}

static inline void
_err_cannot_fetch(Py_ssize_t i, const char* name)
{
    PyErr_Format(PyExc_ValueError,
                 "multidict update sequence element #%zd's "
                 "%s could not be fetched",
                 i,
                 name);
}

static int
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

NOINLINE static int
md_update_from_seq(MultiDictObject* md, PyObject* seq, UpdateOp op,
                   reflist_t* defer, update_marks_t* marks)
{
    PyObject* it = NULL;
    PyObject* item = NULL;  // seq[i]

    PyObject* key = NULL;
    PyObject* value = NULL;
    PyObject* identity = NULL;
    PyObject* items = NULL;

    Py_ssize_t i;
    Py_ssize_t size = -1;

    enum { LIST, TUPLE, ITER } kind;

    if (!PyList_CheckExact(seq) && !PyTuple_CheckExact(seq)) {
        items = PyMapping_Items(seq);
        if (items != NULL) {
            seq = items;
        } else {
            if (!PyErr_ExceptionMatches(PyExc_AttributeError) &&
                !PyErr_ExceptionMatches(PyExc_TypeError)) {
                // propagate MemoryError / KeyboardInterrupt / etc.
                goto fail;
            }
            // seq is not a mapping; fall back to treating it as a sequence
            PyErr_Clear();
        }
    }

    if (PyList_CheckExact(seq)) {
        kind = LIST;
        size = PyList_GET_SIZE(seq);
        if (size == 0) {
            goto exit;
        }
    } else if (PyTuple_CheckExact(seq)) {
        kind = TUPLE;
        size = PyTuple_GET_SIZE(seq);
        if (size == 0) {
            goto exit;
        }
    } else {
        kind = ITER;
        it = PyObject_GetIter(seq);
        if (it == NULL) {
            goto fail;
        }
    }

    for (i = 0;; ++i) {  // i - index into seq of current element
        switch (kind) {
            case LIST:
                /* Re-read the length every iteration.  Building the identity
                   below can run arbitrary Python (a str-subclass key's
                   .lower(), an __eq__), which may shrink seq; a stale cached
                   size would let PyList_GET_ITEM read past the end. */
                if (i >= PyList_GET_SIZE(seq)) {
                    goto exit;
                }
                item = list_getitem_ref(seq, i);
                if (_list_item_gone(item)) {
                    goto fail;
                }
                break;
            case TUPLE:
                if (i >= size) {
                    goto exit;
                }
                item = PyTuple_GET_ITEM(seq, i);
                if (item == NULL) {
                    goto fail;
                }
                Py_INCREF(item);
                break;
            case ITER: {
                int res = PyIter_NextItem(it, &item);
                if (res < 0) {
                    goto fail;
                }
                if (res == 0) {
                    goto exit;
                }
                break;
            }
        }

        if (_md_parse_item(i, item, &key, &value) < 0) {
            goto fail;
        }

        identity = md_calc_identity(md, key);
        if (identity == NULL) {
            goto fail;
        }

        Py_hash_t hash = unicode_hash(identity);
        if (hash == -1) {
            goto fail;
        }

        switch (op) {
            case Update:
                if (_md_update(md, hash, identity, key, value, defer, marks) <
                    0) {
                    goto fail;
                }
                Py_CLEAR(identity);
                Py_CLEAR(key);
                Py_CLEAR(value);
                break;
            case Extend:
                if (md_add_with_hash_steal_refs(
                        md, hash, identity, key, value) < 0) {
                    goto fail;
                }
                identity = NULL;
                key = NULL;
                value = NULL;
                break;
            case Merge:
                if (_md_merge(md, hash, identity, key, value, marks) < 0) {
                    goto fail;
                }
                Py_CLEAR(identity);
                Py_CLEAR(key);
                Py_CLEAR(value);
                break;
        }
        Py_CLEAR(item);
    }

exit:
    Py_CLEAR(it);
    Py_CLEAR(items);
    return 0;

fail:
    Py_CLEAR(identity);
    Py_CLEAR(it);
    Py_CLEAR(item);
    Py_CLEAR(key);
    Py_CLEAR(value);
    Py_CLEAR(items);
    return -1;
}
#ifdef __cplusplus
}
#endif
#endif
