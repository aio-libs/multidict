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
_str_cmp(PyObject* s1, PyObject* s2)
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
    if (PyUnicode_CheckExact(key)) {
        return Py_NewRef(key);
    }
    if (PyUnicode_Check(key)) {
        return PyUnicode_FromObject(key);
    }
    PyErr_SetString(PyExc_TypeError,
                    "MultiDict keys should be either str "
                    "or subclasses of str");
    return NULL;
}

/* Index of the first ASCII uppercase byte in s[0:len], or len if there is
   none. */
ALWAYS_INLINE static inline Py_ssize_t
_ascii_find_upper(const Py_UCS1* s, Py_ssize_t len)
{
    for (Py_ssize_t i = 0; i < len; i++) {
        if (s[i] >= 'A' && s[i] <= 'Z') {
            return i;
        }
    }
    return len;
}

/* Lowercase an all-ASCII string into a fresh exact str, given the index of
   its first uppercase byte.  str.lower() routes ASCII through
   ascii_upper_or_lower(), which is PyUnicode_New(len, 127) filled by
   Py_TOLOWER() per byte, so this is byte-identical to it. */
static inline PyObject*
_ascii_lower(const Py_UCS1* data, Py_ssize_t len, Py_ssize_t pos)
{
    /* PyUnicode_New(0, 127) hands back the shared empty string; writing into
       it would corrupt a singleton.  Unreachable: a zero-length key has no
       uppercase byte, so the caller reuses it instead of calling here. */
    assert(pos < len);
    PyObject* ret = PyUnicode_New(len, 127);
    if (ret == NULL) {
        return NULL;
    }
    Py_UCS1* dst = (Py_UCS1*)PyUnicode_DATA(ret);
    memcpy(dst, data, (size_t)pos);
    for (Py_ssize_t i = pos; i < len; i++) {
        dst[i] = (Py_UCS1)Py_TOLOWER(data[i]);
    }
    return ret;
}

static inline PyObject*
_ci_key_to_identity(mod_state* state, PyObject* key)
{
    if (IStr_Check(state, key)) {
        return Py_NewRef(((istrobject*)key)->canonical);
    }
    /* Exact str only: a str subclass may override lower(), and callers rely
       on the override running. */
    if (PyUnicode_CheckExact(key) && PyUnicode_IS_ASCII(key)) {
        Py_ssize_t len = PyUnicode_GET_LENGTH(key);
        const Py_UCS1* data = (const Py_UCS1*)PyUnicode_DATA(key);
        Py_ssize_t pos = _ascii_find_upper(data, len);
        if (pos == len) {
            /* The key already is its own identity, so reuse it: no copy, and
               _unicode_hash() gets the key's cached hash instead of hashing
               a fresh string. */
            return Py_NewRef(key);
        }
        return _ascii_lower(data, len, pos);
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
    if (IStr_Check(state, key)) {
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

static inline PyObject*
_md_calc_key(MultiDictObject* md, PyObject* key, PyObject* identity)
{
    if (md->is_ci) return _ci_arg_to_key(md->state, key, identity);
    return _arg_to_key(md->state, key, identity);
}

static inline PyObject*
_md_ensure_key(MultiDictObject* md, entry_t* entry)
{
    assert(entry >= htkeys_entries(md->keys));
    assert(entry < htkeys_entries(md->keys) + md->keys->nentries);
    if (!md->is_ci || IStr_Check(md->state, entry->key)) {
        return _md_calc_key(md, entry->key, entry->identity);
    }
    /* Building the istr can run Python code (a str subclass's __str__, a GC
       finalizer) that mutates md and frees entry, so hold our own refs. */
    uint64_t version = md->version;
    PyObject* old_key = Py_NewRef(entry->key);
    PyObject* identity = Py_NewRef(entry->identity);
    PyObject* key = _md_calc_key(md, old_key, identity);
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
