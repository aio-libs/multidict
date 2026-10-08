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

COLD static PyObject*
_err_key_type_cs(void)
{
    PyErr_SetString(PyExc_TypeError,
                    "MultiDict keys should be either str "
                    "or subclasses of str");
    return NULL;
}

COLD static PyObject*
_err_key_type_ci(void)
{
    PyErr_SetString(PyExc_TypeError,
                    "CIMultiDict keys should be either str "
                    "or subclasses of str");
    return NULL;
}

static inline PyObject*
_key_to_identity_cs(mod_state* state, PyObject* key)
{
    /* Inverted so the exact-str case is the fallthrough: left as written,
       GCC puts it out of line behind a taken branch and puts the subclass
       call on the straight line. */
    if (UNLIKELY(!PyUnicode_CheckExact(key))) {
        if (PyUnicode_Check(key)) {
            return PyUnicode_FromObject(key);
        }
        return _err_key_type_cs();
    }
    return Py_NewRef(key);
}

/* True if the eight bytes at p hold an ASCII uppercase one. */
static bool
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
static inline bool
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

static PyObject*
_str_to_identity_ci(mod_state* state, PyObject* key);

/* Anything but an ASCII exact str: rare enough in keys to keep off the
   straight line. */
COLD static PyObject*
_str_call_lower_ci(mod_state* state, PyObject* key)
{
    if (PyUnicode_CheckExact(key)) {
        return PyObject_CallMethodNoArgs(key, state->str_lower);
    }
    if (!PyUnicode_Check(key)) {
        return _err_key_type_ci();
    }
    /* str.__str__(key).lower(), never a subclass's own lower(): no Python
       code runs, and the exact copy takes the ASCII path. */
    PyObject* str = PyUnicode_FromObject(key);
    if (str == NULL) {
        return NULL;
    }
    PyObject* ret = _str_to_identity_ci(state, str);
    Py_DECREF(str);
    return ret;
}

static PyObject*
_str_to_identity_ci(mod_state* state, PyObject* key)
{
    /* Exact str only: a subclass's identity must be a fresh exact str. */
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
    return _str_call_lower_ci(state, key);
}

/* An exact ASCII str with no uppercase is its own identity in a
   CIMultiDict, the key itself; NULL for any other key. */
static PyObject*
_str_borrow_identity_ci(PyObject* key)
{
    if (PyUnicode_CheckExact(key) && PyUnicode_IS_ASCII(key) &&
        !_ascii_has_upper((const Py_UCS1*)PyUnicode_DATA(key),
                          PyUnicode_GET_LENGTH(key))) {
        return key;
    }
    return NULL;
}

static inline PyObject*
_key_to_identity_ci(mod_state* state, PyObject* key)
{
    if (IStr_CheckExact(state, key)) {
        return Py_NewRef(istr_canonical(key));
    }
    return _str_to_identity_ci(state, key);
}

static inline PyObject*
_arg_to_key_cs(mod_state* state, PyObject* key, PyObject* identity)
{
    if (UNLIKELY(!PyUnicode_Check(key))) {
        return _err_key_type_cs();
    }
    return Py_NewRef(key);
}

/* A str subclass is copied to an exact str first: istr(), like str(), would
   call its __str__, which may spell a different string than the key. */
static PyObject*
_subclass_to_key_ci(mod_state* state, PyObject* key, PyObject* identity)
{
    PyObject* str = PyUnicode_FromObject(key);
    if (str == NULL) {
        return NULL;
    }
    PyObject* ret = IStr_New(state, str, identity);
    Py_DECREF(str);
    return ret;
}

static inline PyObject*
_arg_to_key_ci(mod_state* state, PyObject* key, PyObject* identity)
{
    if (IStr_CheckExact(state, key)) {
        return Py_NewRef(key);
    }
    if (PyUnicode_CheckExact(key)) {
        return IStr_New(state, key, identity);
    }
    if (UNLIKELY(!PyUnicode_Check(key))) {
        return _err_key_type_ci();
    }
    return _subclass_to_key_ci(state, key, identity);
}

static inline PyObject*
md_calc_identity(MultiDictObject* md, PyObject* key)
{
    if (md->is_ci) return _key_to_identity_ci(md->state, key);
    return _key_to_identity_cs(md->state, key);
}

/* The identity of key, borrowed, for a lookup that drops it before it
   returns; NULL, with no exception set, when it must be computed instead.
   An exact str that is its own identity and an exact istr's canonical,
   which never changes, are kept alive by the caller's reference to the
   key.  A borrowed identity keeps the owned one's decref off the lookup's
   exit, which cost key in d 4% in taken branches. */
static inline PyObject*
md_borrow_identity(MultiDictObject* md, PyObject* key)
{
    if (md->is_ci) {
        if (IStr_CheckExact(md->state, key)) {
            return istr_canonical(key);
        }
        return _str_borrow_identity_ci(key);
    }
    if (UNLIKELY(!PyUnicode_CheckExact(key))) {
        return NULL;
    }
    return key;
}

/* md_calc_identity() that also says whether key fits a compact table (see
   md_key_fits()): a CIMultiDict's istr test has just run here, so the
   insert need not repeat it. */
static inline PyObject*
md_calc_identity_fits(MultiDictObject* md, PyObject* key, bool* pfits)
{
    if (md->is_ci) {
        if (IStr_CheckExact(md->state, key)) {
            *pfits = true;
            return Py_NewRef(istr_canonical(key));
        }
        *pfits = false;
        return _str_to_identity_ci(md->state, key);
    }
    PyObject* identity = _key_to_identity_cs(md->state, key);
    *pfits = identity == key;
    return identity;
}

/* Reads only `key`, md->is_ci and md->state, all fixed for md's lifetime,
   so the caller need not hold md's critical section. */
static inline int
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

/* md_calc_identity_hash() plus md_calc_identity_fits()'s *pfits. */
static inline int
md_calc_identity_hash_fits(MultiDictObject* md, PyObject* key,
                           PyObject** pidentity, Py_hash_t* phash, bool* pfits)
{
    PyObject* identity = md_calc_identity_fits(md, key, pfits);
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
    if (md->is_ci) return _arg_to_key_ci(md->state, key, identity);
    return _arg_to_key_cs(md->state, key, identity);
}

/* Building the istr allocates, which can run a collection whose finalizers
   mutate md and free entry, so hold our own refs. Only an exact str is
   replaced by its istr: releasing one runs no code, where a subclass's
   __del__ could. */
COLD static PyObject*
_md_cache_key_ci(MultiDictObject* md, entry_t* entry)
{
    assert(md->is_ci);
    uint64_t version = md->version;
    PyObject* old_key = Py_NewRef(entry->key);
    PyObject* identity = Py_NewRef(kind_is_compact(md->keys->kind)
                                       ? compact_entry_identity_ci(entry)
                                       : as_anystr(entry)->identity);
    PyObject* key = _arg_to_key_ci(md->state, old_key, identity);
    if (key != NULL && md->version == version &&
        PyUnicode_CheckExact(old_key)) {
        entry->key = Py_NewRef(key);
        Py_DECREF(old_key);
    }
    /* These can run __del__ or suspend the critical section, so the caller
       must not touch entry after this returns. */
    Py_DECREF(identity);
    Py_DECREF(old_key);
    return key;
}

/* A stored key was checked to be a str when it went in, so only a
   CIMultiDict's plain str needs work. */
static inline PyObject*
md_ensure_key(MultiDictObject* md, entry_t* entry)
{
    assert(entry->key != NULL);
    PyObject* key = entry->key;
    if (!md->is_ci || IStr_CheckExact(md->state, key)) {
        // not Py_NewRef(): GCC leaves it out of line on FT builds
        Py_INCREF(key);
        return key;
    }
    return _md_cache_key_ci(md, entry);
}

#ifdef __cplusplus
}
#endif

#endif
