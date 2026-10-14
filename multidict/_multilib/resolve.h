#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_RESOLVE_H
#define _MULTIDICT_RESOLVE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <stdbool.h>
#include <stdint.h>

#include "compiler.h"
#include "dict.h"
#include "htkeys.h"
#include "str_cmp.h"

/* The identity md's table holds for a key equal to the one whose identity
   is `identity`, borrowed, or NULL if there is none, with *perr set on
   error. Runs __eq__ where str_cmp() cannot decide, which can change md;
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
            if (str_cmp(identity, other)) {
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

COLD static int
_md_resolve_probe(MultiDictObject* md, PyObject** pidentity, Py_hash_t hash)
{
    bool err = false;
    PyObject* found = md_resolve(md, *pidentity, hash, &err);
    if (found == NULL) {
        return err ? -1 : 0;
    }
    *pidentity = found;
    return 1;
}

/* Readies the identity a lookup probes md's table with: 1 to probe with
   *pidentity, which may now be the table's own, borrowed; 0 if no key in
   md is equal; -1 on error. A KIND_GENERAL table's walks match by
   address, and the others' compare exact str identities only, so a probe
   of the one, or whose identity is no exact str, is matched here first.
   That can run Python code; the caller holds md's critical section. */
static inline int
md_resolve_probe(MultiDictObject* md, PyObject** pidentity, Py_hash_t hash)
{
    if (UNLIKELY(!PyUnicode_CheckExact(*pidentity)) ||
        UNLIKELY(md->keys->kind == KIND_GENERAL)) {
        return _md_resolve_probe(md, pidentity, hash);
    }
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif
