#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_TO_DICT_H
#define _MULTIDICT_TO_DICT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <stdbool.h>
#include <stdint.h>

#include "bitmap.h"
#include "compiler.h"
#include "debug.h"
#include "dict.h"
#include "hashtable.h"
#include "htkeys.h"

/* Appends the value of `e`, a match on to_dict()'s hash chain, to *plst,
   creating the list first. */
static inline int
_md_to_dict_append(MultiDictObject* md, PyObject** plst, entry_t* e,
                   uint64_t version)
{
    if (*plst != NULL) {
        return PyList_Append(*plst, e->value);
    }
    *plst = PyList_New(1);
    if (*plst == NULL) {
        return -1;
    }
    /* On 3.10 and 3.11, allocating a tracked object can run a collection
       whose finalizers mutate md: refused as below, before `e` is read
       again. */
    if (md_check_version(md, version) < 0) {
        return -1;
    }
    PyList_SET_ITEM(*plst, 0, Py_NewRef(e->value));
    return 0;
}

/* Stores `lst`, the values of `entry`'s key, under that key; steals `lst`.
   Both calls below can run a str subclass's own __hash__, __eq__ or
   __del__, which may mutate this multidict. That is refused the way the
   iterators refuse one, before the table is trusted again. */
static inline int
_md_to_dict_store(MultiDictObject* md, PyObject* dict, entry_t* entry,
                  PyObject* lst, uint64_t version)
{
    PyObject* key = md_ensure_key(md, entry);
    if (key == NULL) {
        Py_DECREF(lst);
        return -1;
    }
    int ret = PyDict_SetItem(dict, key, lst);
    Py_DECREF(key);
    Py_DECREF(lst);
    if (ret < 0) {
        return -1;
    }
    return md_check_version(md, version);
}

/* Stores the value of `entry`, the only one of its key, as a list of one. */
static inline int
_md_to_dict_single(MultiDictObject* md, PyObject* dict, entry_t* entry,
                   uint64_t version)
{
    PyObject* lst = NULL;
    if (_md_to_dict_append(md, &lst, entry, version) < 0) {
        Py_XDECREF(lst);
        return -1;
    }
    return _md_to_dict_store(md, dict, entry, lst, version);
}

/* _md_to_dict_locked()'s walk of the hash chain of the entry at `pos`,
   the first one of its key, `find` being one of the HTKEYSITER_FIND_*()
   macros. Equal keys are first reached in increasing entry index, so the
   first match is `pos` itself and only the later ones need a mark for the
   outer loop to skip; a repeated slot is one not above the last match,
   as in md_walk_with_hash(). */
#define _MD_TO_DICT_COLLECT(find, identity_, hash_, pos)        \
    do {                                                        \
        PyObject* identity = (identity_);                       \
        Py_hash_t hash = (hash_);                               \
        htkeysiter_t iter;                                      \
        HTKEYSITER_INIT(&iter, keys, hash);                     \
        entry_t* e = NULL;                                      \
        find(&iter, identity, hash, e);                         \
        assert(e != NULL && iter.index == (pos));               \
        Py_ssize_t last = iter.index;                           \
        if (_md_to_dict_append(md, &lst, e, version) < 0) {     \
            goto fail;                                          \
        }                                                       \
        for (;;) {                                              \
            find(&iter, identity, hash, e);                     \
            if (e == NULL) {                                    \
                break;                                          \
            }                                                   \
            if (iter.index <= last) {                           \
                continue;                                       \
            }                                                   \
            last = iter.index;                                  \
            if (bitmap_set(&collected, last) < 0 ||             \
                _md_to_dict_append(md, &lst, e, version) < 0) { \
                goto fail;                                      \
            }                                                   \
        }                                                       \
    } while (0)

/* _md_to_dict_locked()'s loop over a KIND_COMPACT table, `sfx` being cs
   or ci as for compact_entry_identity_cs(), and `find` its
   HTKEYSITER_FIND_COMPACT_CS() or _CI(). */
#define _MD_TO_DICT_COMPACT_LOOP(sfx, find)                                \
    do {                                                                   \
        entry_t* entries = HTKEYS_COMPACT_ENTRIES(keys);                   \
        for (Py_ssize_t pos = 0; pos < keys->nentries; pos++) {            \
            entry_t* entry = entries + pos;                                \
            if (compact_entry_is_hole(entry) ||                            \
                bitmap_test(&collected, pos)) {                            \
                continue; /* deleted, or collected under its first key */  \
            }                                                              \
            _MD_TO_DICT_COLLECT(find,                                      \
                                compact_entry_identity_##sfx(entry),       \
                                compact_entry_hash_##sfx(entry),           \
                                pos);                                      \
            int stored = _md_to_dict_store(md, dict, entry, lst, version); \
            lst = NULL;                                                    \
            if (stored < 0) {                                              \
                goto fail;                                                 \
            }                                                              \
        }                                                                  \
    } while (0)

/* Walks the entries in insertion order, so every key is collected at its
   first spelling; a hash chain walk is not insertion-ordered. Equal keys
   sit on one hash chain in insertion order. One loop per kind; the
   version checks keep md's table in place. */
COLD static int
_md_to_dict_locked(MultiDictObject* md, PyObject** ret)
{
    PyObject* dict = PyDict_New();
    if (dict == NULL) {
        return -1;
    }
    PyObject* lst = NULL;
    uint64_t version = md->version;
    htkeys_t* keys = md->keys;
    bitmap_t collected;
    collected.summary = NULL;

    bitmap_init(&collected, keys, keys->nentries);

    if (!keys->maybe_dups) {
        /* Every key has one entry, so nothing to group; a mutation that
           adds a second is refused. */
        if (kind_is_compact(keys->kind)) {
            entry_t* entries = HTKEYS_COMPACT_ENTRIES(keys);
            for (Py_ssize_t pos = 0; pos < keys->nentries; pos++) {
                entry_t* entry = entries + pos;
                if (!compact_entry_is_hole(entry) &&
                    _md_to_dict_single(md, dict, entry, version) < 0) {
                    goto fail;
                }
            }
        } else {
            anystr_entry_t* entries = HTKEYS_ANYSTR_ENTRIES(keys);
            for (Py_ssize_t pos = 0; pos < keys->nentries; pos++) {
                anystr_entry_t* entry = entries + pos;
                if (!anystr_entry_is_hole(entry) &&
                    _md_to_dict_single(md, dict, &entry->base, version) < 0) {
                    goto fail;
                }
            }
        }
    } else if (kind_is_compact(keys->kind)) {
        if (md->is_ci) {
            _MD_TO_DICT_COMPACT_LOOP(ci, HTKEYSITER_FIND_COMPACT_CI);
        } else {
            _MD_TO_DICT_COMPACT_LOOP(cs, HTKEYSITER_FIND_COMPACT_CS);
        }
    } else {
        anystr_entry_t* entries = HTKEYS_ANYSTR_ENTRIES(keys);
        for (Py_ssize_t pos = 0; pos < keys->nentries; pos++) {
            anystr_entry_t* entry = entries + pos;
            if (anystr_entry_is_hole(entry) || bitmap_test(&collected, pos)) {
                continue;  // deleted, or collected under its first key
            }
            _MD_TO_DICT_COLLECT(
                HTKEYSITER_FIND_ANYSTR, entry->identity, entry->hash, pos);
            int stored =
                _md_to_dict_store(md, dict, &entry->base, lst, version);
            lst = NULL;
            if (stored < 0) {
                goto fail;
            }
        }
    }

    bitmap_release(&collected);
    *ret = dict;
    return 0;
fail:
    bitmap_release(&collected);
    Py_XDECREF(lst);
    Py_DECREF(dict);
    return -1;
}

#undef _MD_TO_DICT_COMPACT_LOOP
#undef _MD_TO_DICT_COLLECT

static int
md_to_dict(MultiDictObject* md, PyObject** ret)
{
    int tmp;
    Py_BEGIN_CRITICAL_SECTION(md);
    tmp = _md_to_dict_locked(md, ret);
    ASSERT_CONSISTENT(md);
    Py_END_CRITICAL_SECTION();
    return tmp;
}

#ifdef __cplusplus
}
#endif

#endif
