#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_GENERAL_H
#define _MULTIDICT_GENERAL_H

#ifdef __cplusplus
extern "C" {
#endif

/* The paths a KIND_GENERAL table, or a key that is no str, takes: declared
   where they are called, defined here, last, so that each str path keeps
   only its call. */

#include <Python.h>

#include "bulk_update.h"
#include "eq.h"
#include "hashtable.h"
#include "resolve.h"
#include "to_dict.h"
#include "walk.h"

/* The identity md's table holds for a key equal to the one whose identity
   is `identity`, borrowed, or NULL if there is none, with *perr set on
   error. Runs __eq__ where two str cannot decide, which can change md;
   the walk then starts over. */
COLD static PyObject*
md_resolve(MultiDictObject* md, PyObject* identity, Py_hash_t hash, bool* perr)
{
    bool ci = md->is_ci;
    bool str = PyUnicode_CheckExact(identity);
restart:;
    htkeys_t* keys = md->keys;
    uint64_t version = md->version;
    bool compact = kind_is_compact(keys->kind);
    htkeysiter_t iter;
    if (keys->kind == KIND_GENERAL) {
        /* An identity the table already holds, as one taken from md
           itself is, needs no __eq__ at all. */
        entry_t* entry = NULL;
        HTKEYSITER_INIT(&iter, keys, hash);
        HTKEYSITER_FIND_GENERAL(&iter, identity, hash, entry);
        if (entry != NULL) {
            return identity;
        }
    }
    HTKEYSITER_INIT(&iter, keys, hash);
    for (; iter.index != DKIX_EMPTY; HTKEYSITER_NEXT(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        PyObject* other;
        Py_hash_t other_hash;
        if (compact) {
            entry_t* e = HTKEYS_COMPACT_ENTRIES(keys) + iter.index;
            other_hash =
                ci ? compact_key_hash_ci(e->key) : compact_key_hash_cs(e->key);
            other = ci ? compact_key_identity_ci(e->key)
                       : compact_key_identity_cs(e->key);
        } else {
            anystr_entry_t* e = HTKEYS_ANYSTR_ENTRIES(keys) + iter.index;
            other_hash = e->hash;
            other = e->identity;
        }
        if (other_hash != hash) {
            continue;
        }
        if (other == identity) {
            return other;
        }
        if (str && PyUnicode_CheckExact(other)) {
            // not str_cmp(): another caller of it pushes it out of line
            if (PyUnicode_Compare(identity, other) == 0) {
                return other;
            }
            continue;
        }
        Py_INCREF(other);
        int cmp = PyObject_RichCompareBool(other, identity, Py_EQ);
        Py_DECREF(other);
        if (UNLIKELY(cmp < 0)) {
            *perr = true;
            return NULL;
        }
        if (md->keys != keys || md->version != version) {
            goto restart;
        }
        if (cmp > 0) {
            return other;
        }
    }
    return NULL;
}

COLD static probe_t
md_resolve_any(MultiDictObject* md, PyObject* identity, Py_hash_t hash)
{
    bool err = false;
    probe_t ret = {md_resolve(md, identity, hash, &err), 1};
    if (ret.identity == NULL) {
        ret.identity = identity;
        ret.found = err ? -1 : 0;
    }
    return ret;
}

/* HTKEYSITER_FIND_FIRST() for a probe whose identity is no exact str, or
   in a KIND_GENERAL table: resolved first, which can run __eq__. Kept off
   the str lookups, which would otherwise pay for its call. The caller
   holds md's critical section. */
COLD static found_t
md_find_first_general(MultiDictObject* md, PyObject* identity, Py_hash_t hash)
{
    found_t ret = {NULL, 0};
    probe_t probed = md_resolve_any(md, identity, hash);
    if (probed.found <= 0) {
        ret.result = probed.found;
        return ret;
    }
    identity = probed.identity;
    bool ci = md->is_ci;
    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, md->keys, hash);
    HTKEYSITER_FIND_FIRST(
        &iter,
        ci,
        identity,
        hash,
        ret.entry,
        HTKEYSITER_FIND_GENERAL(&iter, identity, hash, ret.entry));
    ret.result = ret.entry != NULL;
    return ret;
}

/* Gives every live entry of `keys`, a KIND_GENERAL table just built from
   a table of str identities and indexed, the identity object of the first
   entry equal to it, as the kind requires. Those identities are exact
   str, so nothing here runs Python code; `dups` is the source table's
   maybe_dups, without which no two of them are equal. */
COLD static void
htkeys_intern(htkeys_t* keys, bool dups)
{
    assert(keys->kind == KIND_GENERAL);
    if (!dups) {
        return;
    }
    anystr_entry_t* entries = HTKEYS_ANYSTR_ENTRIES(keys);
    for (Py_ssize_t i = 0; i < keys->nentries; i++) {
        anystr_entry_t* e = entries + i;
        if (entry_is_hole((const entry_t*)e)) {
            continue;
        }
        htkeysiter_t iter;
        HTKEYSITER_INIT(&iter, keys, e->hash);
        for (; iter.index != i; HTKEYSITER_NEXT(&iter)) {
            assert(iter.index != DKIX_EMPTY);
            if (iter.index < 0) {
                continue;
            }
            anystr_entry_t* first = entries + iter.index;
            if (first->hash == e->hash && first->identity != e->identity &&
                PyUnicode_Compare(e->identity, first->identity) == 0) {
                // an exact str: freeing it runs no code
                Py_SETREF(e->identity, Py_NewRef(first->identity));
                break;
            }
        }
    }
}

/* _md_del_locked() for a KIND_GENERAL table, or a probe whose identity is
   no exact str: resolved first, which can run __eq__. */
COLD static int
md_del_general(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
               removed_pairs_t* removed, bool watched)
{
    probe_t probed = md_resolve_any(md, identity, hash);
    if (probed.found <= 0) {
        return probed.found;
    }
    identity = probed.identity;
    if (md->keys->kind != KIND_GENERAL) {
        // matched to a str key, by its own __eq__
        return _md_del_locked(md, identity, hash, removed, watched);
    }
    bool found = false;
    int ret = 0;
    htkeys_t* keys = md->keys;
    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, keys, hash);
    entry_t* entry = NULL;
    for (;;) {
        HTKEYSITER_FIND_GENERAL(&iter, identity, hash, entry);
        if (entry == NULL) {
            break;
        }
        _md_del_record(md, entry, identity, hash, watched, !found);
        ret = _md_del_at_held_general(
            md, keys, iter.slot, as_anystr(entry), removed);
        found = true;
        if (ret < 0 || !keys->maybe_dups) {
            break;  // on error, or past the key's only entry
        }
    }

    if (found) {
        bump_version(md);
        if (watched) {
            MD_WATCH_RECORD_SIMPLE(md, MultiDict_EVENT_BATCH_END);
        }
        if (ret == 0) {
            ret = 1;
        }
    }
    ASSERT_CONSISTENT(md);
    return ret;
}

/* _md_pop_one_locked() for a KIND_GENERAL table, or a probe whose identity
   is no exact str: resolved first, which can run __eq__. */
COLD static int
md_pop_one_general(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                   PyObject** ret, bool watched)
{
    probe_t probed = md_resolve_any(md, identity, hash);
    if (probed.found <= 0) {
        return probed.found;
    }
    identity = probed.identity;
    if (md->keys->kind != KIND_GENERAL) {
        // matched to a str key, by its own __eq__
        return _md_pop_one_locked(md, identity, hash, ret, watched);
    }
    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, md->keys, hash);
    entry_t* entry = NULL;
    HTKEYSITER_FIND_GENERAL(&iter, identity, hash, entry);
    if (entry == NULL) {
        return 0;
    }
    PyObject* value = _md_pop_one_record(md, entry, identity, hash, watched);
    PyObject* old_identity;
    PyObject* old_key;
    PyObject* old_value;
    _md_unlink_at_anystr(
        md, iter.slot, as_anystr(entry), &old_identity, &old_key, &old_value);
    Py_XDECREF(old_identity);
    Py_XDECREF(old_key);
    Py_XDECREF(old_value);
    *ret = value;
    bump_version(md);
    ASSERT_CONSISTENT(md);
    return 1;
}

/* _md_replace_pass() for a KIND_GENERAL table, `identity` being its own. */
static int
_md_replace_pass_general(MultiDictObject* md, PyObject* key, PyObject* value,
                         PyObject* identity, Py_hash_t hash,
                         PyObject** old_key_out, PyObject** old_value_out,
                         reflist_t** dups, bool watched, bool* pfound)
{
    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, md->keys, hash);
    Py_ssize_t replaced = -1;
    bool skip_first = *pfound;
    int ret = 0;
    entry_t* entry = NULL;
    while (ret == 0) {
        HTKEYSITER_FIND_GENERAL(&iter, identity, hash, entry);
        if (entry == NULL) {
            break;
        }
        if (iter.index == replaced) {
            continue;
        }
        if (skip_first || !*pfound) {
            replaced = iter.index;
            if (skip_first) {
                skip_first = false;
                continue;
            }
        }
        if (!*pfound) {
            _md_replace_key_anystr(as_anystr(entry), key, old_key_out);
        }
        ret = _md_replace_matched(md,
                                  iter.slot,
                                  entry,
                                  key,
                                  value,
                                  identity,
                                  hash,
                                  old_value_out,
                                  dups,
                                  watched,
                                  pfound);
        if (!md->keys->maybe_dups) {
            break;  // the key's only entry
        }
    }
    if (ret != 0) {
        return ret;
    }
    return *pfound ? 0 : 1;
}

/* _md_replace_locked() for a KIND_GENERAL table, or a key that is no str:
   its probe resolved first, which can run __eq__. Storing a key that is
   no str needs a KIND_GENERAL table. */
COLD static int
md_replace_general(MultiDictObject* md, PyObject* key, PyObject* value,
                   PyObject* identity, Py_hash_t hash, PyObject** old_key_out,
                   PyObject** old_value_out, reflist_t** dups, bool watched,
                   bool found)
{
    int ret;
    do {
        probe_t probed = md_resolve_any(md, identity, hash);
        if (probed.found <= 0) {
            return probed.found < 0 ? -1 : (found ? 0 : 1);
        }
        uint8_t need = md_key_need(md, key, identity);
        if (md->keys->kind < need && UNLIKELY(md_to_kind(md, need) < 0)) {
            return -1;
        }
        ret = _md_replace_pass_general(md,
                                       key,
                                       value,
                                       probed.identity,
                                       hash,
                                       old_key_out,
                                       old_value_out,
                                       dups,
                                       watched,
                                       &found);
    } while (ret == MD_REPLACE_STALE);
    if (ret == 0) {
        bump_version(md);
    }
    return ret;
}

COLD static int
md_contains_general(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                    PyObject** pret)
{
    int result;
    Py_BEGIN_CRITICAL_SECTION(md);
    found_t found = md_find_first_general(md, identity, hash);
    result = found.result;
    if (result > 0 && pret != NULL) {
        *pret = md_ensure_key(md, found.entry);
        if (UNLIKELY(*pret == NULL)) {
            result = -1;
        }
    }
    Py_END_CRITICAL_SECTION();
    return result;
}

COLD static int
md_contains_general_key(MultiDictObject* md, PyObject* key)
{
    identity_hash_t ih = md_calc_identity_hash(md, key);
    if (UNLIKELY(ih.identity == NULL)) {
        return -1;
    }
    int result = md_contains_general(md, ih.identity, ih.hash, NULL);
    Py_DECREF(ih.identity);
    return result;
}

COLD static lookup_t
md_get_one_general(MultiDictObject* md, PyObject* key)
{
    lookup_t ret = {-1, NULL};
    identity_hash_t ih = md_calc_identity_hash(md, key);
    if (UNLIKELY(ih.identity == NULL)) {
        return ret;
    }
    Py_BEGIN_CRITICAL_SECTION(md);
    found_t found = md_find_first_general(md, ih.identity, ih.hash);
    ret.result = found.result;
    if (found.entry != NULL) {
        ret.value = Py_NewRef(found.entry->value);
    }
    Py_END_CRITICAL_SECTION();
    Py_DECREF(ih.identity);
    return ret;
}

/* _md_update() for a KIND_GENERAL table, or a key that is no str: its
   probe resolved first, which can run __eq__. */
COLD static int
md_update_general(md_update_state_t* st, Py_hash_t hash, PyObject* identity,
                  PyObject* key, PyObject* value, uint8_t need)
{
    MultiDictObject* md = st->md;
    update_marks_t* marks = &st->marks;
    probe_t probed = md_resolve_any(md, identity, hash);
    if (UNLIKELY(probed.found < 0)) {
        return -1;
    }
    if (md->keys->kind < need && UNLIKELY(md_to_kind(md, need) < 0)) {
        return -1;
    }
    if (UNLIKELY(update_marks_sync(marks, md) < 0)) {
        return -1;
    }
    bool found = false;
    bool added = false;
    if (probed.found > 0) {
        if (md->keys->kind != KIND_GENERAL) {
            // __eq__ replaced the table by one of str keys
            return _md_update(st, hash, probed.identity, key, value, need);
        }
        PyObject* computed = identity;
        identity = probed.identity;
        htkeysiter_t iter;
        HTKEYSITER_INIT(&iter, md->keys, hash);
        entry_t* entry = NULL;
        for (;;) {
            HTKEYSITER_FIND_GENERAL(&iter, identity, hash, entry);
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
        identity = computed;
        if (found) {
            return 0;
        }
    }
    // a miss adds under the identity computed from key
    return md_add_for_upd(md,
                          hash,
                          identity,
                          key,
                          value,
                          marks,
                          need,
                          added ? MD_SLOT_CHECK : MD_SLOT_FIND);
}

/* Entry `pos` of md's table, of any kind, with its identity and hash. */
static entry_t*
_md_eq_entry(MultiDictObject* md, Py_ssize_t pos, PyObject** pidentity,
             Py_hash_t* phash)
{
    htkeys_t* keys = md->keys;
    if (kind_is_compact(keys->kind)) {
        entry_t* e = HTKEYS_COMPACT_ENTRIES(keys) + pos;
        if (!entry_is_hole(e)) {
            *pidentity = md->is_ci ? compact_key_identity_ci(e->key)
                                   : compact_key_identity_cs(e->key);
            *phash = md->is_ci ? compact_key_hash_ci(e->key)
                               : compact_key_hash_cs(e->key);
        }
        return e;
    }
    anystr_entry_t* e = HTKEYS_ANYSTR_ENTRIES(keys) + pos;
    *pidentity = e->identity;
    *phash = e->hash;
    return &e->base;
}

/* The scan for tables either of which is KIND_GENERAL, whose identities
   need __eq__. One that ran holds its own references to both pairs, and
   ends the call as a value's __eq__ does. */
COLD static int
md_eq_scan_general(MultiDictObject* md, MultiDictObject* other,
                   Py_ssize_t* ppos1, Py_ssize_t* ppos2)
{
    Py_ssize_t pos1 = *ppos1;
    Py_ssize_t pos2 = *ppos2;
    for (;; pos1++, pos2++) {
        PyObject* identity1 = NULL;
        PyObject* identity2 = NULL;
        Py_hash_t hash1 = 0;
        Py_hash_t hash2 = 0;
        entry_t* entry1 = NULL;
        entry_t* entry2 = NULL;
        for (; pos1 < md->keys->nentries; pos1++) {
            entry1 = _md_eq_entry(md, pos1, &identity1, &hash1);
            if (!entry_is_hole(entry1)) {
                break;
            }
        }
        for (; pos2 < other->keys->nentries; pos2++) {
            entry2 = _md_eq_entry(other, pos2, &identity2, &hash2);
            if (!entry_is_hole(entry2)) {
                break;
            }
        }
        if (pos1 >= md->keys->nentries || pos2 >= other->keys->nentries) {
            return 1;
        }
        if (hash1 != hash2) {
            return 0;
        }
        if (identity1 == identity2 || (PyUnicode_CheckExact(identity1) &&
                                       PyUnicode_CheckExact(identity2))) {
            if (identity1 != identity2 &&
                PyUnicode_Compare(identity1, identity2) != 0) {
                return 0;
            }
            int cmp = _md_eq_values(entry1->value, entry2->value);
            if (cmp == MD_EQ_RAN) {
                *ppos1 = pos1 + 1;
                *ppos2 = pos2 + 1;
            }
            if (cmp != 1) {
                return cmp;
            }
            continue;
        }
        PyObject* value1 = Py_NewRef(entry1->value);
        PyObject* value2 = Py_NewRef(entry2->value);
        Py_INCREF(identity1);
        Py_INCREF(identity2);
        int cmp = PyObject_RichCompareBool(identity1, identity2, Py_EQ);
        if (cmp > 0) {
            cmp = _md_eq_values(value1, value2);
        }
        Py_DECREF(identity1);
        Py_DECREF(identity2);
        Py_DECREF(value1);
        Py_DECREF(value2);
        if (cmp <= 0) {
            return cmp;
        }
        *ppos1 = pos1 + 1;
        *ppos2 = pos2 + 1;
        return MD_EQ_RAN;
    }
}

COLD static int
md_to_dict_general(MultiDictObject* md, PyObject** ret)
{
    PyObject* dict = PyDict_New();
    if (UNLIKELY(dict == NULL)) {
        return -1;
    }
    PyObject* lst = NULL;
    uint64_t version = md->version;
    htkeys_t* keys = md->keys;
    bitmap_t collected;
    collected.summary = NULL;
    bitmap_init(&collected, keys, keys->nentries);
    _MD_TO_DICT_ANYSTR_LOOP(HTKEYSITER_FIND_GENERAL);
    bitmap_release(&collected);
    *ret = dict;
    return 0;
fail:
    bitmap_release(&collected);
    Py_XDECREF(lst);
    Py_DECREF(dict);
    return -1;
}

COLD static Py_ssize_t
md_walk_general(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                bool with_keys, md_item_visitor_t visitor, void* user_data)
{
    probe_t probed = md_resolve_any(md, identity, hash);
    if (probed.found <= 0) {
        return probed.found;
    }
    identity = probed.identity;
    if (md->keys->kind != KIND_GENERAL) {
        // matched to a str key, by its own __eq__
        return md_walk_with_hash(
            md, identity, hash, with_keys, visitor, user_data);
    }
    // the table's own, which the visitors could free
    PyObject* held = Py_NewRef(identity);
    uint64_t version = md->version;
    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, md->keys, hash);
    Py_ssize_t count = 0;
    Py_ssize_t last = -1;
    int ret = 1;
    bool dups = md->keys->maybe_dups;
    entry_t* entry = NULL;
    while (ret > 0) {
        HTKEYSITER_FIND_GENERAL(&iter, identity, hash, entry);
        if (entry == NULL) {
            break;
        }
        if (iter.index <= last) {
            continue;
        }
        last = iter.index;
        ret = _md_walk_matched(
            md, entry, identity, hash, with_keys, visitor, user_data, version);
        count += ret >= 0;
        if (!dups) {
            break;
        }
    }
    Py_DECREF(held);
    return ret < 0 ? -1 : count;
}

COLD static Py_ssize_t
md_merge_present_general(MultiDictObject* md, Py_hash_t hash,
                         PyObject* identity, update_marks_t* marks)
{
    probe_t probed = md_resolve_any(md, identity, hash);
    if (probed.found <= 0) {
        return probed.found < 0 ? MD_SLOT_ERROR : MD_SLOT_FIND;
    }
    identity = probed.identity;
    if (md->keys->kind != KIND_GENERAL) {
        // matched to a str key, by its own __eq__
        return _md_merge_present(md, hash, identity, marks);
    }
    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, md->keys, hash);
    Py_ssize_t slot = MD_SLOT_FIND;
    entry_t* entry = NULL;
    for (;;) {
        HTKEYSITER_FIND_GENERAL(&iter, identity, hash, entry);
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
md_pop_all_general(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                   reflist_t* values, removed_pairs_t* removed)
{
    probe_t probed = md_resolve_any(md, identity, hash);
    if (probed.found <= 0) {
        return probed.found;
    }
    identity = probed.identity;
    if (md->keys->kind != KIND_GENERAL) {
        // matched to a str key, by its own __eq__
        return _md_pop_all_locked(md, identity, hash, values, removed);
    }
    bool begun = false;
    int ret = 0;
    htkeys_t* keys = md->keys;
    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, keys, hash);
    entry_t* entry = NULL;
    for (;;) {
        HTKEYSITER_FIND_GENERAL(&iter, identity, hash, entry);
        if (entry == NULL) {
            break;
        }
        ret = _md_pop_all_record(md, entry, identity, hash, values, &begun);
        if (UNLIKELY(ret < 0)) {
            break;
        }
        ret = _md_del_at_held_general(
            md, keys, iter.slot, as_anystr(entry), removed);
        if (ret < 0 || !keys->maybe_dups) {
            break;  // on error, or past the key's only entry
        }
    }

    if (begun) {
        MD_WATCH_RECORD_SIMPLE(md, MultiDict_EVENT_BATCH_END);
    }
    ASSERT_CONSISTENT(md);
    return ret;
}

COLD static PyObject*
md_calc_key_general(MultiDictObject* md, PyObject* key)
{
    PyObject* identity = md_calc_identity(md, key);
    if (UNLIKELY(identity == NULL)) {
        return NULL;
    }
    PyObject* ret = md_calc_key(md, key, identity);
    Py_DECREF(identity);
    return ret;
}

COLD static int
md_get_all_general(MultiDictObject* md, PyObject* identity, PyObject** ret)
{
    Py_hash_t hash = PyObject_Hash(identity);
    if (UNLIKELY(hash == -1)) {
        return -1;
    }
    reflist_t values;
    reflist_init(&values);
    Py_ssize_t count;
    Py_BEGIN_CRITICAL_SECTION(md);
    count =
        md_walk_general(md, identity, hash, false, _md_getall_visit, &values);
    Py_END_CRITICAL_SECTION();
    return _md_values_to_list(&values, count < 0, ret);
}

COLD static int
md_set_default_general(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                       PyObject* key, PyObject* value, PyObject** result)
{
    found_t found = md_find_first_general(md, identity, hash);
    if (UNLIKELY(found.result < 0)) {
        return -1;
    }
    if (found.entry != NULL) {
        *result = Py_NewRef(found.entry->value);
        return 1;
    }
    // a miss adds under the identity computed from key
    if (UNLIKELY(md_add_with_hash(md,
                                  hash,
                                  identity,
                                  key,
                                  value,
                                  md_key_need(md, key, identity),
                                  MD_SLOT_FIND) < 0)) {
        return -1;
    }
    *result = Py_NewRef(value);
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif
