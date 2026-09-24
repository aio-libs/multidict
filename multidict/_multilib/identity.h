#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_IDENTITY_H
#define _MULTIDICT_IDENTITY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "compiler.h"
#include "dict.h"
#include "htkeys.h"
#include "istr.h"
#include "state.h"

ALWAYS_INLINE static inline bool
str_cmp(PyObject* s1, PyObject* s2)
{
    /* implementation is borrowed from PyUnicode_Equal() but without
       type checks, arguments are identities that are always strings */
    assert(PyUnicode_Check(s1));
    assert(PyUnicode_Check(s2));

    if (s1 == s2) {
        return true;
    }
    Py_ssize_t len = PyUnicode_GET_LENGTH(s1);
    if (PyUnicode_GET_LENGTH(s2) != len) {
        return false;
    }

    int kind = PyUnicode_KIND(s1);
    if (PyUnicode_KIND(s2) != kind) {
        return false;
    }

    const void* data1 = PyUnicode_DATA(s1);
    const void* data2 = PyUnicode_DATA(s2);
    return (memcmp(data1, data2, len * kind) == 0);
}

static inline PyObject*
_key_to_identity(mod_state* state, PyObject* key)
{
    /* Inverted so the exact-str case is the fallthrough: left as written,
       GCC puts it out of line behind a taken branch and puts the subclass
       call on the straight line. */
    if (UNLIKELY(!PyUnicode_CheckExact(key))) {
        if (PyUnicode_Check(key)) {
            return PyUnicode_FromObject(key);
        }
        PyErr_SetString(PyExc_TypeError,
                        "MultiDict keys should be either str "
                        "or subclasses of str");
        return NULL;
    }
    return Py_NewRef(key);
}

/* True if the eight bytes at p hold an ASCII uppercase one. */
ALWAYS_INLINE static inline bool
_word_has_upper(const Py_UCS1* p)
{
    const uint64_t ones = UINT64_C(0x0101010101010101);
    const uint64_t highs = UINT64_C(0x8080808080808080);
    uint64_t w;
    /* memcpy, not a cast: the data of a non-compact str need not be word
       aligned, and the test below asks only whether some lane is uppercase,
       never which, so the byte order does not matter. */
    memcpy(&w, p, 8);
    /* Every byte is below 0x80, so neither sum carries out of its lane: the
       high bit of a lane marks b >= 'A' and b > 'Z' respectively. */
    uint64_t ge_a = w + (0x80 - 'A') * ones;
    uint64_t gt_z = w + (0x80 - 'Z' - 1) * ones;
    return (ge_a & ~gt_z & highs) != 0;
}

/* True if s[0:len] holds an ASCII uppercase byte. */
ALWAYS_INLINE static inline bool
_ascii_has_upper(const Py_UCS1* s, Py_ssize_t len)
{
    /* Too short for a word.  A compact ASCII str is allocated as its header
       plus len + 1 bytes, so reading a whole word here would run off the end
       of the block, which is what AddressSanitizer reports.  This is also
       what lets the last word below start at len - 8. */
    if (len < 8) {
        for (Py_ssize_t i = 0; i < len; i++) {
            if (s[i] >= 'A' && s[i] <= 'Z') {
                return true;
            }
        }
        return false;
    }
    Py_ssize_t i = 0;
    for (; i + 8 <= len; i += 8) {
        if (_word_has_upper(s + i)) {
            return true;
        }
    }
    /* A last word ending at the end of the key rather than a byte loop: it
       overlaps the previous one when len is not a multiple of 8, and
       re-reading a few bytes costs nothing for a yes/no answer.  Scanning
       the leftover byte by byte instead is what made the eight-byte word
       lose to a four-byte one on short keys. */
    return i < len ? _word_has_upper(s + len - 8) : false;
}

/* Lowercase an all-ASCII string into a fresh exact str.  str.lower() routes
   ASCII through ascii_upper_or_lower(), which is PyUnicode_New(len, 127)
   filled by Py_TOLOWER() per byte, so this is byte-identical to it. */
static inline PyObject*
_ascii_lower(const Py_UCS1* data, Py_ssize_t len)
{
    /* PyUnicode_New(0, 127) hands back the shared empty string; writing into
       it would corrupt a singleton.  Unreachable: a zero-length key has no
       uppercase byte, so the caller reuses it instead of calling here. */
    assert(len > 0);
    PyObject* ret = PyUnicode_New(len, 127);
    if (ret == NULL) {
        return NULL;
    }
    Py_UCS1* dst = (Py_UCS1*)PyUnicode_DATA(ret);
    for (Py_ssize_t i = 0; i < len; i++) {
        dst[i] = (Py_UCS1)Py_TOLOWER(data[i]);
    }
    return ret;
}

/* Out of line on purpose.  md_calc_identity() carries this whole function
   into every md_*() that takes a key, and inlining it there costs more than
   it saves: the extra size pushes md_contains() and md_next() past the
   inliner's budget at their own call sites, which slowed keys().isdisjoint()
   by 31% even on a case-sensitive MultiDict, whose keys never reach here. */
NOINLINE static PyObject*
_ci_str_to_identity(mod_state* state, PyObject* key)
{
    /* Exact str only: a str subclass may override lower(), and callers rely
       on the override running. */
    if (PyUnicode_CheckExact(key) && PyUnicode_IS_ASCII(key)) {
        Py_ssize_t len = PyUnicode_GET_LENGTH(key);
        const Py_UCS1* data = (const Py_UCS1*)PyUnicode_DATA(key);
        if (!_ascii_has_upper(data, len)) {
            /* The key already is its own identity, so reuse it: no copy, and
               unicode_hash() gets the key's cached hash instead of hashing
               a fresh string. */
            return Py_NewRef(key);
        }
        return _ascii_lower(data, len);
    }
    if (PyUnicode_Check(key)) {
        PyObject* ret = PyObject_CallMethodNoArgs(key, state->str_lower);
        if (ret == NULL) {
            goto fail;
        }
        if (!PyUnicode_CheckExact(ret)) {
            PyObject* tmp = PyUnicode_FromObject(ret);
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
fail:
    return NULL;
}

static inline PyObject*
_ci_key_to_identity(mod_state* state, PyObject* key)
{
    if (IStr_CheckExact(state, key)) {
        return Py_NewRef(((istrobject*)key)->canonical);
    }
    return _ci_str_to_identity(state, key);
}

static inline PyObject*
_arg_to_key(mod_state* state, PyObject* key, PyObject* identity)
{
    if (PyUnicode_Check(key)) {
        return Py_NewRef(key);
    }
    PyErr_SetString(PyExc_TypeError,
                    "MultiDict keys should be either str "
                    "or subclasses of str");
    return NULL;
}

static inline PyObject*
_ci_arg_to_key(mod_state* state, PyObject* key, PyObject* identity)
{
    if (IStr_CheckExact(state, key)) {
        return Py_NewRef(key);
    }
    if (PyUnicode_Check(key)) {
        return IStr_New(state, key, identity);
    }
    PyErr_SetString(PyExc_TypeError,
                    "CIMultiDict keys should be either str "
                    "or subclasses of str");
    return NULL;
}

static inline PyObject*
md_calc_identity(MultiDictObject* md, PyObject* key)
{
    if (md->is_ci) return _ci_key_to_identity(md->state, key);
    return _key_to_identity(md->state, key);
}

/* Reads only `key`, md->is_ci and md->state, all fixed for md's lifetime,
   so the caller need not hold md's critical section. Always inlined: left
   to itself GCC emits it out of line, and every caller pays the call. */
ALWAYS_INLINE static inline int
md_calc_identity_hash(MultiDictObject* md, PyObject* key, PyObject** pidentity,
                      Py_hash_t* phash)
{
    PyObject* identity = md_calc_identity(md, key);
    if (identity == NULL) {
        return -1;
    }
    Py_hash_t hash = unicode_hash(identity);
    if (hash == -1) {
        Py_DECREF(identity);
        return -1;
    }
    *pidentity = identity;
    *phash = hash;
    return 0;
}

static inline PyObject*
md_calc_key(MultiDictObject* md, PyObject* key, PyObject* identity)
{
    if (md->is_ci) return _ci_arg_to_key(md->state, key, identity);
    return _arg_to_key(md->state, key, identity);
}

static inline PyObject*
md_ensure_key(MultiDictObject* md, entry_t* entry)
{
    assert(entry >= htkeys_entries(md->keys));
    assert(entry < htkeys_entries(md->keys) + md->keys->nentries);
    if (!md->is_ci || IStr_CheckExact(md->state, entry->key)) {
        return md_calc_key(md, entry->key, entry->identity);
    }
    /* Building the istr can run Python code (a str subclass's __str__, a GC
       finalizer) that mutates md and frees entry, so hold our own refs. */
    uint64_t version = md->version;
    PyObject* old_key = Py_NewRef(entry->key);
    PyObject* identity = Py_NewRef(entry->identity);
    PyObject* key = md_calc_key(md, old_key, identity);
    if (key != NULL && md->version == version) {
        entry->key = Py_NewRef(key);
        Py_DECREF(old_key);
    }
    /* These can run __del__ or suspend the critical section, so the caller
       must not touch entry after this returns. */
    Py_DECREF(identity);
    Py_DECREF(old_key);
    return key;
}

#ifdef __cplusplus
}
#endif

#endif
