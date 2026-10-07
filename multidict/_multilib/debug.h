#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_DEBUG_H
#define _MULTIDICT_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <stdbool.h>
#include <stdio.h>

#include "dict.h"
#include "htkeys.h"

#ifndef NDEBUG

static inline int
_md_check_consistency(const MultiDictObject* md)
{
    bool ci = md->is_ci;
    //    ASSERT_WORLD_STOPPED_OR_DICT_LOCKED(op);

#define CHECK(expr) assert(expr)
    //    do { if (!(expr)) { assert(0 && Py_STRINGIFY(expr)); } } while (0)

    htkeys_t* keys = md->keys;
    CHECK(keys != NULL);
    Py_ssize_t calc_usable = USABLE_FRACTION(htkeys_nslots(keys));

    Py_ssize_t usable = keys->usable;
    Py_ssize_t nentries = keys->nentries;

    CHECK(0 <= md->used && md->used <= calc_usable);
    CHECK(0 <= usable && usable <= calc_usable);
    CHECK(0 <= nentries && nentries <= calc_usable);
    CHECK(usable + nentries <= calc_usable);

    for (Py_ssize_t i = 0; i < htkeys_nslots(keys); i++) {
        Py_ssize_t ix = htkeys_get_index(keys, i);
        CHECK(DKIX_DUMMY <= ix && ix <= calc_usable);
    }

    entry_t* entries = htkeys_entries(keys);
    for (Py_ssize_t i = 0; i < calc_usable; i++) {
        entry_t* entry = entry_at(keys->kind, entries, i);
        PyObject* identity = entry_identity(keys->kind, ci, entry);

        if (identity != NULL) {
            CHECK(entry->key != NULL);
            CHECK(entry->value != NULL);
            CHECK(PyUnicode_CheckExact(identity));
            CHECK(entry_hash(keys->kind, ci, entry) == unicode_hash(identity));
        } else {
            CHECK(entry->key == NULL);
        }
    }

    // keys() skips its duplicate probes while maybe_dups is clear
    for (Py_ssize_t i = 0; !keys->maybe_dups && i < nentries; i++) {
        entry_t* entry = entry_at(keys->kind, entries, i);
        if (entry_is_hole(entry)) {
            continue;
        }
        Py_hash_t hash = entry_hash(keys->kind, md->is_ci, entry);
        htkeysiter_t iter;
        htkeysiter_init(&iter, keys, hash);
        for (; iter.index != i; htkeysiter_next(&iter)) {
            CHECK(iter.index != DKIX_EMPTY);
            if (iter.index >= 0) {
                entry_t* other = entry_at(keys->kind, entries, iter.index);
                CHECK(entry_hash(keys->kind, md->is_ci, other) != hash);
            }
        }
    }
    return 1;

#undef CHECK
}

static inline int
_md_dump(MultiDictObject* md)
{
    bool ci = md->is_ci;
    htkeys_t* keys = md->keys;
    printf("Dump %p [%zd from %zd usable %zd nentries %zd]\n",
           (void*)md,
           md->used,
           htkeys_nslots(keys),
           keys->usable,
           keys->nentries);
    for (Py_ssize_t i = 0; i < htkeys_nslots(keys); i++) {
        Py_ssize_t ix = htkeys_get_index(keys, i);
        printf("  %zd -> %zd\n", i, ix);
    }
    printf("  --------\n");
    entry_t* entries = htkeys_entries(keys);
    for (Py_ssize_t i = 0; i < keys->nentries; i++) {
        entry_t* entry = entry_at(keys->kind, entries, i);
        PyObject* identity = entry_identity(keys->kind, ci, entry);

        if (identity == NULL) {
            printf("  %zd [deleted]\n", i);
        } else {
            printf(
                "  %zd h=%20zd, i=\'", i, entry_hash(keys->kind, ci, entry));
            PyObject_Print(
                entry_identity(keys->kind, ci, entry), stdout, Py_PRINT_RAW);
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

#define ASSERT_CONSISTENT(md) assert(_md_check_consistency(md))
#else
#define ASSERT_CONSISTENT(md) assert(1)
#endif  // NDEBUG

#ifdef __cplusplus
}
#endif
#endif
