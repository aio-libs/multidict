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
    if (UNLIKELY(ret == NULL)) {
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
    if (UNLIKELY(str == NULL)) {
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

/* A str subclass is copied to an exact str first: istr(), like str(), would
   call its __str__, which may spell a different string than the key. */
static PyObject*
_subclass_to_key_ci(mod_state* state, PyObject* key, PyObject* identity)
{
    PyObject* str = PyUnicode_FromObject(key);
    if (UNLIKELY(str == NULL)) {
        return NULL;
    }
    PyObject* ret = istr_create(state, str, identity);
    Py_DECREF(str);
    return ret;
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

/* A CIMultiDict key that is no exact istr: the key itself when it is its
   own identity, else a new reference to a lowercased copy, never to the
   key. */
static PyObject*
_str_identity_or_new_ci(mod_state* state, PyObject* key)
{
    if (PyUnicode_CheckExact(key) && PyUnicode_IS_ASCII(key)) {
        Py_ssize_t len = PyUnicode_GET_LENGTH(key);
        const Py_UCS1* data = (const Py_UCS1*)PyUnicode_DATA(key);
        if (!_ascii_has_upper(data, len)) {
            return key;
        }
        return _ascii_lower(data, len);
    }
    PyObject* identity = _str_call_lower_ci(state, key);
    if (identity == key) {
        Py_DECREF(identity);
    }
    return identity;
}

/* md_borrow_identity() for a call that computes the identity when it
   cannot borrow it, with each test of the key run once: the identity is
   borrowed or new, and md_identity_release() drops it. NULL with an
   exception set on error. */
static inline PyObject*
md_identity_or_new(MultiDictObject* md, PyObject* key)
{
    if (md->is_ci) {
        if (IStr_CheckExact(md->state, key)) {
            return istr_canonical(key);
        }
        return _str_identity_or_new_ci(md->state, key);
    }
    if (UNLIKELY(!PyUnicode_CheckExact(key))) {
        return _key_to_identity_cs(md->state, key);
    }
    return key;
}

/* Drops the identity md_identity_or_new() handed out if it is a new
   reference: only the key itself and an exact istr's canonical are
   borrowed. */
static inline void
md_identity_release(MultiDictObject* md, PyObject* key, PyObject* identity)
{
    if (identity != key && !(md->is_ci && IStr_CheckExact(md->state, key))) {
        Py_DECREF(identity);
    }
}

/* Returned by value, like parse2_t: out-parameters would make the caller's
   locals address-taken whenever GCC keeps the helper out of line, and
   -fstack-protector-strong then gives the caller a canary.  identity is
   NULL on error. */
typedef struct {
    PyObject* identity;
    Py_hash_t hash;
} identity_hash_t;

/* Reads only `key`, md->is_ci and md->state, all fixed for md's lifetime,
   so the caller need not hold md's critical section. */
static inline identity_hash_t
md_calc_identity_hash(MultiDictObject* md, PyObject* key)
{
    identity_hash_t ret = {md_calc_identity(md, key), -1};
    if (UNLIKELY(ret.identity == NULL)) {
        return ret;
    }
    ret.hash = unicode_hash(ret.identity);
    if (UNLIKELY(ret.hash == -1)) {
        Py_CLEAR(ret.identity);
    }
    return ret;
}

#ifndef Py_GIL_DISABLED
/* The set for a string hash: Fibonacci hashing, since a str hash may have
   its entropy anywhere. */
static inline istr_cache_entry_t*
_istr_cache_set(mod_state* state, Py_hash_t hash)
{
    size_t set = (size_t)(((uint64_t)hash * UINT64_C(0x9E3779B97F4A7C15)) >>
                          (64 - ISTR_CACHE_LOG2_SETS));
    return &state->istr_cache[set * ISTR_CACHE_WAYS];
}
#endif

/* The istr cached for a str equal to the exact str `key`, borrowed, or
   NULL. Hashing an exact str cannot fail. */
static inline PyObject*
_istr_cache_get(mod_state* state, PyObject* key)
{
#ifdef Py_GIL_DISABLED
    return NULL;
#else
    Py_hash_t hash = unicode_hash(key);
    istr_cache_entry_t* set = _istr_cache_set(state, hash);
    for (int way = 0; way < ISTR_CACHE_WAYS; way++) {
        if (set[way].str == key) {
            return set[way].istr;
        }
    }
    for (int way = 0; way < ISTR_CACHE_WAYS; way++) {
        if (set[way].str != NULL && set[way].hash == hash &&
            str_cmp(set[way].str, key)) {
            return set[way].istr;
        }
    }
    return NULL;
#endif
}

/* Builds and caches the istr for the exact str `key`, in the set's first
   way; the pair there moves to the second, whose pair is released. That
   runs no Python code: an exact str and an istr. */
static PyObject*
_istr_cache_fill(mod_state* state, PyObject* key, PyObject* identity)
{
    PyObject* ret = istr_create(state, key, identity);
#ifndef Py_GIL_DISABLED
    if (ret != NULL) {
        Py_hash_t hash = unicode_hash(key);
        istr_cache_entry_t* set = _istr_cache_set(state, hash);
        istr_cache_entry_t evicted = set[1];
        set[1] = set[0];
        set[0].str = Py_NewRef(key);
        set[0].istr = Py_NewRef(ret);
        set[0].hash = hash;
        Py_XDECREF(evicted.str);
        Py_XDECREF(evicted.istr);
    }
#endif
    return ret;
}

/* Releases every cached pair. Idempotent, as module_clear() may run more
   than once. */
static void
istr_cache_clear(mod_state* state)
{
#ifndef Py_GIL_DISABLED
    for (int i = 0; i < (1 << ISTR_CACHE_LOG2_SETS) * ISTR_CACHE_WAYS; i++) {
        Py_CLEAR(state->istr_cache[i].str);
        Py_CLEAR(state->istr_cache[i].istr);
    }
#else
    (void)state;
#endif
}

/* The istr a CIMultiDict stores for `key`, no exact istr, whose identity
   is `identity`: a new reference. Its canonical form may be another
   object equal to `identity`, when the istr comes from the cache. */
static PyObject*
_str_to_key_ci(mod_state* state, PyObject* key, PyObject* identity)
{
    if (PyUnicode_CheckExact(key)) {
        return _istr_cache_fill(state, key, identity);
    }
    return _subclass_to_key_ci(state, key, identity);
}

static inline PyObject*
str_to_key_ci(mod_state* state, PyObject* key, PyObject* identity)
{
    PyObject* cached =
        PyUnicode_CheckExact(key) ? _istr_cache_get(state, key) : NULL;
    if (cached != NULL) {
        return Py_NewRef(cached);
    }
    return _str_to_key_ci(state, key, identity);
}

/* A key about to be stored: the key md stores and its identity. Returned
   by value, like identity_hash_t. key is NULL on error. */
typedef struct {
    PyObject* key;
    PyObject* identity;
} stored_key_t;

/* md_identity_or_new() for a key about to be stored, which also gives the
   key md stores: `key` itself, borrowed, unless a CIMultiDict stores an
   istr in its place, a new reference whose canonical form is the
   identity, borrowed from it. A cached istr also saves computing the
   identity. md_store_key_release() drops whatever is new. */
static inline stored_key_t
md_store_key(MultiDictObject* md, PyObject* key)
{
    stored_key_t ret = {key, NULL};
    if (!md->is_ci) {
        ret.identity = md_identity_or_new(md, key);
        if (UNLIKELY(ret.identity == NULL)) {
            ret.key = NULL;
        }
        return ret;
    }
    if (IStr_CheckExact(md->state, key)) {
        ret.identity = istr_canonical(key);
        return ret;
    }
    PyObject* cached =
        PyUnicode_CheckExact(key) ? _istr_cache_get(md->state, key) : NULL;
    if (cached != NULL) {
        ret.key = Py_NewRef(cached);
    } else {
        PyObject* identity = _str_identity_or_new_ci(md->state, key);
        if (UNLIKELY(identity == NULL)) {
            ret.key = NULL;
            return ret;
        }
        ret.key = _str_to_key_ci(md->state, key, identity);
        if (identity != key) {
            Py_DECREF(identity);  // the istr holds it now
        }
        if (UNLIKELY(ret.key == NULL)) {
            return ret;
        }
    }
    ret.identity = istr_canonical(ret.key);
    return ret;
}

static inline void
md_store_key_release(MultiDictObject* md, PyObject* key, stored_key_t stored)
{
    if (stored.key != key) {
        Py_DECREF(stored.key);
    } else if (!md->is_ci && stored.identity != key) {
        Py_DECREF(stored.identity);
    }
}

#ifdef __cplusplus
}
#endif

#endif
