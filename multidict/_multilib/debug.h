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

static void
_md_check_entry(uint8_t kind, bool ci, const entry_t* entry)
{
    PyObject* identity;
    if (kind_is_compact(kind)) {
        identity = compact_entry_is_hole(entry)
                       ? NULL
                       : compact_entry_identity(ci, entry);
    } else {
        identity = as_const_anystr(entry)->identity;
    }
    if (identity != NULL) {
        assert(entry->key != NULL);
        assert(entry->value != NULL);
        assert(PyUnicode_CheckExact(identity));
        assert((kind_is_compact(kind)
                    ? compact_entry_hash(ci, entry)
                    : as_const_anystr(entry)->hash) == unicode_hash(identity));
    } else {
        assert(entry->key == NULL);
    }
}

static int
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
        Py_ssize_t ix = HTKEYS_GET_INDEX(keys, i);
        CHECK(DKIX_DUMMY <= ix && ix <= calc_usable);
    }

    if (kind_is_compact(keys->kind)) {
        entry_t* entries = HTKEYS_COMPACT_ENTRIES(keys);
        for (Py_ssize_t i = 0; i < calc_usable; i++) {
            _md_check_entry(KIND_COMPACT, ci, entries + i);
        }
    } else {
        anystr_entry_t* entries = HTKEYS_ANYSTR_ENTRIES(keys);
        for (Py_ssize_t i = 0; i < calc_usable; i++) {
            _md_check_entry(KIND_ANYSTR, ci, &entries[i].base);
        }
    }
    return 1;

#undef CHECK
}

/* For a debugger: nothing calls these, and inline keeps the unused copies
   from warning. */
static inline void
_md_dump_entry(uint8_t kind, bool ci, Py_ssize_t i, const entry_t* entry)
{
    PyObject* identity;
    if (kind_is_compact(kind)) {
        identity = compact_entry_is_hole(entry)
                       ? NULL
                       : compact_entry_identity(ci, entry);
    } else {
        identity = as_const_anystr(entry)->identity;
    }
    if (identity == NULL) {
        printf("  %zd [deleted]\n", i);
        return;
    }
    Py_hash_t hash = kind_is_compact(kind) ? compact_entry_hash(ci, entry)
                                           : as_const_anystr(entry)->hash;
    printf("  %zd h=%20zd, i=\'", i, hash);
    PyObject_Print(identity, stdout, Py_PRINT_RAW);
    printf("\', k=\'");
    PyObject_Print(entry->key, stdout, Py_PRINT_RAW);
    printf("\', v=\'");
    PyObject_Print(entry->value, stdout, Py_PRINT_RAW);
    printf("\'\n");
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
        Py_ssize_t ix = HTKEYS_GET_INDEX(keys, i);
        printf("  %zd -> %zd\n", i, ix);
    }
    printf("  --------\n");
    if (kind_is_compact(keys->kind)) {
        entry_t* entries = HTKEYS_COMPACT_ENTRIES(keys);
        for (Py_ssize_t i = 0; i < keys->nentries; i++) {
            _md_dump_entry(KIND_COMPACT, ci, i, entries + i);
        }
    } else {
        anystr_entry_t* entries = HTKEYS_ANYSTR_ENTRIES(keys);
        for (Py_ssize_t i = 0; i < keys->nentries; i++) {
            _md_dump_entry(KIND_ANYSTR, ci, i, &entries[i].base);
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
