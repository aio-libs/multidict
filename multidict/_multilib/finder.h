#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_FINDER_H
#define _MULTIDICT_FINDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "bitmap.h"
#include "compiler.h"
#include "dict.h"
#include "htkeys.h"
#include "identity.h"

#define MD_FINDER_FEW 8

typedef struct _md_finder {
    MultiDictObject* md;
    htkeysiter_t iter;
    uint64_t version;
    Py_hash_t hash;
    PyObject* identity;  // borrowed ref
    /* Most keys have a handful of values, so the first few matches are
       kept in a short list and the bitmap only starts past it: on a big
       table, starting it means a heap allocation. */
    Py_ssize_t visited_few[MD_FINDER_FEW];
    Py_ssize_t nvisited_few;
    md_bitmap_t visited;
} md_finder_t;

static inline int
md_finder_init(MultiDictObject* md, PyObject* identity, md_finder_t* finder)
{
    finder->version = md->version;
    finder->md = md;
    finder->identity = identity;
    finder->hash = _unicode_hash(identity);
    if (finder->hash == -1) {
        return -1;
    }
    finder->nvisited_few = 0;
    htkeysiter_init(&finder->iter, finder->md->keys, finder->hash);
    return 0;
}

/* Past this many matches, the short list is moved into the bitmap. */
COLD static int
_md_finder_spill(md_finder_t* finder)
{
    md_bitmap_init(
        &finder->visited, finder->md->keys, finder->md->keys->nentries);
    finder->nvisited_few = MD_FINDER_FEW + 1;
    for (Py_ssize_t i = 0; i < MD_FINDER_FEW; i++) {
        if (md_bitmap_set(&finder->visited, finder->visited_few[i]) < 0) {
            return -1;
        }
    }
    return 0;
}

/* 1 if `index` was already returned by this walk, 0 if not (it is now
   recorded), -1 on error. */
static inline int
_md_finder_seen(md_finder_t* finder, Py_ssize_t index)
{
    Py_ssize_t n = finder->nvisited_few;
    if (n <= MD_FINDER_FEW) {
        /* A repeat is most often the slot just returned, which the next
           call re-examines, so the list is scanned from its end. */
        for (Py_ssize_t i = n - 1; i >= 0; i--) {
            if (finder->visited_few[i] == index) {
                return 1;
            }
        }
        if (n < MD_FINDER_FEW) {
            finder->visited_few[n] = index;
            finder->nvisited_few = n + 1;
            return 0;
        }
        if (_md_finder_spill(finder) < 0) {
            return -1;
        }
    }
    return md_bitmap_test_and_set(&finder->visited, index);
}

static inline int
md_find_next(md_finder_t* finder, PyObject** pkey, PyObject** pvalue)
{
    int ret = 0;
    assert(finder->iter.keys == finder->md->keys);
    if (finder->iter.keys != finder->md->keys ||
        finder->version != finder->md->version) {
        ret = -1;
        PyErr_SetString(PyExc_RuntimeError,
                        "MultiDict is changed during iteration");
        goto cleanup;
    }

    entry_t* entries = htkeys_entries(finder->md->keys);

    for (; finder->iter.index != DKIX_EMPTY; htkeysiter_next(&finder->iter)) {
        if (finder->iter.index < 0) {
            continue;
        }
        entry_t* entry = entries + finder->iter.index;
        if (entry->hash != finder->hash) {
            continue;
        }
        if (!_str_cmp(finder->identity, entry->identity)) {
            continue;
        }

        /* htkeysiter_next() can repeat a slot already seen in this scan
           (see its doc comment), and this scan never marks the table. */
        int seen = _md_finder_seen(finder, finder->iter.index);
        if (seen < 0) {
            ret = -1;
            goto cleanup;
        }
        if (seen) {
            continue;
        }

        if (pvalue) {
            *pvalue = Py_NewRef(entry->value);
        }
        if (pkey) {
            *pkey = _md_ensure_key(finder->md, entry);  // last entry access
            if (*pkey == NULL) {
                if (pvalue) {
                    Py_CLEAR(*pvalue);
                }
                ret = -1;
                goto cleanup;
            }
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

static inline void
md_finder_cleanup(md_finder_t* finder)
{
    if (finder->nvisited_few > MD_FINDER_FEW) {
        md_bitmap_release(&finder->visited);
    }
}

static inline int
md_get_all(MultiDictObject* md, PyObject* key, PyObject** ret)
{
    int tmp;
    PyObject* value = NULL;
    *ret = NULL;

    /* Not zero-initialized: the bitmap's inline buffer is 4 KB. Cleanup
       only needs `nvisited_few`. */
    md_finder_t finder;
    finder.nvisited_few = 0;

    PyObject* identity = md_calc_identity(md, key);
    if (identity == NULL) {
        goto fail;
    }

    if (md_finder_init(md, identity, &finder) < 0) {
        assert(PyErr_Occurred());
        goto fail;
    }

    while ((tmp = md_find_next(&finder, NULL, &value)) > 0) {
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

    md_finder_cleanup(&finder);
    Py_DECREF(identity);
    return *ret != NULL;
fail:
    md_finder_cleanup(&finder);
    Py_XDECREF(identity);
    Py_XDECREF(value);
    Py_CLEAR(*ret);
    return -1;
}

/* Collect every (key, value) pair matching `identity` into a fresh list,
   so that callers can run a custom __eq__ against it after the walk.

   `with_keys` selects values (false) or (key, value) tuples (true). */
static inline PyObject*
md_finder_collect(MultiDictObject* md, PyObject* identity, bool with_keys)
{
    md_finder_t finder;
    finder.nvisited_few = 0;
    PyObject* key = NULL;
    PyObject* value = NULL;
    PyObject* item;
    int tmp;

    PyObject* ret = PyList_New(0);
    if (ret == NULL) {
        return NULL;
    }

    if (md_finder_init(md, identity, &finder) < 0) {
        assert(PyErr_Occurred());
        Py_DECREF(ret);
        return NULL;
    }

    while ((tmp = md_find_next(&finder, with_keys ? &key : NULL, &value)) >
           0) {
        if (with_keys) {
            item = PyTuple_Pack(2, key, value);
            Py_CLEAR(key);
            Py_CLEAR(value);
            if (item == NULL) {
                goto fail;
            }
        } else {
            item = value;
            value = NULL;
        }
        tmp = PyList_Append(ret, item);
        Py_DECREF(item);
        if (tmp < 0) {
            goto fail;
        }
    }
    md_finder_cleanup(&finder);
    if (tmp < 0) {
        goto fail_no_cleanup;
    }
    return ret;
fail:
    md_finder_cleanup(&finder);
fail_no_cleanup:
    Py_CLEAR(key);
    Py_CLEAR(value);
    Py_DECREF(ret);
    return NULL;
}

#ifdef __cplusplus
}
#endif

#endif
