#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_REPR_H
#define _MULTIDICT_REPR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <stdbool.h>

#include "compiler.h"
#include "dict.h"
#include "hashtable.h"
#include "state.h"
#include "walk.h"

typedef struct _md_repr_state {
    PyUnicodeWriter* writer;
    bool show_keys;
    bool show_values;
    bool comma;
    // a keys view's repr of a table that may repeat a key: the ones shown
    PyObject* shown;
} md_repr_state_t;

static int
_md_repr_visit(void* user_data, PyObject* identity, Py_hash_t hash,
               PyObject* key, PyObject* value)
{
    md_repr_state_t* state = (md_repr_state_t*)user_data;
    PyUnicodeWriter* writer = state->writer;
    if (state->shown != NULL) {
        int seen = PySet_Contains(state->shown, identity);
        if (seen != 0) {
            return seen < 0 ? -1 : 1;
        }
        if (UNLIKELY(PySet_Add(state->shown, identity) < 0)) {
            return -1;
        }
    }
    if (state->comma) {
        if (UNLIKELY(PyUnicodeWriter_WriteChar(writer, ',') < 0)) {
            return -1;
        }
        if (UNLIKELY(PyUnicodeWriter_WriteChar(writer, ' ') < 0)) {
            return -1;
        }
    }
    state->comma = true;
    if (state->show_keys) {
        /* Fast path: ASCII keys without characters that would be escaped
         * by repr() can be wrapped in single quotes directly. Falls back
         * to PyUnicodeWriter_WriteRepr for keys containing quotes,
         * backslashes, or non-printable characters so the output stays
         * a valid Python string literal. */
        int fast = 0;
        if (PyUnicode_IS_ASCII(key)) {
            Py_ssize_t klen = PyUnicode_GET_LENGTH(key);
            const unsigned char* kdata =
                (const unsigned char*)PyUnicode_DATA(key);
            fast = 1;
            for (Py_ssize_t ki = 0; ki < klen; ++ki) {
                unsigned char c = kdata[ki];
                if (c < 0x20 || c == 0x7f || c == '\'' || c == '\\') {
                    fast = 0;
                    break;
                }
            }
        }
        if (fast) {
            if (UNLIKELY(PyUnicodeWriter_WriteChar(writer, '\'') < 0)) {
                return -1;
            }
            /* WriteStr() copies an istr through str(). */
            if (UNLIKELY(
                    (PyUnicode_CheckExact(key)
                         ? PyUnicodeWriter_WriteStr(writer, key)
                         : PyUnicodeWriter_WriteSubstring(
                               writer, key, 0, PyUnicode_GET_LENGTH(key))) <
                    0)) {
                return -1;
            }
            if (UNLIKELY(PyUnicodeWriter_WriteChar(writer, '\'') < 0)) {
                return -1;
            }
        } else {
            if (UNLIKELY(PyUnicodeWriter_WriteRepr(writer, key) < 0)) {
                return -1;
            }
        }
    }
    if (state->show_keys && state->show_values) {
        if (UNLIKELY(PyUnicodeWriter_WriteChar(writer, ':') < 0)) {
            return -1;
        }
        if (UNLIKELY(PyUnicodeWriter_WriteChar(writer, ' ') < 0)) {
            return -1;
        }
    }
    if (state->show_values) {
        if (UNLIKELY(PyUnicodeWriter_WriteRepr(writer, value) < 0)) {
            return -1;
        }
    }
    return 1;
}

COLD static PyObject*
_md_repr_locked(MultiDictObject* md, PyObject* obj, bool show_keys,
                bool show_values)
{
    int reprenter = Py_ReprEnter(obj);
    if (reprenter != 0) {
        return reprenter > 0 ? PyUnicode_FromString("...") : NULL;
    }

    PyObject* name =
        PyObject_GetAttr((PyObject*)Py_TYPE(obj), md->state->str_name);
    if (UNLIKELY(name == NULL)) {
        Py_ReprLeave(obj);
        return NULL;
    }

    PyUnicodeWriter* writer = PyUnicodeWriter_Create(1024);
    if (UNLIKELY(writer == NULL)) {
        Py_CLEAR(name);
        Py_ReprLeave(obj);
        return NULL;
    }

    if (UNLIKELY(PyUnicodeWriter_WriteChar(writer, '<') < 0)) {
        goto fail;
    }
    if (UNLIKELY(PyUnicodeWriter_WriteStr(writer, name) < 0)) {
        goto fail;
    }
    if (UNLIKELY(PyUnicodeWriter_WriteChar(writer, '(') < 0)) {
        goto fail;
    }

    md_repr_state_t state = {writer, show_keys, show_values, false, NULL};
    // only a keys view hides values, and it lists each key once
    if (!show_values && md->keys->maybe_dups) {
        state.shown = PySet_New(NULL);
        if (UNLIKELY(state.shown == NULL)) {
            goto fail;
        }
    }
    Py_ssize_t walked = md_walk_all(md, show_keys, _md_repr_visit, &state);
    Py_XDECREF(state.shown);
    if (UNLIKELY(walked < 0)) {
        goto fail;
    }

    if (UNLIKELY(PyUnicodeWriter_WriteChar(writer, ')') < 0)) {
        goto fail;
    }
    if (UNLIKELY(PyUnicodeWriter_WriteChar(writer, '>') < 0)) {
        goto fail;
    }
    Py_DECREF(name);
    Py_ReprLeave(obj);
    return PyUnicodeWriter_Finish(writer);
fail:
    Py_CLEAR(name);
    PyUnicodeWriter_Discard(writer);
    Py_ReprLeave(obj);
    return NULL;
}

static PyObject*
md_repr(MultiDictObject* md, PyObject* obj, bool show_keys, bool show_values)
{
    PyObject* ret;
    Py_BEGIN_CRITICAL_SECTION(md);
    ret = _md_repr_locked(md, obj, show_keys, show_values);
    Py_END_CRITICAL_SECTION();
    return ret;
}

#ifdef __cplusplus
}
#endif

#endif
