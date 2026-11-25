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

/* The identity md's table holds for a key equal to the one whose identity
   is `identity`, borrowed, or NULL if there is none, with *perr set on
   error. Runs __eq__ where two str cannot decide, which can change md;
   the walk then starts over. */
COLD static PyObject*
md_resolve(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
           bool* perr);

/* md_resolve_any()'s result, by value so the caller's identity stays in
   a register: `found` is 1 to probe with `identity`, which may be the
   table's own, borrowed; 0 if no key in md is equal; -1 on error. */
typedef struct {
    PyObject* identity;
    int found;
} probe_t;

COLD static probe_t
md_resolve_any(MultiDictObject* md, PyObject* identity, Py_hash_t hash);

/* md_find_first_general()'s result: `entry` the first entry of a key
   equal to the probe, NULL if none, `result` 1, 0 or -1 on error. */
typedef struct {
    entry_t* entry;
    int result;
} found_t;

/* HTKEYSITER_FIND_FIRST() for a probe whose identity is no exact str, or
   in a KIND_GENERAL table: resolved first, which can run __eq__. Kept off
   the str lookups, which would otherwise pay for its call. The caller
   holds md's critical section. */
COLD static found_t
md_find_first_general(MultiDictObject* md, PyObject* identity, Py_hash_t hash);

#ifdef __cplusplus
}
#endif

#endif
