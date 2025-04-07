#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_HASHTABLE_H
#define _MULTIDICT_HASHTABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "istr.h"
#include "state.h"
#include "htkeys.h"


typedef struct _hashtable {
    mod_state *state;
    Py_ssize_t ma_used;

    uint64_t version;
    bool calc_ci_indentity;

    htkeys_t * ma_keys;
} ht_t;


typedef struct _ht_pos {
    Py_ssize_t pos;
    uint64_t version;
} ht_pos_t;


typedef struct _ht_finder {
    ht_t *ht;
    htkeysiter_t iter;
    uint64_t version;
    Py_hash_t hash;
    PyObject *identity; // borrowed ref
    bool first;
} ht_finder_t;


/* Global counter used to set ma_version_tag field of dictionary.
 * It is incremented each time that a dictionary is created and each
 * time that a dictionary is modified. */
static uint64_t ht_global_version = 0;

#define NEXT_VERSION() (++ht_global_version)

/* GROWTH_RATE. Growth rate upon hitting maximum load.
 * Currently set to used*3.
 * This means that dicts double in size when growing without deletions,
 * but have more head room when the number of deletions is on a par with the
 * number of insertions.  See also bpo-17563 and bpo-33205.
 *
 * GROWTH_RATE was set to used*4 up to version 3.2.
 * GROWTH_RATE was set to used*2 in version 3.3.0
 * GROWTH_RATE was set to used*2 + capacity/2 in 3.4.0-3.6.0.
 */
static inline Py_ssize_t GROWTH_RATE(ht_t *ht) {
    return ht->ma_used * 3;
}


static inline int _ht_check_consistency(ht_t *ht, bool update);
static inline int _ht_dump(ht_t *ht);

#ifndef NDEBUG
#  define ASSERT_CONSISTENT(ht, update) assert(_ht_check_consistency(ht, update))
#else
#  define ASSERT_CONSISTENT(ht, update) assert(1)
#endif


static inline Py_ssize_t
ht_sizeof(ht_t *ht)
{
    if (ht->ma_keys != &empty_htkeys) {
        return htkeys_sizeof(ht->ma_keys);
    }
    return 0;
}


static inline int
_str_cmp(PyObject *s1, PyObject *s2)
{
    PyObject *ret = PyUnicode_RichCompare(s1, s2, Py_EQ);
    if (Py_IsTrue(ret)) {
        Py_DECREF(ret);
        return 1;
    }
    else if (ret == NULL) {
        return -1;
    }
    else {
        Py_DECREF(ret);
        return 0;
    }
}


static inline PyObject *
_key_to_ident(mod_state *state, PyObject *key)
{
    if (IStr_Check(state, key)) {
        return Py_NewRef(((istrobject*)key)->canonical);
    }
    if (PyUnicode_CheckExact(key)) {
        return Py_NewRef(key);
    }
    if (PyUnicode_Check(key)) {
        return PyUnicode_FromObject(key);
    }
    PyErr_SetString(PyExc_TypeError,
                    "MultiDict keys should be either str "
                    "or subclasses of str");
    return NULL;
}


static inline PyObject *
_ci_key_to_ident(mod_state *state, PyObject *key)
{
    if (IStr_Check(state, key)) {
        return Py_NewRef(((istrobject*)key)->canonical);
    }
    if (PyUnicode_Check(key)) {
        PyObject *ret = PyObject_CallMethodNoArgs(key, state->str_lower);
        if (!PyUnicode_CheckExact(ret)) {
            PyObject *tmp = PyUnicode_FromObject(ret);
            Py_CLEAR(ret);
            if (tmp == NULL) {
                return NULL;
            }
            ret = tmp;
        }
        return ret;
    }
    PyErr_SetString(PyExc_TypeError,
                    "CIMultiDict keys should be either str "
                    "or subclasses of str");
    return NULL;
}


static inline PyObject *
_arg_to_key(mod_state *state, PyObject *key, PyObject *ident)
{
    if (PyUnicode_Check(key)) {
        return Py_NewRef(key);
    }
    PyErr_SetString(PyExc_TypeError,
                    "MultiDict keys should be either str "
                    "or subclasses of str");
    return NULL;
}


static inline PyObject *
_ci_arg_to_key(mod_state *state, PyObject *key, PyObject *ident)
{
    if (IStr_Check(state, key)) {
        return Py_NewRef(key);
    }
    if (PyUnicode_Check(key)) {
        return IStr_New(state, key, ident);
    }
    PyErr_SetString(PyExc_TypeError,
                    "CIMultiDict keys should be either str "
                    "or subclasses of str");
    return NULL;
}


static inline int
_ht_resize(ht_t *ht, uint8_t log2_newsize, bool update)
{
    htkeys_t *oldkeys, *newkeys;

    if (log2_newsize >= SIZEOF_SIZE_T*8) {
        PyErr_NoMemory();
        return -1;
    }
    assert(log2_newsize >= HT_LOG_MINSIZE);

    oldkeys = ht->ma_keys;

    /* Allocate a new table. */
    newkeys = htkeys_new(log2_newsize);
    assert(newkeys);
    if (newkeys == NULL) {
        return -1;
    }
    // New table must be large enough.
    assert(newkeys->dk_usable >= ht->ma_used);

    Py_ssize_t numentries = ht->ma_used;
    entry_t *oldentries = DK_ENTRIES(oldkeys);
    entry_t *newentries = DK_ENTRIES(newkeys);
    if (oldkeys->dk_nentries == numentries) {
        memcpy(newentries, oldentries, numentries * sizeof(entry_t));
    } else {
        entry_t *ep = oldentries;
        for (Py_ssize_t i = 0; i < numentries; i++) {
            if (!update) {
                while (ep->identity == NULL)
                    ep++;
            }
            newentries[i] = *ep++;
        }
    }
    if (update) {
        if (htkeys_build_indices_for_upd(newkeys, newentries, numentries) < 0) {
            return -1;
        }
    } else {
        if (htkeys_build_indices(newkeys, newentries, numentries) < 0) {
            return -1;
        }
    }

    ht->ma_keys = newkeys;

    if (oldkeys != &empty_htkeys) {
        htkeys_free(oldkeys);
    }

    ht->ma_keys->dk_usable = ht->ma_keys->dk_usable - numentries;
    ht->ma_keys->dk_nentries = numentries;
    ASSERT_CONSISTENT(ht, update);
    return 0;
}


static inline int
_ht_resize_for_insert(ht_t *ht)
{
    return _ht_resize(ht, calculate_log2_keysize(GROWTH_RATE(ht)), false);
}


static inline int
_ht_resize_for_update(ht_t *ht)
{
    return _ht_resize(ht, calculate_log2_keysize(GROWTH_RATE(ht)), true);
}


static inline int
_ht_reserve(ht_t *ht, Py_ssize_t extra_size, bool update)
{
    uint8_t new_size = estimate_log2_keysize(extra_size + ht->ma_used);
    if (new_size > DK_LOG_SIZE(ht->ma_keys)) {
        return _ht_resize(ht, new_size, update);
    }
    return 0;
}


static inline int
ht_reserve(ht_t *ht, Py_ssize_t extra_size)
{
    return _ht_reserve(ht, extra_size, false);
}


static inline int
_ht_init(ht_t *ht, mod_state *state,
                bool calc_ci_identity, Py_ssize_t minused)
{
    ht->state = state;
    ht->calc_ci_indentity = calc_ci_identity;
    ht->ma_used = 0;
    ht->version = NEXT_VERSION();

    const uint8_t log2_max_presize = 17;
    const Py_ssize_t max_presize = ((Py_ssize_t)1) << log2_max_presize;
    uint8_t log2_newsize;
    htkeys_t *new_keys;

    if (minused <= USABLE_FRACTION(HT_MINSIZE)) {
        ht->ma_keys = &empty_htkeys;
        ASSERT_CONSISTENT(ht, false);
        return 0;
    }
    /* There are no strict guarantee that returned dict can contain minused
     * items without resize.  So we create medium size dict instead of very
     * large dict or MemoryError.
     */
    if (minused > USABLE_FRACTION(max_presize)) {
        log2_newsize = log2_max_presize;
    }
    else {
        log2_newsize = estimate_log2_keysize(minused);
    }

    new_keys = htkeys_new(log2_newsize);
    if (new_keys == NULL)
        return -1;
    ht->ma_keys = new_keys;
    ASSERT_CONSISTENT(ht, false);
    return 0;
}


static inline int
ht_init(ht_t *ht, mod_state *state, Py_ssize_t size)
{
    return _ht_init(ht, state, /* calc_ci_identity = */ false, size);
}


static inline int
ci_ht_init(ht_t *ht, mod_state *state, Py_ssize_t size)
{
    return _ht_init(ht, state, /* calc_ci_identity = */ true, size);
}


static inline int
ht_clone_from_ht(ht_t *ht, ht_t *other)
{
    ASSERT_CONSISTENT(other, false);
    memcpy(ht, other, sizeof(ht_t));
    if (other->ma_keys != &empty_htkeys) {
        size_t size = htkeys_sizeof(other->ma_keys);
        htkeys_t *keys = PyMem_Malloc(size);
        if (keys == NULL) {
            PyErr_NoMemory();
            return -1;
        }
        memcpy(keys, other->ma_keys, size);
        entry_t *entry = DK_ENTRIES(keys);
        for (Py_ssize_t idx = 0; idx < keys->dk_nentries; idx++, entry++) {
            Py_XINCREF(entry->identity);
            Py_XINCREF(entry->key);
            Py_XINCREF(entry->value);
        }
        ht->ma_keys = keys;
    }
    ASSERT_CONSISTENT(ht, false);
    return 0;
}


static inline PyObject *
ht_calc_identity(ht_t *ht, PyObject *key)
{
    if (ht->calc_ci_indentity)
        return _ci_key_to_ident(ht->state, key);
    return _key_to_ident(ht->state, key);
}


static inline PyObject *
ht_calc_key(ht_t *ht, PyObject *key, PyObject *ident)
{
    if (ht->calc_ci_indentity)
        return _ci_arg_to_key(ht->state, key, ident);
    return _arg_to_key(ht->state, key, ident);
}


static inline Py_ssize_t
ht_len(ht_t *ht)
{
    return ht->ma_used;
}


static inline PyObject *
_ht_ensure_key(ht_t *ht, entry_t *entry)
{
    assert(entry >= DK_ENTRIES(ht->ma_keys));
    assert(entry < DK_ENTRIES(ht->ma_keys) + ht->ma_keys->dk_nentries);
    PyObject *key = ht_calc_key(ht, entry->key, entry->identity);
    if (key == NULL) {
        return NULL;
    }
    if (key != entry->key) {
        Py_SETREF(entry->key, key);
    } else {
        Py_CLEAR(key);
    }
    return Py_NewRef(entry->key);
}


static inline int
_ht_add_with_hash_steal_refs(ht_t *ht, Py_hash_t hash, PyObject *identity,
                             PyObject *key, PyObject *value)
{
    if (ht->ma_keys->dk_usable <= 0 || ht->ma_keys == &empty_htkeys) {
        /* Need to resize. */
        if (_ht_resize_for_insert(ht) < 0) {
            return -1;
        }
    } else if (ht->ma_keys->dk_ndummies > htkeys_dummies_fraction(ht->ma_keys)) {
        if (htkeys_rebuild_indices(ht->ma_keys, false) < 0) {
            return -1;
        }
    }
    Py_ssize_t hashpos = htkeys_find_empty_slot(ht->ma_keys, hash);
    htkeys_set_index(ht->ma_keys, hashpos, ht->ma_keys->dk_nentries);

    entry_t *entry = DK_ENTRIES(ht->ma_keys) + ht->ma_keys->dk_nentries;

    entry->identity = identity;
    entry->key = key;
    entry->value = value;
    entry->hash = hash;

    ht->version = NEXT_VERSION();
    ht->ma_used += 1;
    ht->ma_keys->dk_usable -= 1;
    ht->ma_keys->dk_nentries += 1;

    return 0;
}


static inline int
_ht_add_with_hash(ht_t *ht, Py_hash_t hash, PyObject *identity,
                  PyObject *key, PyObject *value)
{
    Py_INCREF(identity);
    Py_INCREF(key);
    Py_INCREF(value);
    return _ht_add_with_hash_steal_refs(ht, hash, identity, key, value);
}


static inline int
_ht_add_for_upd_steal_refs(ht_t *ht, Py_hash_t hash, PyObject *identity,
                           PyObject *key, PyObject *value)
{
    if (ht->ma_keys->dk_usable <= 0 || ht->ma_keys == &empty_htkeys) {
        /* Need to resize. */
        if (_ht_resize_for_update(ht) < 0) {
            return -1;
        }
    } else if (ht->ma_keys->dk_ndummies > htkeys_dummies_fraction(ht->ma_keys)) {
        if (htkeys_rebuild_indices(ht->ma_keys, true) < 0) {
            return -1;
        }
    }
    Py_ssize_t hashpos = htkeys_find_empty_slot(ht->ma_keys, hash);
    htkeys_set_index(ht->ma_keys, hashpos, ht->ma_keys->dk_nentries);

    entry_t *entry = DK_ENTRIES(ht->ma_keys) + ht->ma_keys->dk_nentries;

    entry->identity = identity;
    entry->key = key;
    entry->value = value;
    entry->hash = -1;

    ht->version = NEXT_VERSION();
    ht->ma_used += 1;
    ht->ma_keys->dk_usable -= 1;
    ht->ma_keys->dk_nentries += 1;

    return 0;
}


static inline int
_ht_add_for_upd(ht_t *ht, Py_hash_t hash, PyObject *identity,
                PyObject *key, PyObject *value)
{
    Py_INCREF(identity);
    Py_INCREF(key);
    Py_INCREF(value);
    return _ht_add_for_upd_steal_refs(ht, hash, identity, key, value);
}


static inline int
ht_add(ht_t *ht, PyObject *key, PyObject *value)
{
    PyObject *identity = ht_calc_identity(ht, key);
    if (identity == NULL) {
        goto fail;
    }
    Py_hash_t hash = PyObject_Hash(identity);
    if (hash == -1) {
        goto fail;
    }
    int ret = _ht_add_with_hash(ht, hash, identity, key, value);
    ASSERT_CONSISTENT(ht, false);
    Py_DECREF(identity);
    return ret;
fail:
    Py_XDECREF(identity);
    return -1;
}


static inline int
_ht_del_at(ht_t *ht, size_t slot, entry_t *entry)
{
    assert(ht->ma_keys != &empty_htkeys);
    Py_CLEAR(entry->identity);
    Py_CLEAR(entry->key);
    Py_CLEAR(entry->value);
    htkeys_set_index(ht->ma_keys, slot, DKIX_DUMMY);
    ht->ma_keys->dk_ndummies += 1;
    ht->ma_used -= 1;
    return 0;
}


static inline int
_ht_del_at_for_upd(ht_t *ht, size_t slot, entry_t *entry)
{
    /* half deletion,
       the entry could be replaced later with key and value set
       or it will be finally cleaned up with identity=NULL,
       ma_used -= 1, and setting the hash to DKIX_DUMMY
       in ht_post_update()
    */
    assert(ht->ma_keys != &empty_htkeys);
    Py_CLEAR(entry->key);
    Py_CLEAR(entry->value);
    return 0;
}


static inline int
ht_del(ht_t *ht, PyObject *key)
{
    PyObject *identity = ht_calc_identity(ht, key);
    if (identity == NULL) {
        goto fail;
    }

    Py_hash_t hash = PyObject_Hash(identity);
    if (hash == -1) {
        goto fail;
    }

    bool found = false;

    htkeysiter_t iter;
    htkeysiter_init(&iter, ht->ma_keys, hash);

    entry_t *entries = DK_ENTRIES(ht->ma_keys);

    for (;iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t *entry = entries + iter.index;
        if (hash != entry->hash) {
            continue;
        }
        int tmp = _str_cmp(entry->identity, identity);
        if (tmp < 0) {
            goto fail;
        }
        if (tmp == 0) {
            continue;
        }

        found = true;
        if (_ht_del_at(ht, iter.slot, entry) < 0) {
            goto fail;
        }
    }

    if (!found) {
        PyErr_SetObject(PyExc_KeyError, key);
        goto fail;
    } else {
        ht->version = NEXT_VERSION();
    }
    Py_DECREF(identity);
    ASSERT_CONSISTENT(ht, false);
    return 0;
fail:
    Py_XDECREF(identity);
    return -1;
}


static inline uint64_t
ht_version(ht_t *ht)
{
    return ht->version;
}


static inline void
ht_init_pos(ht_t *ht, ht_pos_t *pos)
{
    pos->pos = 0;
    pos->version = ht->version;
}

static inline int
ht_next(ht_t *ht, ht_pos_t *pos, PyObject **pidentity,
        PyObject **pkey, PyObject **pvalue)
{
    int ret = 0;
    if (pos->pos >= ht->ma_keys->dk_nentries) {
        goto cleanup;
    }

    if (pos->version != ht->version) {
        PyErr_SetString(PyExc_RuntimeError,
                        "MultiDict is changed during iteration");
        ret = -1;
        goto cleanup;
    }

    entry_t *entries = DK_ENTRIES(ht->ma_keys);
    entry_t *entry = entries + pos->pos;

    while (entry->identity == NULL) {
        pos->pos += 1;
        if (pos->pos >= ht->ma_keys->dk_nentries) {
            goto cleanup;
        }
        entry += 1;
    }

    if (pidentity) {
        *pidentity = Py_NewRef(entry->identity);;
    }

    if (pkey) {
        assert(entry->key != NULL);
        *pkey = _ht_ensure_key(ht, entry);
        if (*pkey == NULL) {
            assert(PyErr_Occurred());
            ret = -1;
            goto cleanup;
        }
    }
    if (pvalue) {
        *pvalue = Py_NewRef(entry->value);
    }

    ++pos->pos;
    return 1;
cleanup:
    if (pidentity) {
        *pidentity = NULL;
    }
    if (pkey) {
        *pkey = NULL;
    }
    if (pvalue) {
        *pvalue = NULL;
    }
    return ret;
}

static inline int
ht_init_finder(ht_t *ht, PyObject *identity, ht_finder_t *finder)
{
    finder->version = ht->version;
    finder->ht = ht;
    finder->identity = identity;
    finder->hash = PyObject_Hash(identity);
    if (finder->hash == -1) {
        return -1;
    }
    htkeysiter_init(&finder->iter, finder->ht->ma_keys, finder->hash);
    finder->first = true;
    return -0;
}


static inline Py_ssize_t
ht_finder_slot(ht_finder_t *finder)
{
    assert(finder->ht != NULL);
    return finder->iter.slot;
}


static inline Py_ssize_t
ht_finder_index(ht_finder_t *finder)
{
    assert(finder->ht != NULL);
    assert(finder->iter.index >= 0);
    return finder->iter.index;
}


static inline int
ht_find_next(ht_finder_t *finder, PyObject **pkey, PyObject **pvalue)
{
    int ret = 0;
    assert(finder->iter.keys == finder->ht->ma_keys);
    if (finder->iter.keys != finder->ht->ma_keys
        || finder->version != finder->ht->version) {
        ret = -1;
        PyErr_SetString(PyExc_RuntimeError,
                        "MultiDict is changed during iteration");
        goto cleanup;
    }

    entry_t *entries = DK_ENTRIES(finder->ht->ma_keys);
    if (!finder->first) {
        htkeysiter_next(&finder->iter);
        finder->first = false;
    }

    for (;finder->iter.index != DKIX_EMPTY; htkeysiter_next(&finder->iter)) {
        if (finder->iter.index < 0) {
            continue;
        }
        entry_t *entry = entries + finder->iter.index;
        if (entry->hash != finder->hash) {
            continue;
        }
        int tmp = _str_cmp(finder->identity, entry->identity);
        if (tmp < 0) {
            ret = -1;
            goto cleanup;
        }
        if (tmp == 0) {
            continue;
        }

        /* found, mark the entry as visited */
        entry->hash = -1;

        if (pkey) {
            *pkey = _ht_ensure_key(finder->ht, entry);
            if (*pkey == NULL) {
                ret = -1;
                goto cleanup;
            }
        }
        if (pvalue) {
            *pvalue = Py_NewRef(entry->value);
        }
        return 1;
    }
    ret = 0;
cleanup:
    if (pkey) {
        *pkey = NULL;
    }
    if (pvalue) {
        *pvalue = NULL;
    }
    return ret;
}


static inline void ht_finder_cleanup(ht_finder_t *finder)
{
    if (finder->ht == NULL) {
        return;
    }

    htkeysiter_init(&finder->iter, finder->ht->ma_keys, finder->hash);
    entry_t *entries = DK_ENTRIES(finder->ht->ma_keys);
    for (;finder->iter.index != DKIX_EMPTY; htkeysiter_next(&finder->iter)) {
        if (finder->iter.index < 0) {
            continue;
        }
        entry_t *entry = entries + finder->iter.index;
        if (entry->hash == -1) {
            entry->hash = finder->hash;
        }
    }
    ASSERT_CONSISTENT(finder->ht, false);
    finder->ht = NULL;
}


static inline int
ht_contains(ht_t *ht, PyObject *key, PyObject **pret)
{
    if (!PyUnicode_Check(key)) {
        return 0;
    }

    if (ht->ma_keys->dk_ndummies > htkeys_dummies_fraction(ht->ma_keys)) {
        if (htkeys_rebuild_indices(ht->ma_keys, false) < 0) {
            return -1;
        }
    }

    PyObject *ident = ht_calc_identity(ht, key);
    if (ident == NULL) {
        goto fail;
    }

    Py_hash_t hash = PyObject_Hash(ident);
    if (hash == -1) {
        goto fail;
    }

    htkeysiter_t iter;
    htkeysiter_init(&iter, ht->ma_keys, hash);
    entry_t *entries = DK_ENTRIES(ht->ma_keys);

    for(; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t *entry = entries + iter.index;
        if (hash != entry->hash) {
            continue;
        }
        int tmp = _str_cmp(ident, entry->identity);
        if (tmp > 0) {
            Py_DECREF(ident);
            if (pret != NULL) {
                *pret = _ht_ensure_key(ht, entry);
                if (*pret == NULL) {
                    goto fail;
                }
            }
            return 1;
        }
        else if (tmp < 0) {
            goto fail;
        }
    }

    Py_DECREF(ident);
    if (pret != NULL) {
        *pret = NULL;
    }
    return 0;
fail:
    Py_XDECREF(ident);
    if (pret != NULL) {
        *pret = NULL;
    }
    return -1;
}


static inline int
ht_get_one(ht_t *ht, PyObject *key, PyObject **ret)
{
    if (ht->ma_keys->dk_ndummies > htkeys_dummies_fraction(ht->ma_keys)) {
        if (htkeys_rebuild_indices(ht->ma_keys, false) < 0) {
            return -1;
        }
    }

    PyObject *ident = ht_calc_identity(ht, key);
    if (ident == NULL) {
        goto fail;
    }

    Py_hash_t hash = PyObject_Hash(ident);
    if (hash == -1) {
        goto fail;
    }

    htkeysiter_t iter;
    htkeysiter_init(&iter, ht->ma_keys, hash);
    entry_t *entries = DK_ENTRIES(ht->ma_keys);

    for(; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t *entry = entries + iter.index;
        if (hash != entry->hash) {
            continue;
        }
        int tmp = _str_cmp(ident, entry->identity);
        if (tmp > 0) {
            Py_DECREF(ident);
            *ret = Py_NewRef(entry->value);
            return 0;
        }
        else if (tmp < 0) {
            goto fail;
        }
    }

    Py_DECREF(ident);
    return 0;
fail:
    Py_XDECREF(ident);
    return -1;
}


static inline int
ht_get_all(ht_t *ht, PyObject *key, PyObject **ret)
{
    *ret == NULL;

    if (ht->ma_keys->dk_ndummies > htkeys_dummies_fraction(ht->ma_keys)) {
        if (htkeys_rebuild_indices(ht->ma_keys, false) < 0) {
            return -1;
        }
    }

    ht_finder_t finder = {0};

    PyObject *identity = ht_calc_identity(ht, key);
    if (identity == NULL) {
        goto fail;
    }

    if (ht_init_finder(ht, identity, &finder) < 0) {
        assert(PyErr_Occurred());
        goto fail;
    }

    int tmp;
    PyObject *value = NULL;;

    while ((tmp = ht_find_next(&finder, NULL, &value)) > 0) {
        if (*ret == NULL) {
            *ret = PyList_New(1);
            if (*ret == NULL) {
                goto fail;
            }
            PyList_SET_ITEM(*ret, 0, value);
            value = NULL;  // stealed by PyList_SET_ITEM
        } else {
            if (PyList_Append(*ret, value) < 0) {
                goto fail;
            }
            Py_CLEAR(value);
        }
    }
    if (tmp < 0) {
        goto fail;
    }

    ht_finder_cleanup(&finder);
    Py_DECREF(identity);
    return 0;
fail:
    ht_finder_cleanup(&finder);
    Py_XDECREF(identity);
    Py_XDECREF(value);
    Py_CLEAR(*ret);
    return -1;
}


static inline PyObject *
ht_set_default(ht_t *ht, PyObject *key, PyObject *value)
{
    if (ht->ma_keys->dk_ndummies > htkeys_dummies_fraction(ht->ma_keys)) {
        if (htkeys_rebuild_indices(ht->ma_keys, false) < 0) {
            return NULL;
        }
    }

    PyObject *ident = ht_calc_identity(ht, key);
    if (ident == NULL) {
        goto fail;
    }

    Py_hash_t hash = PyObject_Hash(ident);
    if (hash == -1) {
        goto fail;
    }

    htkeysiter_t iter;
    htkeysiter_init(&iter, ht->ma_keys, hash);
    entry_t *entries = DK_ENTRIES(ht->ma_keys);

    for(; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t *entry = entries + iter.index;

        if (hash != entry->hash) {
            continue;
        }
        int tmp = _str_cmp(ident, entry->identity);
        if (tmp > 0) {
            Py_DECREF(ident);
            ASSERT_CONSISTENT(ht, false);
            return Py_NewRef(entry->value);
        }
        else if (tmp < 0) {
            goto fail;
        }
    }

    if (_ht_add_with_hash(ht, hash, ident, key, value) < 0) {
        goto fail;
    }

    Py_DECREF(ident);
    ASSERT_CONSISTENT(ht, false);
    return Py_NewRef(value);
fail:
    Py_XDECREF(ident);
    return NULL;
}


static inline int
ht_pop_one(ht_t *ht, PyObject *key, PyObject **ret)
{
    if (ht->ma_keys->dk_ndummies > htkeys_dummies_fraction(ht->ma_keys)) {
        if (htkeys_rebuild_indices(ht->ma_keys, false) < 0) {
            return -1;
        }
    }

    PyObject *value = NULL;

    PyObject *ident = ht_calc_identity(ht, key);
    if (ident == NULL) {
        goto fail;
    }

    Py_hash_t hash = PyObject_Hash(ident);
    if (hash == -1) {
        goto fail;
    }

    htkeysiter_t iter;
    htkeysiter_init(&iter, ht->ma_keys, hash);
    entry_t *entries = DK_ENTRIES(ht->ma_keys);

    for(; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t *entry = entries + iter.index;

        if (hash != entry->hash) {
            continue;
        }
        int tmp = _str_cmp(ident, entry->identity);
        if (tmp > 0) {
            value = Py_NewRef(entry->value);
            if (_ht_del_at(ht, iter.slot, entry) < 0) {
                goto fail;
            }
            Py_DECREF(ident);
            *ret = value;
            ht->version = NEXT_VERSION();
            ASSERT_CONSISTENT(ht, false);
            return 0;
        }
        else if (tmp < 0) {
            goto fail;
        }
    }

    ASSERT_CONSISTENT(ht, false);
    return 0;
fail:
    Py_XDECREF(value);
    Py_XDECREF(ident);
    return -1;
}


static inline int
ht_pop_all(ht_t *ht, PyObject *key, PyObject ** ret)
{
    if (ht->ma_keys->dk_ndummies > htkeys_dummies_fraction(ht->ma_keys)) {
        if (htkeys_rebuild_indices(ht->ma_keys, false) < 0) {
            return -1;
        }
    }
    PyObject *lst = NULL;

    PyObject *ident = ht_calc_identity(ht, key);
    if (ident == NULL) {
        goto fail;
    }

    Py_hash_t hash = PyObject_Hash(ident);
    if (hash == -1) {
        goto fail;
    }

    if (ht_len(ht) == 0) {
        Py_DECREF(ident);
        return 0;
    }

    htkeysiter_t iter;
    htkeysiter_init(&iter, ht->ma_keys, hash);
    entry_t *entries = DK_ENTRIES(ht->ma_keys);

    for(; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t *entry = entries + iter.index;

        if (hash != entry->hash) {
            continue;
        }
        int tmp = _str_cmp(ident, entry->identity);
        if (tmp > 0) {
            if (lst == NULL) {
                lst = PyList_New(1);
                if (lst == NULL) {
                    goto fail;
                }
                if (PyList_SetItem(lst, 0, Py_NewRef(entry->value)) < 0) {
                    goto fail;
                }
            } else if (PyList_Append(lst, entry->value) < 0) {
                goto fail;
            }
            if (_ht_del_at(ht, iter.slot, entry) < 0) {
                goto fail;
            }
            ht->version = NEXT_VERSION();
        }
        else if (tmp < 0) {
            goto fail;
        }
    }

    *ret = lst;
    Py_DECREF(ident);
    ASSERT_CONSISTENT(ht, false);
    return 0;
fail:
    Py_XDECREF(ident);
    Py_XDECREF(lst);
    return -1;
}


static inline PyObject *
ht_pop_item(ht_t *ht)
{
    if (ht->ma_used == 0) {
        PyErr_SetString(PyExc_KeyError, "empty multidict");
        return NULL;
    }

    if (ht->ma_keys->dk_ndummies > htkeys_dummies_fraction(ht->ma_keys)) {
        if (htkeys_rebuild_indices(ht->ma_keys, false) < 0) {
            return NULL;
        }
    }

    entry_t *entries = DK_ENTRIES(ht->ma_keys);

    Py_ssize_t pos = ht->ma_keys->dk_nentries - 1;
    entry_t *entry = entries + pos;
    while (pos >= 0 && entry->identity == NULL) {
        pos--;
        entry--;
    }
    assert(pos >= 0);

    PyObject *key = ht_calc_key(ht, entry->key, entry->identity);
    if (key == NULL) {
        return NULL;
    }
    PyObject *ret = PyTuple_Pack(2, key, entry->value);
    Py_CLEAR(key);
    if (ret == NULL) {
        return NULL;
    }

    htkeysiter_t iter;
    htkeysiter_init(&iter, ht->ma_keys, entry->hash);

    for(; iter.index != pos; htkeysiter_next(&iter)) {
    }
    if (_ht_del_at(ht, iter.slot, entry) < 0) {
        return NULL;
    }
    ht->version = NEXT_VERSION();
    ASSERT_CONSISTENT(ht, false);
    return ret;
}


static inline int
_ht_replace(ht_t *ht, PyObject * key, PyObject *value,
            PyObject *identity, Py_hash_t hash)
{
    if (ht->ma_keys->dk_ndummies > htkeys_dummies_fraction(ht->ma_keys)) {
        if (htkeys_rebuild_indices(ht->ma_keys, false) < 0) {
            return -1;
        }
    }

    int found = 0;
    ht_finder_t finder = {0};
    if (ht_init_finder(ht, identity, &finder) < 0) {
        assert(PyErr_Occurred());
        goto fail;
    }
    entry_t *entries = DK_ENTRIES(ht->ma_keys);

    int tmp;

    // don't grab neither key nor value but use the calculated index
    while ((tmp = ht_find_next(&finder, NULL, NULL)) > 0) {
        entry_t *entry = entries + ht_finder_index(&finder);
        if (!found) {
            found = 1;
            Py_SETREF(entry->key, Py_NewRef(key));
            Py_SETREF(entry->value, Py_NewRef(value));
            entry->hash = -1;
        } else {
            if (_ht_del_at(ht, ht_finder_slot(&finder), entry) < 0) {
                goto fail;
            }
        }
    }
    if (tmp < 0) {
        goto fail;
    }

    ht_finder_cleanup(&finder);
    if (!found) {
        if (_ht_add_with_hash(ht, hash, identity, key, value) < 0) {
            goto fail;
        }
        return 0;
    }
    else {
        ht->version = NEXT_VERSION();
        return 0;
    }
fail:
    ht_finder_cleanup(&finder);
    return -1;
}

static inline int
ht_replace(ht_t *ht, PyObject * key, PyObject *value)
{
    PyObject *identity = ht_calc_identity(ht, key);
    if (identity == NULL) {
        goto fail;
    }

    Py_hash_t hash = PyObject_Hash(identity);
    if (hash == -1) {
        goto fail;
    }

    int ret = _ht_replace(ht, key, value, identity, hash);
    Py_DECREF(identity);
    ASSERT_CONSISTENT(ht, false);
    return ret;
fail:
    Py_XDECREF(identity);
    return -1;
}


static inline int
_ht_update(ht_t *ht, Py_hash_t hash, PyObject *identity,
           PyObject *key, PyObject *value)
{
    if (ht->ma_keys->dk_ndummies > htkeys_dummies_fraction(ht->ma_keys)) {
        if (htkeys_rebuild_indices(ht->ma_keys, true) < 0) {
            return -1;
        }
    }
    htkeysiter_t iter;
    htkeysiter_init(&iter, ht->ma_keys, hash);
    entry_t *entries = DK_ENTRIES(ht->ma_keys);
    bool found = false;

    for(; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (iter.index == DKIX_DUMMY) {
            continue;
        }
        entry_t *entry = entries + iter.index;
        if (hash != entry->hash) {
            continue;
        }
        int tmp = _str_cmp(identity, entry->identity);
        if (tmp > 0) {
            if (!found) {
                found = true;
                if (entry->key == NULL) {
                    /* entry->key could be NULL if it was deleted
                       by the previous _ht_update call during the iteration
                       in ht_update_from* functions. */
                    assert(entry->value == NULL);
                    entry->key = Py_NewRef(key);
                    entry->value = Py_NewRef(value);
                } else {
                    Py_SETREF(entry->key, Py_NewRef(key));
                    Py_SETREF(entry->value, Py_NewRef(value));
                }
                entry->hash = -1;
            } else {
                if (_ht_del_at_for_upd(ht, iter.slot, entry) < 0) {
                    goto fail;
                }
            }
        }
        else if (tmp < 0) {
            goto fail;
        }

    }

    if (!found) {
        if (_ht_add_for_upd(ht, hash, identity, key, value) < 0) {
            goto fail;
        }
    }
    return 0;
fail:
    return -1;
}

static inline int
ht_post_update(ht_t *ht)
{
    size_t num_slots = DK_SIZE(ht->ma_keys);
    entry_t *entries = DK_ENTRIES(ht->ma_keys);
    for (size_t slot = 0; slot < num_slots; slot++) {
        Py_ssize_t index = htkeys_get_index(ht->ma_keys, slot);
        if (index >= 0) {
            entry_t *entry = entries + index;
            if (entry->hash == -1) {
                entry->hash = PyObject_Hash(entry->identity);
                if (entry->hash == -1) {
                    // hash of string always exists but still
                    return -1;
                }
            }
            if (entry->key == NULL) {
                /* the entry is marked for deletion during .update() call
                   and not replaced with a new value */
                Py_CLEAR(entry->identity);
                htkeys_set_index(ht->ma_keys, slot, DKIX_DUMMY);
                ht->ma_keys->dk_ndummies += 1;
                ht->ma_used -= 1;
            }
        }
    }
    if (ht->ma_keys->dk_ndummies > htkeys_dummies_fraction(ht->ma_keys)) {
        if (htkeys_rebuild_indices(ht->ma_keys, false) < 0) {
            return -1;
        }
    }
    return 0;
}

static inline int
ht_update_from_ht(ht_t *ht, ht_t *other, bool update)
{
    Py_ssize_t pos;
    Py_hash_t hash;
    PyObject *identity = NULL;
    PyObject *key = NULL;
    bool recalc_identity = ht->calc_ci_indentity != other->calc_ci_indentity;

    if (other->ma_used == 0) {
        return 0;
    }

    uint8_t new_size = Py_MAX(
        estimate_log2_keysize(ht_len(other) + ht->ma_used),
        DK_LOG_SIZE(ht->ma_keys));
    if (new_size > ht->ma_keys->dk_log2_size) {
        if (_ht_resize(ht, new_size, update)) {
            return -1;
        }
    }

    entry_t *entries = DK_ENTRIES(other->ma_keys);

    for (pos = 0; pos < other->ma_keys->dk_nentries; pos++) {
        entry_t *entry = entries + pos;
        if (entry->identity == NULL) {
            continue;
        }
        if (recalc_identity) {
            identity = ht_calc_identity(ht, entry->key);
            if (identity == NULL) {
                goto fail;
            }
            hash = PyObject_Hash(identity);
            if (hash == -1) {
                goto fail;
            }
            /* materialize key */
            key = ht_calc_key(other, entry->key, identity);
            if (key == NULL) {
                goto fail;
            }
        } else {
            identity = entry->identity;
            hash = entry->hash;
            key = entry->key;
        }
        if (update) {
            if (_ht_update(ht, hash, identity, key, entry->value) < 0) {
                goto fail;
            }
        } else {
            if (_ht_add_with_hash(ht, hash, identity, key, entry->value) < 0) {
                goto fail;
            }
        }
        if (recalc_identity) {
            Py_CLEAR(identity);
            Py_CLEAR(key);
        }
    }
    return 0;
fail:
    if (recalc_identity) {
        Py_CLEAR(identity);
        Py_CLEAR(key);
    }
    return -1;
}

static inline int
ht_update_from_dict(ht_t *ht, PyObject *kwds, bool update)
{
    Py_ssize_t pos = 0;
    PyObject *identity = NULL;
    PyObject *key = NULL;
    PyObject *value = NULL;

    assert(PyDict_CheckExact(kwds));
    if (PyDict_GET_SIZE(kwds) == 0) {
        return 0;
    }

    uint8_t new_size = Py_MAX(
        estimate_log2_keysize(PyDict_GET_SIZE(kwds) + ht->ma_used),
        DK_LOG_SIZE(ht->ma_keys));
    if (new_size > ht->ma_keys->dk_log2_size) {
        if (_ht_resize(ht, new_size, update)) {
            return -1;
        }
    }

    while(PyDict_Next(kwds, &pos, &key, &value)) {
        Py_INCREF(key);
        identity = ht_calc_identity(ht, key);
        if (identity == NULL) {
            goto fail;
        }
        Py_hash_t hash = PyObject_Hash(identity);
        if (hash == -1) {
            goto fail;
        }
        if (update) {
            if (_ht_update(ht, hash, identity, key, value) < 0) {
                goto fail;
            }
        } else {
            if (_ht_add_with_hash(ht, hash, identity, key, value) < 0) {
                goto fail;
            }
        }
        Py_CLEAR(identity);
        Py_CLEAR(key);
    }
    return 0;
fail:
    Py_CLEAR(identity);
    Py_CLEAR(key);
    return -1;
}

static inline void _err_not_sequence(Py_ssize_t i)
{
    PyErr_Format(PyExc_TypeError,
                 "multidict cannot convert sequence element #%zd"
                 " to a sequence",
                 i);
}

static inline void _err_bad_length(Py_ssize_t i, Py_ssize_t n)
{
    PyErr_Format(PyExc_ValueError,
                 "multidict update sequence element #%zd "
                 "has length %zd; 2 is required",
                 i, n);
}

static inline void _err_cannot_fetch(Py_ssize_t i, const char * name)
{
    PyErr_Format(PyExc_ValueError,
                 "multidict update sequence element #%zd's "
                 "%s could not be fetched", name, i);
}


static int _ht_parse_item(Py_ssize_t i, PyObject *item,
                                 PyObject **pkey, PyObject **pvalue)
{
    Py_ssize_t n;

    if (PyList_CheckExact(item)) {
        n = PyList_GET_SIZE(item);
        if (n != 2) {
            _err_bad_length(i, n);
            goto fail;
        }
        *pkey = Py_NewRef(PyList_GET_ITEM(item, 0));
        *pvalue = Py_NewRef(PyList_GET_ITEM(item, 1));
    } else if (PyTuple_CheckExact(item)) {
        n = PyTuple_GET_SIZE(item);
        if (n != 2) {
            _err_bad_length(i, n);
            goto fail;
        }
        *pkey = Py_NewRef(PyTuple_GET_ITEM(item, 0));
        *pvalue = Py_NewRef(PyTuple_GET_ITEM(item, 1));
    } else {
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
        *pvalue = PySequence_ITEM(item, 1);
        if (*pkey == NULL) {
            _err_cannot_fetch(i, "key");
            goto fail;
        }
        if (*pvalue == NULL) {
            _err_cannot_fetch(i, "value");
            goto fail;
        }
    }
    return 0;
fail:
    Py_CLEAR(*pkey);
    Py_CLEAR(*pvalue);
    return -1;
}


static inline int
ht_update_from_seq(ht_t *ht, PyObject *seq, bool update)
{
    PyObject *it = NULL;
    PyObject *item = NULL; // seq[i]

    PyObject *key = NULL;
    PyObject *value = NULL;
    PyObject *identity = NULL;

    Py_ssize_t i;
    Py_ssize_t size = -1;

    enum {LIST, TUPLE, ITER} kind;

    Py_ssize_t length_hint = PyObject_LengthHint(seq, 0);
    if (length_hint < 0) {
        return -1;
    }
    uint8_t new_size = Py_MAX(
        estimate_log2_keysize(length_hint + ht->ma_used),
        DK_LOG_SIZE(ht->ma_keys));
    if (new_size > ht->ma_keys->dk_log2_size) {
        if (_ht_resize(ht, new_size, update)) {
            return -1;
        }
    }

    if (PyList_CheckExact(seq)) {
        kind = LIST;
        size = PyList_GET_SIZE(seq);
        if (size == 0) {
            return 0;
        }
    } else if (PyTuple_CheckExact(seq)) {
        kind = TUPLE;
        size = PyTuple_GET_SIZE(seq);
        if (size == 0) {
            return 0;
        }
    } else {
        kind = ITER;
        it = PyObject_GetIter(seq);
        if (it == NULL) {
            goto fail;
        }
    }

    for (i = 0; ; ++i) { // i - index into seq of current element
        switch (kind) {
        case LIST:
            if (i >= size) {
                goto exit;
            }
            item = PyList_GET_ITEM(seq, i);
            if (item == NULL) {
                goto fail;
            }
            Py_INCREF(item);
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
        case ITER:
            item = PyIter_Next(it);
            if (item == NULL) {
                if (PyErr_Occurred()) {
                    goto fail;
                }
                goto exit;
            }
        }

        if (_ht_parse_item(i, item, &key, &value) < 0) {
            goto fail;
        }

        identity = ht_calc_identity(ht, key);
        if (identity == NULL) {
            goto fail;
        }

        Py_hash_t hash = PyObject_Hash(identity);
        if (hash == -1) {
            goto fail;
        }

        if (update) {
            if (_ht_update(ht, hash, identity, key, value) < 0) {
                goto fail;
            }
            Py_CLEAR(identity);
            Py_CLEAR(key);
            Py_CLEAR(value);
        } else {
            if (_ht_add_with_hash_steal_refs(ht, hash, identity,
                                             key, value) < 0) {
                goto fail;
            }
            identity = NULL;
            key = NULL;
            value = NULL;
        }
        Py_CLEAR(item);
    }

exit:
    Py_CLEAR(it);
    return 0;

fail:
    Py_CLEAR(identity);
    Py_CLEAR(it);
    Py_CLEAR(item);
    Py_CLEAR(key);
    Py_CLEAR(value);
    return -1;
}


static inline int
ht_eq(ht_t *ht, ht_t *other)
{
    if (ht == other) {
        return 1;
    }

    if (ht_len(ht) != ht_len(other)) {
        return 0;
    }

    Py_ssize_t pos1 = 0;
    Py_ssize_t pos2 = 0;

    entry_t *lft_entries = DK_ENTRIES(ht->ma_keys);
    entry_t *rht_entries = DK_ENTRIES(other->ma_keys);
    for (;;) {
        if (pos1 >= ht->ma_keys->dk_nentries
            || pos2 >= other->ma_keys->dk_nentries) {
            return 1;
        }
        entry_t *entry1 = lft_entries + pos1;
        if (entry1->identity == NULL) {
            pos1++;
            continue;
        }
        entry_t *entry2 = rht_entries +pos2;
        if (entry1->identity == NULL) {
            pos2++;
            continue;
        }

        if (entry1->hash != entry2->hash) {
            return 0;
        }

        int cmp = PyObject_RichCompareBool(entry1->identity, entry2->identity, Py_EQ);
        if (cmp < 0) {
            return -1;
        };
        if (cmp == 0) {
            return 0;
        }

        cmp = PyObject_RichCompareBool(entry1->value, entry2->value, Py_EQ);
        if (cmp < 0) {
            return -1;
        };
        if (cmp == 0) {
            return 0;
        }
        pos1++;
        pos2++;
    }
    return 1;
}

static inline int
ht_eq_to_mapping(ht_t *ht, PyObject *other)
{
    PyObject *key = NULL;
    PyObject *avalue = NULL;
    PyObject *bvalue;

    Py_ssize_t other_len;

    if (!PyMapping_Check(other)) {
        PyErr_Format(PyExc_TypeError,
                     "other argument must be a mapping, not %s",
                     Py_TYPE(other)->tp_name);
        return -1;
    }

    other_len = PyMapping_Size(other);
    if (other_len < 0) {
        return -1;
    }
    if (ht_len(ht) != other_len) {
        return 0;
    }

    ht_pos_t pos;
    ht_init_pos(ht, &pos);

    for(;;) {
        int ret = ht_next(ht, &pos, NULL, &key, &avalue);
        if (ret < 0) {
            return -1;
        }
        if (ret == 0) {
            break;
        }
        ret = PyMapping_GetOptionalItem(other, key, &bvalue);
        Py_CLEAR(key);
        if (ret < 0) {
            Py_CLEAR(avalue);
            return -1;
        }

        if (bvalue == NULL) {
            Py_CLEAR(avalue);
            return 0;
        }

        int eq = PyObject_RichCompareBool(avalue, bvalue, Py_EQ);
        Py_CLEAR(bvalue);
        Py_CLEAR(avalue);

        if (eq <= 0) {
            return eq;
        }
    }

    return 1;
}


static inline PyObject *
ht_repr(ht_t *ht, PyObject *name,
                   bool show_keys, bool show_values)
{
    PyObject *key = NULL;
    PyObject *value = NULL;

    bool comma = false;
    uint64_t version = ht->version;

    PyUnicodeWriter *writer = PyUnicodeWriter_Create(1024);
    if (writer == NULL)
        return NULL;

    if (PyUnicodeWriter_WriteChar(writer, '<') <0)
        goto fail;
    if (PyUnicodeWriter_WriteStr(writer, name) <0)
        goto fail;
    if (PyUnicodeWriter_WriteChar(writer, '(') <0)
        goto fail;

    entry_t *entries = DK_ENTRIES(ht->ma_keys);

    for (Py_ssize_t pos = 0; pos < ht->ma_keys->dk_nentries; ++pos) {
        if (version != ht->version) {
            PyErr_SetString(PyExc_RuntimeError, "MultiDict changed during iteration");
            return NULL;
        }
        entry_t *entry = entries + pos;
        if (entry->identity == NULL) {
            continue;
        }
        key = Py_NewRef(entry->key);
        value = Py_NewRef(entry->value);

        if (comma) {
            if (PyUnicodeWriter_WriteChar(writer, ',') <0)
                goto fail;
            if (PyUnicodeWriter_WriteChar(writer, ' ') <0)
                goto fail;
        }
        if (show_keys) {
            if (PyUnicodeWriter_WriteChar(writer, '\'') <0)
                goto fail;
            /* Don't need to convert key to istr, the text is the same*/
            if (PyUnicodeWriter_WriteStr(writer, key) <0)
                goto fail;
            if (PyUnicodeWriter_WriteChar(writer, '\'') <0)
                goto fail;
        }
        if (show_keys && show_values) {
            if (PyUnicodeWriter_WriteChar(writer, ':') <0)
                goto fail;
            if (PyUnicodeWriter_WriteChar(writer, ' ') <0)
                goto fail;
        }
        if (show_values) {
            if (PyUnicodeWriter_WriteRepr(writer, value) <0)
                goto fail;
        }

        comma = true;
        Py_CLEAR(key);
        Py_CLEAR(value);
    }

    if (PyUnicodeWriter_WriteChar(writer, ')') <0)
        goto fail;
    if (PyUnicodeWriter_WriteChar(writer, '>') <0)
        goto fail;
    return PyUnicodeWriter_Finish(writer);
fail:
    Py_CLEAR(key);
    Py_CLEAR(value);
    PyUnicodeWriter_Discard(writer);
    return NULL;
}



/***********************************************************************/

static inline int
ht_traverse(ht_t *ht, visitproc visit, void *arg)
{
    if (ht->ma_used == 0) {
        return 0;
    }

    entry_t *entries = DK_ENTRIES(ht->ma_keys);
    for (Py_ssize_t pos = 0; pos < ht->ma_keys->dk_nentries; pos++) {
        entry_t *entry = entries + pos;
        if (entry->identity != NULL) {
            Py_VISIT(entry->identity);
            Py_VISIT(entry->key);
            Py_VISIT(entry->value);
        }
    }

    return 0;
}


static inline int
ht_clear(ht_t *ht)
{
    if (ht->ma_used == 0) {
        return 0;
    }
    ht->version = NEXT_VERSION();

    entry_t *entries = DK_ENTRIES(ht->ma_keys);
    for (Py_ssize_t pos = 0; pos < ht->ma_keys->dk_nentries; pos++) {
        entry_t *entry = entries + pos;
        if (entry->identity != NULL) {
            Py_CLEAR(entry->identity);
            Py_CLEAR(entry->key);
            Py_CLEAR(entry->value);
        }
    }

    ht->ma_used = 0;
    if (ht->ma_keys != &empty_htkeys) {
        htkeys_free(ht->ma_keys);
        ht->ma_keys = &empty_htkeys;
    }

    return 0;
}


static inline void
ht_dealloc(ht_t *ht)
{
    if (ht->ma_keys == NULL) {
        return;
    }
    if (ht->ma_keys != &empty_htkeys) {
        entry_t *entries = DK_ENTRIES(ht->ma_keys);
        for (Py_ssize_t idx=0; idx<ht->ma_keys->dk_nentries; ++idx) {
            entry_t *entry = entries + idx;
            Py_XDECREF(entry->identity);
            Py_XDECREF(entry->key);
            Py_XDECREF(entry->value);
        }

        htkeys_free(ht->ma_keys);
        ht->ma_keys = &empty_htkeys;
    }
}


static inline int
_ht_check_consistency(ht_t *ht, bool update)
{
//    ASSERT_WORLD_STOPPED_OR_DICT_LOCKED(op);

#define CHECK(expr) assert(expr)
//    do { if (!(expr)) { assert(0 && Py_STRINGIFY(expr)); } } while (0)

    htkeys_t *keys = ht->ma_keys;
    CHECK(keys != NULL);
    Py_ssize_t usable = USABLE_FRACTION(DK_SIZE(keys));

    // In the free-threaded build, shared keys may be concurrently modified,
    // so use atomic loads.
    Py_ssize_t dk_usable = keys->dk_usable;
    Py_ssize_t dk_nentries = keys->dk_nentries;

    CHECK(0 <= ht->ma_used && ht->ma_used <= usable);
    CHECK(0 <= dk_usable && dk_usable <= usable);
    CHECK(0 <= dk_nentries && dk_nentries <= usable);
    CHECK(dk_usable + dk_nentries <= usable);

    for (Py_ssize_t i=0; i < DK_SIZE(keys); i++) {
        Py_ssize_t ix = htkeys_get_index(keys, i);
        CHECK(DKIX_DUMMY <= ix && ix <= usable);
    }

    entry_t *entries = DK_ENTRIES(keys);
    for (Py_ssize_t i=0; i < usable; i++) {
        entry_t *entry = &entries[i];
        PyObject *identity = entry->identity;

        if (identity != NULL) {
            if (!update) {
                CHECK(entry->hash != -1);
                CHECK(entry->key != NULL);
                CHECK(entry->value != NULL);
            } else {
                if (entry->key == NULL) {
                    CHECK(entry->value == NULL);
                } else {
                    CHECK(entry->value != NULL);
                }
            }

            CHECK(PyUnicode_CheckExact(identity));
            if (entry->hash != -1) {
                Py_hash_t hash = PyObject_Hash(identity);
                CHECK(entry->hash == hash);
            }
        }
    }
    return 1;

#undef CHECK
}


static inline int
_ht_dump(ht_t *ht)
{
    htkeys_t *keys = ht->ma_keys;
    printf("Dump %p [%ld from %ld usable %ld nentries %ld]\n",
           ht, ht->ma_used, DK_SIZE(keys), keys->dk_usable, keys->dk_nentries);
    for (Py_ssize_t i=0; i < DK_SIZE(keys); i++) {
        Py_ssize_t ix = htkeys_get_index(keys, i);
        printf("  %ld -> %ld\n", i, ix);
    }
    printf("  --------\n");
    entry_t *entries = DK_ENTRIES(keys);
    for (Py_ssize_t i=0; i < keys->dk_nentries; i++) {
        entry_t *entry = &entries[i];
        PyObject *identity = entry->identity;

        if (identity == NULL) {
            printf("  %ld [deleted]\n", i);
        } else {
            printf("  %ld h=%20ld, i=\'", i, entry->hash);
            PyObject_Print(entry->identity, stdout, Py_PRINT_RAW);
            printf("\', k=\'");
            PyObject_Print(entry->key, stdout, Py_PRINT_RAW);
            printf("\', v=\'");
            PyObject_Print(entry->value, stdout, Py_PRINT_RAW);
            printf("\'\n");
        }
    }
    printf("\n");
    return 1;
}

#ifdef __cplusplus
}
#endif
#endif
