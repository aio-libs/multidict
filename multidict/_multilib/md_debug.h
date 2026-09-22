#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_MD_DEBUG_H
#define _MULTIDICT_MD_DEBUG_H

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
_md_check_consistency(MultiDictObject* md, bool update)
{
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
        entry_t* entry = &entries[i];
        PyObject* identity = entry->identity;

        if (identity != NULL) {
#ifdef Py_GIL_DISABLED
            /* `update` describes only this call's own operation, not
               whether some entirely different, concurrently-suspended
               thread's _md_update() (on some other key) currently has
               an entry of its own half-deleted (key == NULL, identity
               kept) pending that
               thread's own cleanup -- critical section suspension
               means that can be true regardless of what this call's
               update flag says. So always use the tolerant checks
               here; the strict !update ones remain meaningful only
               where nothing else can be concurrently mid-operation,
               i.e. the GIL build below. */
            if (entry->key == NULL) {
                CHECK(entry->value == NULL);
            } else {
                CHECK(entry->value != NULL);
            }
#else
            if (!update) {
                CHECK(entry->key != NULL);
                CHECK(entry->value != NULL);
            } else {
                if (entry->key == NULL) {
                    CHECK(entry->value == NULL);
                } else {
                    CHECK(entry->value != NULL);
                }
            }
#endif

            CHECK(PyUnicode_CheckExact(identity));
            CHECK(entry->hash == _unicode_hash(identity));
        }
    }
    return 1;

#undef CHECK
}

static inline int
_md_dump(MultiDictObject* md)
{
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
        entry_t* entry = &entries[i];
        PyObject* identity = entry->identity;

        if (identity == NULL) {
            printf("  %zd [deleted]\n", i);
        } else {
            printf("  %zd h=%20zd, i=\'", i, entry->hash);
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

#define ASSERT_CONSISTENT(md, update) assert(_md_check_consistency(md, update))
#else
#define ASSERT_CONSISTENT(md, update) assert(1)
#endif  // NDEBUG

#ifdef __cplusplus
}
#endif
#endif
