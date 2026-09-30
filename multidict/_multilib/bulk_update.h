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

/* Nothing here runs Python code or suspends the critical section: the
 * replaced key and value go to `defer`. */
ALWAYS_INLINE static inline int
_md_update(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
           PyObject* key, PyObject* value, reflist_t* defer,
           update_marks_t* marks, uint8_t known)
{
    bool found = false;
    if (htkeys_kind(md->keys, known) == KIND_STR &&
        UNLIKELY(key != identity) && md_to_anystr(md) < 0) {
        return -1;
    }
    if (update_marks_sync(marks, md) < 0) {
        return -1;
    }
    htkeysiter_t iter;
    htkeysiter_init(&iter, md->keys, hash);
    entry_t* entries = htkeys_entries(md->keys);
    uint8_t kind = htkeys_kind(md->keys, known);

    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t* entry = entry_at_k(kind, entries, iter.index);
        if (hash != entry_hash_k(kind, entry) ||
            bitmap_test(&marks->updated, iter.index) ||
            !str_cmp(identity, entry_identity_k(kind, entry))) {
            continue;
        }
        if (!found) {
            found = true;
            /* Marked first: nothing below can fail half-way after the
               entry has changed. An entry an earlier item of this batch
               doomed is reused, which keeps the key at its position. */
            if (bitmap_set(&marks->updated, iter.index) < 0) {
                return -1;
            }
            bitmap_clear(&marks->deleted, iter.index);
            // old_key/old_value decref deferred: see reflist_t
            PyObject* old_key = entry->key;
            PyObject* old_value = load_value(entry);
            replace_key_k(kind, entry, Py_NewRef(key));
            publish_value(entry, Py_NewRef(value));
            md_watch_record(md,
                            MultiDict_EVENT_REPLACED,
                            identity,
                            hash,
                            key,
                            value,
                            old_value);
            /* Push both unconditionally, not with `||`: a failed first
               push already decref'd old_key itself (see reflist_push()'s
               doc comment), but short-circuiting past the second push
               would leak old_value -- neither deferred nor decref'd. */
            int push_ret = reflist_push(defer, old_key);
            if (reflist_push(defer, old_value) < 0) {
                push_ret = -1;
            }
            if (push_ret < 0) {
                return -1;
            }
        } else if (update_marks_doom(marks, iter.index, entry) < 0) {
            return -1;
        }
    }

    if (!found) {
        return md_add_for_upd(md, hash, identity, key, value, marks, known);
    }
    return 0;
}

ALWAYS_INLINE static inline int
_md_merge(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
          PyObject* key, PyObject* value, update_marks_t* marks, uint8_t known)
{
    if (update_marks_sync(marks, md) < 0) {
        return -1;
    }
    htkeysiter_t iter;
    htkeysiter_init(&iter, md->keys, hash);
    entry_t* entries = htkeys_entries(md->keys);
    uint8_t kind = htkeys_kind(md->keys, known);

    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t* entry = entry_at_k(kind, entries, iter.index);
        /* An entry this batch added doesn't count as already present. */
        if (hash != entry_hash_k(kind, entry) ||
            bitmap_test(&marks->updated, iter.index)) {
            continue;
        }
        if (str_cmp(identity, entry_identity_k(kind, entry))) {
            return 0;
        }
    }

    return md_add_for_upd(md, hash, identity, key, value, marks, known);
}

/* Removes the entries update() doomed and nothing has written since. Only
   an out-of-memory fallback decref in _md_del_at_deferred() can run Python
   here; the walk then starts over, which each record leaving the set as it
   goes makes safe. */
static inline int
_md_post_update_deleted(MultiDictObject* md, reflist_t* defer,
                        update_marks_t* marks)
{
    int ret = 0;
restart:
    if (update_marks_sync(marks, md) < 0) {
        return -1;
    }
    htkeys_t* keys = md->keys;
    uint64_t version = md->version;
    entry_t* entries = htkeys_entries(keys);
    uint8_t kind = keys->kind;
    for (Py_ssize_t i = 0; i < marks->ndoomed; i++) {
        doomed_entry_t* doomed = marks->doomed + i;
        Py_ssize_t pos = doomed->index;
        // revived by a later item of this batch, or handled before a restart
        if (!bitmap_test(&marks->deleted, pos)) {
            continue;
        }
        assert(pos < keys->nentries);
        bitmap_clear(&marks->deleted, pos);
        entry_t* entry = entry_at_k(kind, entries, pos);
        // Python code run between items may have removed or rewritten it
        if (entry_identity_k(kind, entry) == NULL ||
            load_value(entry) != doomed->value) {
            continue;
        }
        htkeysiter_t iter;
        htkeysiter_init(&iter, keys, entry_hash_k(kind, entry));
        while (iter.index != pos) {
            assert(iter.index != DKIX_EMPTY);
            htkeysiter_next(&iter);
        }
        md_watch_record(md,
                        MultiDict_EVENT_DELETED,
                        entry_identity_k(kind, entry),
                        entry_hash_k(kind, entry),
                        entry->key,
                        entry->value,
                        NULL);
        if (_md_del_at_deferred(md, iter.slot, entry, defer) < 0) {
            ret = -1;
            if (md->keys != keys || md->version != version) {
                goto restart;
            }
        }
    }
    return ret;
}

/* Ends the batch; the caller holds md's critical section. */
static inline int
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

ALWAYS_INLINE static inline int
_md_update_from_ht_k(MultiDictObject* md, MultiDictObject* other, UpdateOp op,
                     reflist_t* defer, update_marks_t* marks, uint8_t known)
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
    if (md_reserve(md, other->used) < 0) {
        return -1;
    }

    entry_t* entries = htkeys_entries(other->keys);
    uint8_t kind = other->keys->kind;
    Py_ssize_t nentries = other->keys->nentries;

    for (pos = 0; pos < nentries; pos++) {
        entry_t* entry = entry_at_k(kind, entries, pos);
        if (entry_identity_k(kind, entry) == NULL) {
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
            identity = entry_identity_k(kind, entry);
            hash = entry_hash_k(kind, entry);
            key = entry->key;
            value = entry->value;
        }
        switch (op) {
            case Update:
                if (_md_update(
                        md, hash, identity, key, value, defer, marks, known) <
                    0) {
                    goto fail;
                }
                break;
            case Extend:
                if (md_add_with_hash_k(md, hash, identity, key, value, known) <
                    0) {
                    goto fail;
                }
                break;
            case Merge:
                if (_md_merge(md, hash, identity, key, value, marks, known) <
                    0) {
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
            kind = other->keys->kind;
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
_md_update_from_ht_extend_ci(MultiDictObject* md, MultiDictObject* other,
                             reflist_t* defer, update_marks_t* marks)
{
    return _md_update_from_ht_k(md, other, Extend, defer, marks, KIND_ANYSTR);
}

NOINLINE static int
_md_update_from_ht_extend_any(MultiDictObject* md, MultiDictObject* other,
                              reflist_t* defer, update_marks_t* marks)
{
    return _md_update_from_ht_k(md, other, Extend, defer, marks, KIND_UNKNOWN);
}

NOINLINE static int
_md_update_from_ht_update_ci(MultiDictObject* md, MultiDictObject* other,
                             reflist_t* defer, update_marks_t* marks)
{
    return _md_update_from_ht_k(md, other, Update, defer, marks, KIND_ANYSTR);
}

NOINLINE static int
_md_update_from_ht_update_any(MultiDictObject* md, MultiDictObject* other,
                              reflist_t* defer, update_marks_t* marks)
{
    return _md_update_from_ht_k(md, other, Update, defer, marks, KIND_UNKNOWN);
}

NOINLINE static int
_md_update_from_ht_merge_ci(MultiDictObject* md, MultiDictObject* other,
                            reflist_t* defer, update_marks_t* marks)
{
    return _md_update_from_ht_k(md, other, Merge, defer, marks, KIND_ANYSTR);
}

NOINLINE static int
_md_update_from_ht_merge_any(MultiDictObject* md, MultiDictObject* other,
                             reflist_t* defer, update_marks_t* marks)
{
    return _md_update_from_ht_k(md, other, Merge, defer, marks, KIND_UNKNOWN);
}

/* One copy per operation, which every caller names as a constant, and
   per kind: a CIMultiDict table is always KIND_ANYSTR, and its copy
   skips the tests on the kind. */
ALWAYS_INLINE static inline int
md_update_from_ht(MultiDictObject* md, MultiDictObject* other, UpdateOp op,
                  reflist_t* defer, update_marks_t* marks)
{
    switch (op) {
        case Extend:
            return md->is_ci
                       ? _md_update_from_ht_extend_ci(md, other, defer, marks)
                       : _md_update_from_ht_extend_any(
                             md, other, defer, marks);
        case Update:
            return md->is_ci
                       ? _md_update_from_ht_update_ci(md, other, defer, marks)
                       : _md_update_from_ht_update_any(
                             md, other, defer, marks);
        case Merge:
            return md->is_ci
                       ? _md_update_from_ht_merge_ci(md, other, defer, marks)
                       : _md_update_from_ht_merge_any(md, other, defer, marks);
    }
    Py_UNREACHABLE();
}

ALWAYS_INLINE static inline int
_md_extend_self_k(MultiDictObject* md, uint8_t known)
{
    if (md_reserve(md, md->keys->nentries) < 0) {
        return -1;
    }

    Py_ssize_t nentries = md->keys->nentries;
    entry_t* entries = htkeys_entries(md->keys);
    uint8_t kind = htkeys_kind(md->keys, known);
    for (Py_ssize_t pos = 0; pos < nentries; pos++) {
        entry_t* entry = entry_at_k(kind, entries, pos);
        if (entry_identity_k(kind, entry) != NULL) {
            if (md_add_with_hash_k(md,
                                   entry_hash_k(kind, entry),
                                   entry_identity_k(kind, entry),
                                   entry->key,
                                   entry->value,
                                   known) < 0) {
                return -1;
            }
        }
    }
    return 0;
}

NOINLINE static int
_md_extend_self_ci(MultiDictObject* md)
{
    return _md_extend_self_k(md, KIND_ANYSTR);
}

NOINLINE static int
_md_extend_self_any(MultiDictObject* md)
{
    return _md_extend_self_k(md, KIND_UNKNOWN);
}

// A CIMultiDict table is always KIND_ANYSTR: its copy skips the tests on
// the kind.
static inline int
md_extend_self(MultiDictObject* md)
{
    if (md->is_ci) {
        return _md_extend_self_ci(md);
    }
    return _md_extend_self_any(md);
}

ALWAYS_INLINE static inline int
_md_update_from_dict_k(MultiDictObject* md, PyObject* kwds, UpdateOp op,
                       reflist_t* defer, update_marks_t* marks, uint8_t known)
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
                if (_md_update(
                        md, hash, identity, key, value, defer, marks, known) <
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
                if (md_add_with_hash_steal_refs_k(
                        md, hash, identity, key, value, known) < 0) {
                    goto fail;
                }
                identity = NULL;
                owned = false;
                break;
            case Merge:
                if (_md_merge(md, hash, identity, key, value, marks, known) <
                    0) {
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

NOINLINE static int
_md_update_from_dict_extend_ci(MultiDictObject* md, PyObject* kwds,
                               reflist_t* defer, update_marks_t* marks)
{
    return _md_update_from_dict_k(md, kwds, Extend, defer, marks, KIND_ANYSTR);
}

NOINLINE static int
_md_update_from_dict_extend_any(MultiDictObject* md, PyObject* kwds,
                                reflist_t* defer, update_marks_t* marks)
{
    return _md_update_from_dict_k(
        md, kwds, Extend, defer, marks, KIND_UNKNOWN);
}

NOINLINE static int
_md_update_from_dict_update_ci(MultiDictObject* md, PyObject* kwds,
                               reflist_t* defer, update_marks_t* marks)
{
    return _md_update_from_dict_k(md, kwds, Update, defer, marks, KIND_ANYSTR);
}

NOINLINE static int
_md_update_from_dict_update_any(MultiDictObject* md, PyObject* kwds,
                                reflist_t* defer, update_marks_t* marks)
{
    return _md_update_from_dict_k(
        md, kwds, Update, defer, marks, KIND_UNKNOWN);
}

NOINLINE static int
_md_update_from_dict_merge_ci(MultiDictObject* md, PyObject* kwds,
                              reflist_t* defer, update_marks_t* marks)
{
    return _md_update_from_dict_k(md, kwds, Merge, defer, marks, KIND_ANYSTR);
}

NOINLINE static int
_md_update_from_dict_merge_any(MultiDictObject* md, PyObject* kwds,
                               reflist_t* defer, update_marks_t* marks)
{
    return _md_update_from_dict_k(md, kwds, Merge, defer, marks, KIND_UNKNOWN);
}

/* One copy per operation, which every caller names as a constant, and
   per kind: a CIMultiDict table is always KIND_ANYSTR, and its copy
   skips the tests on the kind. */
ALWAYS_INLINE static inline int
md_update_from_dict(MultiDictObject* md, PyObject* kwds, UpdateOp op,
                    reflist_t* defer, update_marks_t* marks)
{
    switch (op) {
        case Extend:
            return md->is_ci
                       ? _md_update_from_dict_extend_ci(md, kwds, defer, marks)
                       : _md_update_from_dict_extend_any(
                             md, kwds, defer, marks);
        case Update:
            return md->is_ci
                       ? _md_update_from_dict_update_ci(md, kwds, defer, marks)
                       : _md_update_from_dict_update_any(
                             md, kwds, defer, marks);
        case Merge:
            return md->is_ci
                       ? _md_update_from_dict_merge_ci(md, kwds, defer, marks)
                       : _md_update_from_dict_merge_any(
                             md, kwds, defer, marks);
    }
    Py_UNREACHABLE();
}

ALWAYS_INLINE static inline int
_md_update_from_kwnames_k(MultiDictObject* md, PyObject* const* args,
                          Py_ssize_t nargs, PyObject* kwnames, uint8_t known)
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
        if (md_add_with_hash_steal_refs_k(
                md, hash, identity, key, Py_NewRef(value), known) < 0) {
            Py_DECREF(value);
            Py_DECREF(identity);
            Py_DECREF(key);
            return -1;
        }
    }
    return 0;
}

NOINLINE static int
_md_update_from_kwnames_ci(MultiDictObject* md, PyObject* const* args,
                           Py_ssize_t nargs, PyObject* kwnames)
{
    return _md_update_from_kwnames_k(md, args, nargs, kwnames, KIND_ANYSTR);
}

NOINLINE static int
_md_update_from_kwnames_any(MultiDictObject* md, PyObject* const* args,
                            Py_ssize_t nargs, PyObject* kwnames)
{
    return _md_update_from_kwnames_k(md, args, nargs, kwnames, KIND_UNKNOWN);
}

// A CIMultiDict table is always KIND_ANYSTR: its copy skips the tests on
// the kind.
static inline int
md_update_from_kwnames(MultiDictObject* md, PyObject* const* args,
                       Py_ssize_t nargs, PyObject* kwnames)
{
    if (md->is_ci) {
        return _md_update_from_kwnames_ci(md, args, nargs, kwnames);
    }
    return _md_update_from_kwnames_any(md, args, nargs, kwnames);
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

ALWAYS_INLINE static inline int
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

ALWAYS_INLINE static inline int
_md_update_from_seq_k(MultiDictObject* md, PyObject* seq, UpdateOp op,
                      reflist_t* defer, update_marks_t* marks, uint8_t known)
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
                if (_md_update(
                        md, hash, identity, key, value, defer, marks, known) <
                    0) {
                    goto fail;
                }
                Py_CLEAR(identity);
                Py_CLEAR(key);
                Py_CLEAR(value);
                break;
            case Extend:
                if (md_add_with_hash_steal_refs_k(
                        md, hash, identity, key, value, known) < 0) {
                    goto fail;
                }
                identity = NULL;
                key = NULL;
                value = NULL;
                break;
            case Merge:
                if (_md_merge(md, hash, identity, key, value, marks, known) <
                    0) {
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

NOINLINE static int
_md_update_from_seq_extend_ci(MultiDictObject* md, PyObject* seq,
                              reflist_t* defer, update_marks_t* marks)
{
    return _md_update_from_seq_k(md, seq, Extend, defer, marks, KIND_ANYSTR);
}

NOINLINE static int
_md_update_from_seq_extend_any(MultiDictObject* md, PyObject* seq,
                               reflist_t* defer, update_marks_t* marks)
{
    return _md_update_from_seq_k(md, seq, Extend, defer, marks, KIND_UNKNOWN);
}

NOINLINE static int
_md_update_from_seq_update_ci(MultiDictObject* md, PyObject* seq,
                              reflist_t* defer, update_marks_t* marks)
{
    return _md_update_from_seq_k(md, seq, Update, defer, marks, KIND_ANYSTR);
}

NOINLINE static int
_md_update_from_seq_update_any(MultiDictObject* md, PyObject* seq,
                               reflist_t* defer, update_marks_t* marks)
{
    return _md_update_from_seq_k(md, seq, Update, defer, marks, KIND_UNKNOWN);
}

NOINLINE static int
_md_update_from_seq_merge_ci(MultiDictObject* md, PyObject* seq,
                             reflist_t* defer, update_marks_t* marks)
{
    return _md_update_from_seq_k(md, seq, Merge, defer, marks, KIND_ANYSTR);
}

NOINLINE static int
_md_update_from_seq_merge_any(MultiDictObject* md, PyObject* seq,
                              reflist_t* defer, update_marks_t* marks)
{
    return _md_update_from_seq_k(md, seq, Merge, defer, marks, KIND_UNKNOWN);
}

/* One copy per operation, which every caller names as a constant, and
   per kind: a CIMultiDict table is always KIND_ANYSTR, and its copy
   skips the tests on the kind. */
ALWAYS_INLINE static inline int
md_update_from_seq(MultiDictObject* md, PyObject* seq, UpdateOp op,
                   reflist_t* defer, update_marks_t* marks)
{
    switch (op) {
        case Extend:
            return md->is_ci
                       ? _md_update_from_seq_extend_ci(md, seq, defer, marks)
                       : _md_update_from_seq_extend_any(md, seq, defer, marks);
        case Update:
            return md->is_ci
                       ? _md_update_from_seq_update_ci(md, seq, defer, marks)
                       : _md_update_from_seq_update_any(md, seq, defer, marks);
        case Merge:
            return md->is_ci
                       ? _md_update_from_seq_merge_ci(md, seq, defer, marks)
                       : _md_update_from_seq_merge_any(md, seq, defer, marks);
    }
    Py_UNREACHABLE();
}
#ifdef __cplusplus
}
#endif
#endif
