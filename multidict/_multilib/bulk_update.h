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
static int
_md_update(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
           PyObject* key, PyObject* value, reflist_t* defer,
           update_marks_t* marks, bool fits)
{
    bool ci = md->is_ci;
    assert(fits == md_key_fits(md, key, identity));
    bool found = false;
    if (kind_is_compact(md->keys->kind) && UNLIKELY(!fits) &&
        md_to_anystr(md) < 0) {
        return -1;
    }
    if (update_marks_sync(marks, md) < 0) {
        return -1;
    }
    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, md->keys, hash);
    entry_t* entries = htkeys_entries(md->keys);
    uint8_t kind = md->keys->kind;

    for (; iter.index != DKIX_EMPTY; HTKEYSITER_NEXT(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t* entry = entry_at(kind, entries, iter.index);
        if (hash != entry_hash(kind, ci, entry) ||
            bitmap_test(&marks->updated, iter.index) ||
            !str_cmp(identity, entry_identity(kind, ci, entry))) {
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
            replace_key(kind, entry, Py_NewRef(key));
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
        return md_add_for_upd(md, hash, identity, key, value, marks, fits);
    }
    return 0;
}

static int
_md_merge(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
          PyObject* key, PyObject* value, update_marks_t* marks, bool fits)
{
    bool ci = md->is_ci;
    if (update_marks_sync(marks, md) < 0) {
        return -1;
    }
    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, md->keys, hash);
    entry_t* entries = htkeys_entries(md->keys);
    uint8_t kind = md->keys->kind;

    for (; iter.index != DKIX_EMPTY; HTKEYSITER_NEXT(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t* entry = entry_at(kind, entries, iter.index);
        /* An entry this batch added doesn't count as already present. */
        if (hash != entry_hash(kind, ci, entry) ||
            bitmap_test(&marks->updated, iter.index)) {
            continue;
        }
        if (str_cmp(identity, entry_identity(kind, ci, entry))) {
            return 0;
        }
    }

    return md_add_for_upd(md, hash, identity, key, value, marks, fits);
}

/* Removes the entries update() doomed and nothing has written since. Only
   an out-of-memory fallback decref in _md_del_at_deferred() can run Python
   here; the walk then starts over, which each record leaving the set as it
   goes makes safe. */
static int
_md_post_update_deleted(MultiDictObject* md, reflist_t* defer,
                        update_marks_t* marks)
{
    bool ci = md->is_ci;
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
        entry_t* entry = entry_at(kind, entries, pos);
        // Python code run between items may have removed or rewritten it
        if (entry_is_hole(entry) || load_value(entry) != doomed->value) {
            continue;
        }
        htkeysiter_t iter;
        HTKEYSITER_INIT(&iter, keys, entry_hash(kind, ci, entry));
        while (iter.index != pos) {
            assert(iter.index != DKIX_EMPTY);
            HTKEYSITER_NEXT(&iter);
        }
        md_watch_record(md,
                        MultiDict_EVENT_DELETED,
                        entry_identity(kind, ci, entry),
                        entry_hash(kind, ci, entry),
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
                reflist_t* defer, update_marks_t* marks)
{
    // from another table, so not known from the identity
    bool fits = md_key_fits(md, key, identity);
    switch (op) {
        case Update:
            return _md_update(
                md, hash, identity, key, value, defer, marks, fits);
        case Extend:
            return md_add_with_hash(md, hash, identity, key, value, fits);
        case Merge:
            return _md_merge(md, hash, identity, key, value, marks, fits);
    }
    Py_UNREACHABLE();
}

static int
md_update_from_ht(MultiDictObject* md, MultiDictObject* other, UpdateOp op,
                  reflist_t* defer, update_marks_t* marks)
{
    bool other_ci = other->is_ci;
    Py_ssize_t pos;
    Py_hash_t hash;
    PyObject* identity = NULL;
    PyObject* canonical = NULL;
    PyObject* key = NULL;
    PyObject* value = NULL;

    if (other->used == 0) {
        return 0;
    }

    // callers handle md itself: md_extend_self(), or a no-op
    assert(md != other);

    if (md_reserve(md, other->used) < 0) {
        return -1;
    }

    entry_t* entries = htkeys_entries(other->keys);
    uint8_t kind = other->keys->kind;
    Py_ssize_t nentries = other->keys->nentries;

    if (md->is_ci == other_ci) {
        /* other of md's class: nothing here runs Python code, so other's
           table and its kind hold throughout. */
        entry_t* end = entry_at(kind, entries, nentries);
        for (entry_t* entry = entries; entry < end;
             entry = entry_next(kind, entry)) {
            if (entry_is_hole(entry)) {
                continue;
            }
            if (_md_update_item(md,
                                op,
                                entry_hash(kind, other_ci, entry),
                                entry_identity(kind, other_ci, entry),
                                entry->key,
                                entry->value,
                                defer,
                                marks) < 0) {
                return -1;
            }
        }
        return 0;
    }

    for (pos = 0; pos < nentries; pos++) {
        entry_t* entry = entry_at(kind, entries, pos);
        if (entry_is_hole(entry)) {
            continue;
        }
        /* lower() on a str subclass key runs Python code that can mutate
           other and free entry, so hold our own refs. */
        key = Py_NewRef(entry->key);
        value = Py_NewRef(entry->value);
        /* The key leaves as other's istr, whose canonical must be
           other's identity: md's is the unlowered key. */
        canonical =
            Py_XNewRef(other_ci ? entry_identity(kind, true, entry) : NULL);
        identity = md_calc_identity(md, key);
        if (identity == NULL) {
            goto fail;
        }
        hash = unicode_hash(identity);
        if (hash == -1) {
            goto fail;
        }
        /* materialize key */
        Py_SETREF(key, md_calc_key(other, key, canonical));
        Py_CLEAR(canonical);
        if (key == NULL) {
            goto fail;
        }
        if (_md_update_item(md, op, hash, identity, key, value, defer, marks) <
            0) {
            goto fail;
        }
        Py_DECREF(identity);
        Py_DECREF(key);
        Py_DECREF(value);
        /* Both lower() and a finalizer run by the decrefs above can
           replace other's table. */
        entries = htkeys_entries(other->keys);
        kind = other->keys->kind;
        if (nentries > other->keys->nentries) {
            nentries = other->keys->nentries;
        }
    }
    return 0;
fail:
    Py_CLEAR(canonical);
    Py_CLEAR(identity);
    Py_CLEAR(key);
    Py_CLEAR(value);
    return -1;
}

// d.extend(d) is rare: one copy, the class read at run time
static int
md_extend_self(MultiDictObject* md)
{
    if (md_reserve(md, md->keys->nentries) < 0) {
        return -1;
    }

    bool ci = md->is_ci;
    uint8_t kind = md->keys->kind;
    entry_t* entry = htkeys_entries(md->keys);
    entry_t* end = entry_at(kind, entry, md->keys->nentries);
    for (; entry < end; entry = entry_next(kind, entry)) {
        if (!entry_is_hole(entry)) {
            PyObject* identity = entry_identity(kind, ci, entry);
            if (md_add_with_hash(md,
                                 entry_hash(kind, ci, entry),
                                 identity,
                                 entry->key,
                                 entry->value,
                                 md_key_fits(md, entry->key, identity)) < 0) {
                return -1;
            }
        }
    }
    return 0;
}

static int
md_update_from_dict(MultiDictObject* md, PyObject* kwds, UpdateOp op,
                    reflist_t* defer, update_marks_t* marks)
{
    bool ci = md->is_ci;
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
        owned = ci && !PyUnicode_CheckExact(key) &&
                !IStr_CheckExact(md->state, key);
        if (UNLIKELY(owned)) {
            Py_INCREF(key);
            Py_INCREF(value);
        }
        bool fits;
        identity = md_calc_identity_fits(md, key, &fits);
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
                        md, hash, identity, key, value, defer, marks, fits) <
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
                        md, hash, identity, key, value, fits) < 0) {
                    goto fail;
                }
                identity = NULL;
                owned = false;
                break;
            case Merge:
                if (_md_merge(md, hash, identity, key, value, marks, fits) <
                    0) {
                    goto fail;
                }
                break;
        }
        Py_XDECREF(identity);
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

static int
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
        bool fits;
        PyObject* identity = md_calc_identity_fits(md, key, &fits);
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
                md, hash, identity, key, Py_NewRef(value), fits) < 0) {
            Py_DECREF(value);
            Py_DECREF(identity);
            Py_DECREF(key);
            return -1;
        }
    }
    return 0;
}

static void
_err_not_sequence(Py_ssize_t i)
{
    PyErr_Format(PyExc_TypeError,
                 "multidict cannot convert sequence element #%zd"
                 " to a sequence",
                 i);
}

static void
_err_bad_length(Py_ssize_t i, Py_ssize_t n)
{
    PyErr_Format(PyExc_ValueError,
                 "multidict update sequence element #%zd "
                 "has length %zd; 2 is required",
                 i,
                 n);
}

static void
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
            /* Re-read the length every iteration.  Building the identity
               can run arbitrary Python (a str-subclass key's .lower(), an
               __eq__), which may shrink seq; a stale cached size would let
               PyList_GET_ITEM read past the end. */
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

    out->identity = md_calc_identity_fits(md, out->key, &out->fits);
    if (out->identity == NULL) {
        goto fail;
    }

    out->hash = unicode_hash(out->identity);
    if (out->hash == -1) {
        Py_DECREF(out->identity);
        goto fail;
    }
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
        ret = md_add_with_hash_steal_refs(
            md, item.hash, item.identity, item.key, item.value, item.fits);
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
