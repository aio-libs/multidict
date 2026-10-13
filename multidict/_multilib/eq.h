#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_EQ_H
#define _MULTIDICT_EQ_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <stdbool.h>

#include "compiler.h"
#include "dict.h"
#include "hashtable.h"
#include "htkeys.h"
#include "str_cmp.h"
#include "walk.h"

/* Compares two values of matching entries: 1 if equal, 0 if not, -1 on
   error, _MD_EQ_RAN when they compared equal through a value's __eq__,
   which can mutate either dict and free its keys. */
#define _MD_EQ_RAN 2

static inline int
_md_eq_values(PyObject* value1, PyObject* value2)
{
    if (value1 == value2) {
        return 1;
    }
    if (PyUnicode_CheckExact(value1) && PyUnicode_CheckExact(value2)) {
        return str_cmp(value1, value2);
    }
    Py_INCREF(value1);
    Py_INCREF(value2);
    int cmp = PyObject_RichCompareBool(value1, value2, Py_EQ);
    Py_DECREF(value1);
    Py_DECREF(value2);
    return cmp > 0 ? _MD_EQ_RAN : cmp;
}

/* The per-kind parts of _MD_DEFINE_EQ_SCAN(): CS and CI are a KIND_COMPACT
   table of a MultiDict and of a CIMultiDict. */
#define _MD_EQ_CS_T entry_t
#define _MD_EQ_CS_ENTRIES(keys) HTKEYS_COMPACT_ENTRIES(keys)
#define _MD_EQ_CS_IS_HOLE(e) entry_is_hole(e)
#define _MD_EQ_CS_HASH(e) compact_key_hash_cs((e)->key)
#define _MD_EQ_CS_IDENTITY(e) compact_key_identity_cs((e)->key)
#define _MD_EQ_CS_BASE(e) (e)
#define _MD_EQ_CI_T entry_t
#define _MD_EQ_CI_ENTRIES(keys) HTKEYS_COMPACT_ENTRIES(keys)
#define _MD_EQ_CI_IS_HOLE(e) entry_is_hole(e)
#define _MD_EQ_CI_HASH(e) compact_key_hash_ci((e)->key)
#define _MD_EQ_CI_IDENTITY(e) compact_key_identity_ci((e)->key)
#define _MD_EQ_CI_BASE(e) (e)
#define _MD_EQ_ANYSTR_T anystr_entry_t
#define _MD_EQ_ANYSTR_ENTRIES(keys) HTKEYS_ANYSTR_ENTRIES(keys)
#define _MD_EQ_ANYSTR_IS_HOLE(e) entry_is_hole((const entry_t*)(e))
#define _MD_EQ_ANYSTR_HASH(e) ((e)->hash)
#define _MD_EQ_ANYSTR_IDENTITY(e) ((e)->identity)
#define _MD_EQ_ANYSTR_BASE(e) (&(e)->base)

/* md_eq()'s scan of md's table, of kind K1, against other's, of kind K2,
   from *ppos1 and *ppos2: 1 if equal, 0 if not, -1 on error, _MD_EQ_RAN
   when a value's __eq__ ran, after which the caller carries on from
   *ppos1 and *ppos2 under the tables and kinds they have now. */
#define _MD_DEFINE_EQ_SCAN(K1, K2)                                          \
    COLD static int _md_eq_scan_##K1##_##K2(MultiDictObject* md,            \
                                            MultiDictObject* other,         \
                                            Py_ssize_t* ppos1,              \
                                            Py_ssize_t* ppos2)              \
    {                                                                       \
        _MD_EQ_##K1##_T* first1 = _MD_EQ_##K1##_ENTRIES(md->keys);          \
        _MD_EQ_##K2##_T* first2 = _MD_EQ_##K2##_ENTRIES(other->keys);       \
        _MD_EQ_##K1##_T* end1 = first1 + md->keys->nentries;                \
        _MD_EQ_##K2##_T* end2 = first2 + other->keys->nentries;             \
        _MD_EQ_##K1##_T* entry1 = first1 + *ppos1;                          \
        _MD_EQ_##K2##_T* entry2 = first2 + *ppos2;                          \
        for (;;) {                                                          \
            if (entry1 >= end1 || entry2 >= end2) {                         \
                return 1;                                                   \
            }                                                               \
            if (_MD_EQ_##K1##_IS_HOLE(entry1)) {                            \
                entry1++;                                                   \
                continue;                                                   \
            }                                                               \
            if (_MD_EQ_##K2##_IS_HOLE(entry2)) {                            \
                entry2++;                                                   \
                continue;                                                   \
            }                                                               \
            if (_MD_EQ_##K1##_HASH(entry1) != _MD_EQ_##K2##_HASH(entry2) || \
                !str_cmp(_MD_EQ_##K1##_IDENTITY(entry1),                    \
                         _MD_EQ_##K2##_IDENTITY(entry2))) {                 \
                return 0;                                                   \
            }                                                               \
            int cmp = _md_eq_values(_MD_EQ_##K1##_BASE(entry1)->value,      \
                                    _MD_EQ_##K2##_BASE(entry2)->value);     \
            if (cmp == _MD_EQ_RAN) {                                        \
                *ppos1 = entry1 - first1 + 1;                               \
                *ppos2 = entry2 - first2 + 1;                               \
                return cmp;                                                 \
            }                                                               \
            if (cmp <= 0) {                                                 \
                return cmp;                                                 \
            }                                                               \
            entry1++;                                                       \
            entry2++;                                                       \
        }                                                                   \
    }

_MD_DEFINE_EQ_SCAN(CS, CS)
_MD_DEFINE_EQ_SCAN(CS, CI)
_MD_DEFINE_EQ_SCAN(CS, ANYSTR)
_MD_DEFINE_EQ_SCAN(CI, CS)
_MD_DEFINE_EQ_SCAN(CI, CI)
_MD_DEFINE_EQ_SCAN(CI, ANYSTR)
_MD_DEFINE_EQ_SCAN(ANYSTR, CS)
_MD_DEFINE_EQ_SCAN(ANYSTR, CI)
_MD_DEFINE_EQ_SCAN(ANYSTR, ANYSTR)

#undef _MD_DEFINE_EQ_SCAN
#undef _MD_EQ_CS_T
#undef _MD_EQ_CS_ENTRIES
#undef _MD_EQ_CS_IS_HOLE
#undef _MD_EQ_CS_HASH
#undef _MD_EQ_CS_IDENTITY
#undef _MD_EQ_CS_BASE
#undef _MD_EQ_CI_T
#undef _MD_EQ_CI_ENTRIES
#undef _MD_EQ_CI_IS_HOLE
#undef _MD_EQ_CI_HASH
#undef _MD_EQ_CI_IDENTITY
#undef _MD_EQ_CI_BASE
#undef _MD_EQ_ANYSTR_T
#undef _MD_EQ_ANYSTR_ENTRIES
#undef _MD_EQ_ANYSTR_IS_HOLE
#undef _MD_EQ_ANYSTR_HASH
#undef _MD_EQ_ANYSTR_IDENTITY
#undef _MD_EQ_ANYSTR_BASE

// The _MD_EQ_ kind of md's table: 0 is CS, 1 is CI, 2 is ANYSTR.
static inline int
_md_eq_kind(MultiDictObject* md)
{
    return kind_is_compact(md->keys->kind) ? md->is_ci : 2;
}

/* Picks the scan for the kinds md's and other's tables have now. */
static int
_md_eq_scan(MultiDictObject* md, MultiDictObject* other, Py_ssize_t* ppos1,
            Py_ssize_t* ppos2)
{
    switch (_md_eq_kind(md) * 3 + _md_eq_kind(other)) {
        case 0:
            return _md_eq_scan_CS_CS(md, other, ppos1, ppos2);
        case 1:
            return _md_eq_scan_CS_CI(md, other, ppos1, ppos2);
        case 2:
            return _md_eq_scan_CS_ANYSTR(md, other, ppos1, ppos2);
        case 3:
            return _md_eq_scan_CI_CS(md, other, ppos1, ppos2);
        case 4:
            return _md_eq_scan_CI_CI(md, other, ppos1, ppos2);
        case 5:
            return _md_eq_scan_CI_ANYSTR(md, other, ppos1, ppos2);
        case 6:
            return _md_eq_scan_ANYSTR_CS(md, other, ppos1, ppos2);
        case 7:
            return _md_eq_scan_ANYSTR_CI(md, other, ppos1, ppos2);
        default:
            return _md_eq_scan_ANYSTR_ANYSTR(md, other, ppos1, ppos2);
    }
}

static int
md_eq(MultiDictObject* md, MultiDictObject* other)
{
    if (md == other) {
        return 1;
    }

    if (md_len(md) != md_len(other)) {
        return 0;
    }

    Py_ssize_t pos1 = 0;
    Py_ssize_t pos2 = 0;
    int ret;
    do {
        ret = _md_eq_scan(md, other, &pos1, &pos2);
    } while (ret == _MD_EQ_RAN);
    return ret;
}

#undef _MD_EQ_RAN

typedef struct _md_eq_to_mapping_state {
    PyObject* other;
    bool eq;
} md_eq_to_mapping_state_t;

static int
_md_eq_to_mapping_visit(void* user_data, PyObject* identity, Py_hash_t hash,
                        PyObject* key, PyObject* value)
{
    (void)identity;
    (void)hash;
    md_eq_to_mapping_state_t* state = (md_eq_to_mapping_state_t*)user_data;
    PyObject* other_value;
    int ret = PyMapping_GetOptionalItem(state->other, key, &other_value);
    if (ret <= 0) {
        state->eq = false;
        return ret;
    }
    ret = PyObject_RichCompareBool(value, other_value, Py_EQ);
    Py_DECREF(other_value);
    if (ret == 0) {
        state->eq = false;
    }
    return ret;
}

static int
_md_eq_to_mapping_locked(MultiDictObject* md, PyObject* other)
{
    Py_ssize_t other_len;

    if (UNLIKELY(!PyMapping_Check(other))) {
        PyErr_Format(PyExc_TypeError,
                     "other argument must be a mapping, not %s",
                     Py_TYPE(other)->tp_name);
        return -1;
    }

    other_len = PyMapping_Size(other);
    if (UNLIKELY(other_len < 0)) {
        return -1;
    }
    if (md_len(md) != other_len) {
        return 0;
    }

    md_eq_to_mapping_state_t state = {other, true};
    if (UNLIKELY(md_walk_all(md, true, _md_eq_to_mapping_visit, &state) < 0)) {
        return -1;
    }
    return state.eq;
}

COLD static int
md_eq_to_mapping(MultiDictObject* md, PyObject* other)
{
    int ret;
    Py_BEGIN_CRITICAL_SECTION(md);
    ret = _md_eq_to_mapping_locked(md, other);
    Py_END_CRITICAL_SECTION();
    return ret;
}

#ifdef __cplusplus
}
#endif
#endif
