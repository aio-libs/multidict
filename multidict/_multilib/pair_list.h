#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_PAIR_LIST_H
#define _MULTIDICT_PAIR_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct pair {
    PyObject  *identity;  // 8
    PyObject  *key;       // 8
    PyObject  *value;     // 8
    Py_hash_t  hash;      // 8
} pair_t;

/* Note about the structure size
With 29 pairs the MultiDict object size is slightly less than 1KiB
(1000-1008 bytes depending on Python version,
plus extra 12 bytes for memory allocator internal structures).
As the result the max reserved size is 1020 bytes at most.

To fit into 512 bytes, the structure can contain only 13 pairs
which is too small, e.g. https://www.python.org returns 16 headers
(9 of them are caching proxy information though).

The embedded buffer intention is to fit the vast majority of possible
HTTP headers into the buffer without allocating an extra memory block.
*/

#define EMBEDDED_CAPACITY 29

typedef struct pair_list {
    Py_ssize_t capacity;
    Py_ssize_t size;
    uint64_t version;
    bool calc_ci_indentity;
    pair_t *pairs;
    pair_t buffer[EMBEDDED_CAPACITY];
} pair_list_t;

#define MIN_CAPACITY 63
#define CAPACITY_STEP 64

/* Global counter used to set ma_version_tag field of dictionary.
 * It is incremented each time that a dictionary is created and each
 * time that a dictionary is modified. */
static uint64_t pair_list_global_version = 0;

#define NEXT_VERSION() (++pair_list_global_version)


typedef struct pair_list_pos {
    Py_ssize_t pos;
    uint64_t version;
} pair_list_pos_t;


static inline int
str_cmp(PyObject *s1, PyObject *s2)
{
    PyObject *ret = PyUnicode_RichCompare(s1, s2, Py_EQ);
    if (Py_IsTrue(ret)) {
        Py_DECREF(ret);
        return 1;
    }
    else if (ret == NULL) {
        return -1;
    }
    else {
        Py_DECREF(ret);
        return 0;
    }
}


static inline PyObject *
key_to_str(PyObject *key)
{
    PyTypeObject *type = Py_TYPE(key);
    if (type == &istr_type) {
        return Py_NewRef(((istrobject*)key)->canonical);
    }
    if (PyUnicode_CheckExact(key)) {
        Py_INCREF(key);
        return key;
    }
    if (PyUnicode_Check(key)) {
        return PyObject_Str(key);
    }
    PyErr_SetString(PyExc_TypeError,
                    "MultiDict keys should be either str "
                    "or subclasses of str");
    return NULL;
}


static inline PyObject *
ci_key_to_str(PyObject *key)
{
    PyTypeObject *type = Py_TYPE(key);
    if (type == &istr_type) {
        return Py_NewRef(((istrobject*)key)->canonical);
    }
    if (PyUnicode_Check(key)) {
        return PyObject_CallMethodNoArgs(key, multidict_str_lower);
    }
    PyErr_SetString(PyExc_TypeError,
                    "CIMultiDict keys should be either str "
                    "or subclasses of str");
    return NULL;
}


static inline int
pair_list_grow(pair_list_t *list)
{
    // Grow by one element if needed
    Py_ssize_t new_capacity;
    pair_t *new_pairs;

    if (list->size < list->capacity) {
        return 0;
    }

    if (list->pairs == list->buffer) {
        new_pairs = PyMem_New(pair_t, MIN_CAPACITY);
        memcpy(new_pairs, list->buffer, (size_t)list->capacity * sizeof(pair_t));

        list->pairs = new_pairs;
        list->capacity = MIN_CAPACITY;
        return 0;
    } else {
        new_capacity = list->capacity + CAPACITY_STEP;
        new_pairs = PyMem_Resize(list->pairs, pair_t, (size_t)new_capacity);

        if (NULL == new_pairs) {
            // Resizing error
            return -1;
        }

        list->pairs = new_pairs;
        list->capacity = new_capacity;
        return 0;
    }
}


static inline int
pair_list_shrink(pair_list_t *list)
{
    // Shrink by one element if needed.
    // Optimization is applied to prevent jitter
    // (grow-shrink-grow-shrink on adding-removing the single element
    // when the buffer is full).
    // To prevent this, the buffer is resized if the size is less than the capacity
    // by 2*CAPACITY_STEP factor.
    // The switch back to embedded buffer is never performed for both reasons:
    // the code simplicity and the jitter prevention.

    pair_t *new_pairs;
    Py_ssize_t new_capacity;

    if (list->capacity - list->size < 2 * CAPACITY_STEP) {
        return 0;
    }
    new_capacity = list->capacity - CAPACITY_STEP;
    if (new_capacity < MIN_CAPACITY) {
        return 0;
    }

    new_pairs = PyMem_Resize(list->pairs, pair_t, (size_t)new_capacity);

    if (NULL == new_pairs) {
        // Resizing error
        return -1;
    }

    list->pairs = new_pairs;
    list->capacity = new_capacity;

    return 0;
}


static inline int
_pair_list_init(pair_list_t *list, bool calc_ci_identity)
{
    list->calc_ci_indentity = calc_ci_identity;
    list->pairs = list->buffer;
    list->capacity = EMBEDDED_CAPACITY;
    list->size = 0;
    list->version = NEXT_VERSION();
    return 0;
}

static inline int
pair_list_init(pair_list_t *list)
{
    return _pair_list_init(list, /* calc_ci_identity = */ false);
}


static inline int
ci_pair_list_init(pair_list_t *list)
{
    return _pair_list_init(list, /* calc_ci_identity = */ true);
}


static inline PyObject *
pair_list_calc_identity(pair_list_t *list, PyObject *key)
{
    if (list->calc_ci_indentity)
        return ci_key_to_str(key);
    return key_to_str(key);
}

static inline void
pair_list_dealloc(pair_list_t *list)
{
    Py_ssize_t pos;

    for (pos = 0; pos < list->size; pos++) {
        pair_t *pair = list->pairs + pos;

        Py_CLEAR(pair->identity);
        Py_CLEAR(pair->key);
        Py_CLEAR(pair->value);
    }

    /*
    Strictly speaking, resetting size and capacity and
    assigning pairs to buffer is not necessary.
    Do it to consistency and idemotency.
    The cleanup doesn't hurt performance.
    !!!
    !!! The buffer deletion is crucial though.
    !!!
    */
    list->size = 0;
    if (list->pairs != list->buffer) {
        PyMem_Free(list->pairs);
        list->pairs = list->buffer;
        list->capacity = EMBEDDED_CAPACITY;
    }
}


static inline Py_ssize_t
pair_list_len(pair_list_t *list)
{
    return list->size;
}


static inline int
_pair_list_add_with_hash(pair_list_t *list,
                         PyObject *identity,
                         PyObject *key,
                         PyObject *value,
                         Py_hash_t hash)
{
    if (pair_list_grow(list) < 0) {
        return -1;
    }

    pair_t *pair = list->pairs + list->size;

    pair->identity = Py_NewRef(identity);

    pair->key = Py_NewRef(key);

    pair->value = Py_NewRef(value);

    pair->hash = hash;

    list->version = NEXT_VERSION();
    list->size += 1;

    return 0;
}


static inline int
pair_list_add(pair_list_t *list,
              PyObject *key,
              PyObject *value)
{
    PyObject *identity = pair_list_calc_identity(list, key);
    if (identity == NULL) {
        goto fail;
    }
    Py_hash_t hash = PyObject_Hash(identity);
    if (hash == -1) {
        goto fail;
    }
    int ret = _pair_list_add_with_hash(list, identity, key, value, hash);
    Py_DECREF(identity);
    return ret;
fail:
    Py_XDECREF(identity);
    return -1;
}


static inline int
pair_list_del_at(pair_list_t *list, Py_ssize_t pos)
{
    // return 1 on success, -1 on failure
    pair_t *pair = list->pairs + pos;
    Py_DECREF(pair->identity);
    Py_DECREF(pair->key);
    Py_DECREF(pair->value);

    list->size -= 1;
    list->version = NEXT_VERSION();

    if (list->size == pos) {
        // remove from tail, no need to shift body
        return 0;
    }

    Py_ssize_t tail = list->size - pos;
    // TODO: raise an error if tail < 0
    memmove((void *)(list->pairs + pos),
            (void *)(list->pairs + pos + 1),
            sizeof(pair_t) * (size_t)tail);

    return pair_list_shrink(list);
}


static inline int
_pair_list_drop_tail(pair_list_t *list, PyObject *identity, Py_hash_t hash,
                     Py_ssize_t pos)
{
    // return 1 if deleted, 0 if not found
    int found = 0;

    if (pos >= list->size) {
        return 0;
    }

    for (; pos < list->size; pos++) {
        pair_t *pair = list->pairs + pos;
        if (pair->hash != hash) {
            continue;
        }
        int ret = str_cmp(pair->identity, identity);
        if (ret > 0) {
            if (pair_list_del_at(list, pos) < 0) {
               return -1;
            }
            found = 1;
            pos--;
        }
        else if (ret == -1) {
            return -1;
        }
    }

    return found;
}


static inline int
pair_list_del(pair_list_t *list, PyObject *key)
{
    PyObject *identity = pair_list_calc_identity(list, key);
    if (identity == NULL) {
        goto fail;
    }

    Py_hash_t hash = PyObject_Hash(identity);
    if (hash == -1) {
        goto fail;
    }

    int ret = _pair_list_drop_tail(list, identity, hash, 0);

    if (ret < 0) {
        goto fail;
    }
    else if (ret == 0) {
        PyErr_SetObject(PyExc_KeyError, key);
        goto fail;
    }
    else {
        list->version = NEXT_VERSION();
    }
    Py_DECREF(identity);
    return 0;
fail:
    Py_XDECREF(identity);
    return -1;
}


static inline uint64_t
pair_list_version(pair_list_t *list)
{
    return list->version;
}


static inline void
pair_list_init_pos(pair_list_t *list, pair_list_pos_t *pos)
{
    pos->pos = 0;
    pos->version = list->version;
}

static inline int
pair_list_next(pair_list_t *list, pair_list_pos_t *pos,
               PyObject **pkey, PyObject **pvalue)
{
    if (pos->pos >= list->size) {
        return 0;
    }

    if (pos->version != list->version) {
        PyErr_SetString(PyExc_RuntimeError, "MultiDict changed during iteration");
        return -1;
    }


    pair_t *pair = list->pairs + pos->pos;

    if (pkey) {
        *pkey = Py_NewRef(pair->key);
    }
    if (pvalue) {
        *pvalue = Py_NewRef(pair->value);
    }

    ++pos->pos;
    return 1;
}


static inline int
pair_list_contains(pair_list_t *list, PyObject *key)
{
    Py_ssize_t pos;

    if (!PyUnicode_Check(key)) {
        return 0;
    }

    PyObject *ident = pair_list_calc_identity(list, key);
    if (ident == NULL) {
        goto fail;
    }

    Py_hash_t hash = PyObject_Hash(ident);
    if (hash == -1) {
        goto fail;
    }

    Py_ssize_t size = pair_list_len(list);

    for(pos = 0; pos < size; ++pos) {
        pair_t * pair = list->pairs + pos;
        if (hash != pair->hash) {
            continue;
        }
        int tmp = str_cmp(ident, pair->identity);
        if (tmp > 0) {
            Py_DECREF(ident);
            return 1;
        }
        else if (tmp < 0) {
            goto fail;
        }
    }

    Py_DECREF(ident);
    return 0;
fail:
    Py_XDECREF(ident);
    return -1;
}


static inline int
pair_list_get_one(pair_list_t *list, PyObject *key, PyObject **ret)
{
    Py_ssize_t pos;

    PyObject *ident = pair_list_calc_identity(list, key);
    if (ident == NULL) {
        goto fail;
    }

    Py_hash_t hash = PyObject_Hash(ident);
    if (hash == -1) {
        goto fail;
    }

    Py_ssize_t size = pair_list_len(list);

    for(pos = 0; pos < size; ++pos) {
        pair_t *pair = list->pairs + pos;
        if (hash != pair->hash) {
            continue;
        }
        int tmp = str_cmp(ident, pair->identity);
        if (tmp > 0) {
            Py_DECREF(ident);
            *ret = Py_NewRef(pair->value);
            return 0;
        }
        else if (tmp < 0) {
            goto fail;
        }
    }

    Py_DECREF(ident);
    return 0;
fail:
    Py_XDECREF(ident);
    return -1;
}


static inline int
pair_list_get_all(pair_list_t *list, PyObject *key, PyObject **ret)
{
    Py_ssize_t pos;
    PyObject *res = NULL;

    PyObject *ident = pair_list_calc_identity(list, key);
    if (ident == NULL) {
        goto fail;
    }

    Py_hash_t hash = PyObject_Hash(ident);
    if (hash == -1) {
        goto fail;
    }

    Py_ssize_t size = pair_list_len(list);
    for(pos = 0; pos < size; ++pos) {
        pair_t *pair = list->pairs + pos;

        if (hash != pair->hash) {
            continue;
        }
        int tmp = str_cmp(ident, pair->identity);
        if (tmp > 0) {
            if (res == NULL) {
                res = PyList_New(1);
                if (res == NULL) {
                    goto fail;
                }
                if (PyList_SetItem(res, 0, Py_NewRef(pair->value)) < 0) {
                    goto fail;
                }
            }
            else if (PyList_Append(res, pair->value) < 0) {
                goto fail;
            }
        }
        else if (tmp < 0) {
            goto fail;
        }
    }

    if (res != NULL) {
        *ret = res;
    }
    Py_DECREF(ident);
    return 0;

fail:
    Py_XDECREF(ident);
    Py_XDECREF(res);
    return -1;
}


static inline PyObject *
pair_list_set_default(pair_list_t *list, PyObject *key, PyObject *value)
{
    Py_ssize_t pos;

    PyObject *ident = pair_list_calc_identity(list, key);
    if (ident == NULL) {
        goto fail;
    }

    Py_hash_t hash = PyObject_Hash(ident);
    if (hash == -1) {
        goto fail;
    }
    Py_ssize_t size = pair_list_len(list);

    for(pos = 0; pos < size; ++pos) {
        pair_t * pair = list->pairs + pos;

        if (hash != pair->hash) {
            continue;
        }
        int tmp = str_cmp(ident, pair->identity);
        if (tmp > 0) {
            Py_DECREF(ident);
            return Py_NewRef(pair->value);
        }
        else if (tmp < 0) {
            goto fail;
        }
    }

    if (_pair_list_add_with_hash(list, ident, key, value, hash) < 0) {
        goto fail;
    }

    Py_DECREF(ident);
    return Py_NewRef(value);
fail:
    Py_XDECREF(ident);
    return NULL;
}


static inline int
pair_list_pop_one(pair_list_t *list, PyObject *key, PyObject **ret)
{
    Py_ssize_t pos;
    PyObject *value = NULL;

    PyObject *ident = pair_list_calc_identity(list, key);
    if (ident == NULL) {
        goto fail;
    }

    Py_hash_t hash = PyObject_Hash(ident);
    if (hash == -1) {
        goto fail;
    }

    for (pos=0; pos < list->size; pos++) {
        pair_t *pair = list->pairs + pos;
        if (pair->hash != hash) {
            continue;
        }
        int tmp = str_cmp(ident, pair->identity);
        if (tmp > 0) {
            value = Py_NewRef(pair->value);
            if (pair_list_del_at(list, pos) < 0) {
                goto fail;
            }
            Py_DECREF(ident);
            *ret = value;
            return 0;
        }
        else if (tmp < 0) {
            goto fail;
        }
    }

    return 0;
fail:
    Py_XDECREF(value);
    Py_XDECREF(ident);
    return -1;
}


static inline int
pair_list_pop_all(pair_list_t *list, PyObject *key, PyObject ** ret)
{
    Py_ssize_t pos;
    PyObject *lst = NULL;

    PyObject *ident = pair_list_calc_identity(list, key);
    if (ident == NULL) {
        goto fail;
    }

    Py_hash_t hash = PyObject_Hash(ident);
    if (hash == -1) {
        goto fail;
    }

    if (list->size == 0) {
        Py_DECREF(ident);
        return 0;
    }

    for (pos = list->size - 1; pos >= 0; pos--) {
        pair_t *pair = list->pairs + pos;
        if (hash != pair->hash) {
            continue;
        }
        int tmp = str_cmp(ident, pair->identity);
        if (tmp > 0) {
            if (lst == NULL) {
                lst = PyList_New(1);
                if (lst == NULL) {
                    goto fail;
                }
                if (PyList_SetItem(lst, 0, Py_NewRef(pair->value)) < 0) {
                    goto fail;
                }
            } else if (PyList_Append(lst, pair->value) < 0) {
                goto fail;
            }
            if (pair_list_del_at(list, pos) < 0) {
                goto fail;
            }
        }
        else if (tmp < 0) {
            goto fail;
        }
    }

    if (lst != NULL) {
        if (PyList_Reverse(lst) < 0) {
            goto fail;
        }
    }
    *ret = lst;
    Py_DECREF(ident);
    return 0;
fail:
    Py_XDECREF(ident);
    Py_XDECREF(lst);
    return -1;
}


static inline PyObject *
pair_list_pop_item(pair_list_t *list)
{
    if (list->size == 0) {
        PyErr_SetString(PyExc_KeyError, "empty multidict");
        return NULL;
    }

    pair_t *pair = list->pairs;
    PyObject *ret = PyTuple_Pack(2, pair->key, pair->value);
    if (ret == NULL) {
        return NULL;
    }

    if (pair_list_del_at(list, 0) < 0) {
        Py_DECREF(ret);
        return NULL;
    }

    return ret;
}


static inline int
pair_list_replace(pair_list_t *list, PyObject * key, PyObject *value)
{
    Py_ssize_t pos;
    int found = 0;

    PyObject *identity = pair_list_calc_identity(list, key);
    if (identity == NULL) {
        goto fail;
    }

    Py_hash_t hash = PyObject_Hash(identity);
    if (hash == -1) {
        goto fail;
    }


    for (pos = 0; pos < list->size; pos++) {
        pair_t *pair = list->pairs + pos;
        if (hash != pair->hash) {
            continue;
        }
        int tmp = str_cmp(identity, pair->identity);
        if (tmp > 0) {
            found = 1;
            Py_SETREF(pair->key, Py_NewRef(key));
            Py_SETREF(pair->value, Py_NewRef(value));
            break;
        }
        else if (tmp < 0) {
            goto fail;
        }
    }

    if (!found) {
        if (_pair_list_add_with_hash(list, identity, key, value, hash) < 0) {
            goto fail;
        }
        Py_DECREF(identity);
        return 0;
    }
    else {
        list->version = NEXT_VERSION();
        if (_pair_list_drop_tail(list, identity, hash, pos+1) < 0) {
            goto fail;
        }
        Py_DECREF(identity);
        return 0;
    }
fail:
    Py_XDECREF(identity);
    return -1;
}


static inline int
_dict_set_number(PyObject *dict, PyObject *key, Py_ssize_t num)
{
    PyObject *tmp = PyLong_FromSsize_t(num);
    if (tmp == NULL) {
       return -1;
    }

    if (PyDict_SetItem(dict, key, tmp) < 0) {
        Py_DECREF(tmp);
        return -1;
    }

    return 0;
}


static inline int
_pair_list_post_update(pair_list_t *list, PyObject* used_keys, Py_ssize_t pos)
{
    PyObject *tmp = NULL;

    for (; pos < list->size; pos++) {
        pair_t *pair = list->pairs + pos;
        int status = PyDict_GetItemRef(used_keys, pair->identity, &tmp);
        if (status == -1) {
            // exception set
            return -1;
        }
        else if (status == 0) {
            // not found
            continue;
        }

        Py_ssize_t num = PyLong_AsSsize_t(tmp);
        Py_DECREF(tmp);
        if (num == -1) {
            if (!PyErr_Occurred()) {
                PyErr_SetString(PyExc_RuntimeError, "invalid internal state");
            }
            return -1;
        }

        if (pos >= num) {
            // del self[pos]
            if (pair_list_del_at(list, pos) < 0) {
                return -1;
            }
            pos--;
        }
    }

    list->version = NEXT_VERSION();
    return 0;
}

// TODO: need refactoring function name
static inline int
_pair_list_update(pair_list_t *list, PyObject *key,
                  PyObject *value, PyObject *used_keys,
                  PyObject *identity, Py_hash_t hash)
{
    PyObject *item = NULL;
    Py_ssize_t pos;
    int found;

    int status = PyDict_GetItemRef(used_keys, identity, &item);
    if (status == -1) {
        // exception set
        return -1;
    }
    else if (status == 0) {
        // not found
        pos = 0;
    }
    else {
        pos = PyLong_AsSsize_t(item);
        Py_DECREF(item);
        if (pos == -1) {
            if (!PyErr_Occurred()) {
                PyErr_SetString(PyExc_RuntimeError, "invalid internal state");
            }
            return -1;
        }
    }

    found = 0;
    for (; pos < list->size; pos++) {
        pair_t *pair = list->pairs + pos;
        if (pair->hash != hash) {
            continue;
        }

        int ident_cmp_res = str_cmp(pair->identity, identity);
        if (ident_cmp_res > 0) {
            Py_INCREF(key);
            Py_DECREF(pair->key);
            pair->key = key;

            Py_INCREF(value);
            Py_DECREF(pair->value);
            pair->value = value;

            if (_dict_set_number(used_keys, pair->identity, pos + 1) < 0) {
                return -1;
            }

            found = 1;
            break;
        }
        else if (ident_cmp_res < 0) {
            return -1;
        }
    }

    if (!found) {
        if (_pair_list_add_with_hash(list, identity, key, value, hash) < 0) {
            return -1;
        }
        if (_dict_set_number(used_keys, identity, list->size) < 0) {
            return -1;
        }
    }

    return 0;
}


static inline int
pair_list_update(pair_list_t *list, pair_list_t *other)
{
    Py_ssize_t pos;

    if (other->size == 0) {
        return 0;
    }

    PyObject *used_keys = PyDict_New();
    if (used_keys == NULL) {
        return -1;
    }

    for (pos = 0; pos < other->size; pos++) {
        pair_t *pair = other->pairs + pos;
        if (_pair_list_update(list, pair->key, pair->value, used_keys,
                              pair->identity, pair->hash) < 0) {
            goto fail;
        }
    }

    if (_pair_list_post_update(list, used_keys, 0) < 0) {
        goto fail;
    }

    Py_DECREF(used_keys);
    return 0;

fail:
    Py_XDECREF(used_keys);
    return -1;
}


static inline int
pair_list_update_from_seq(pair_list_t *list, PyObject *seq)
{
    PyObject *fast = NULL; // item as a 2-tuple or 2-list
    PyObject *item = NULL; // seq[i]

    PyObject *key = NULL;
    PyObject *value = NULL;
    PyObject *identity = NULL;

    Py_ssize_t i;
    Py_ssize_t n;

    PyObject *it = PyObject_GetIter(seq);
    if (it == NULL) {
        return -1;
    }

    PyObject *used_keys = PyDict_New();
    if (used_keys == NULL) {
        goto fail_1;
    }

    for (i = 0; ; ++i) { // i - index into seq of current element
        fast = NULL;
        item = PyIter_Next(it);
        if (item == NULL) {
            if (PyErr_Occurred()) {
                goto fail_1;
            }
            break;
        }

        // Convert item to sequence, and verify length 2.
#ifdef Py_GIL_DISABLED
        if (!PySequence_Check(item)) {
#else
        fast = PySequence_Fast(item, "");
        if (fast == NULL) {
            if (PyErr_ExceptionMatches(PyExc_TypeError)) {
#endif
                PyErr_Format(PyExc_TypeError,
                             "multidict cannot convert sequence element #%zd"
                             " to a sequence",
                             i);
#ifndef Py_GIL_DISABLED
            }
#endif
            goto fail_1;
        }

#ifdef Py_GIL_DISABLED
        n = PySequence_Size(item);
#else
        n = PySequence_Fast_GET_SIZE(fast);
#endif
        if (n != 2) {
            PyErr_Format(PyExc_ValueError,
                         "multidict update sequence element #%zd "
                         "has length %zd; 2 is required",
                         i, n);
            goto fail_1;
        }

#ifdef Py_GIL_DISABLED
        key = PySequence_ITEM(item, 0);
        if (key == NULL) {
            PyErr_Format(PyExc_ValueError,
                         "multidict update sequence element #%zd's "
                         "key could not be fetched", i);
            goto fail_1;
        }
        value = PySequence_ITEM(item, 1);
        if (value == NULL) {
            PyErr_Format(PyExc_ValueError,
                         "multidict update sequence element #%zd's "
                         "value could not be fetched", i);
            goto fail_1;
        }
#else
        key = PySequence_Fast_GET_ITEM(fast, 0);
        value = PySequence_Fast_GET_ITEM(fast, 1);
        Py_INCREF(key);
        Py_INCREF(value);
#endif

        identity = pair_list_calc_identity(list, key);
        if (identity == NULL) {
            goto fail_1;
        }

        Py_hash_t hash = PyObject_Hash(identity);
        if (hash == -1) {
            goto fail_1;
        }

        if (_pair_list_update(list, key, value, used_keys, identity, hash) < 0) {
            goto fail_1;
        }

        Py_DECREF(key);
        Py_DECREF(value);
#ifndef Py_GIL_DISABLED
        Py_DECREF(fast);
#endif
        Py_DECREF(item);
        Py_DECREF(identity);
    }

    if (_pair_list_post_update(list, used_keys, 0) < 0) {
        goto fail_2;
    }

    Py_DECREF(it);
    Py_DECREF(used_keys);
    return 0;

fail_1:
    Py_XDECREF(key);
    Py_XDECREF(value);
    Py_XDECREF(fast);
    Py_XDECREF(item);
    Py_XDECREF(identity);

fail_2:
    Py_XDECREF(it);
    Py_XDECREF(used_keys);
    return -1;
}


static inline int
pair_list_eq(pair_list_t *list, pair_list_t *other)
{
    Py_ssize_t pos;

    if (list == other) {
        return 1;
    }

    Py_ssize_t size = pair_list_len(list);

    if (size != pair_list_len(other)) {
        return 0;
    }

    for(pos = 0; pos < size; ++pos) {
        pair_t *pair1 = list->pairs + pos;
        pair_t *pair2 = other->pairs +pos;

        if (pair1->hash != pair2->hash) {
            return 0;
        }

        int cmp = PyObject_RichCompareBool(pair1->identity, pair2->identity, Py_EQ);
        if (cmp < 0) {
            return -1;
        };
        if (cmp == 0) {
            return 0;
        }

        cmp = PyObject_RichCompareBool(pair1->value, pair2->value, Py_EQ);
        if (cmp < 0) {
            return -1;
        };
        if (cmp == 0) {
            return 0;
        }
    }

    return 1;
}

static inline int
pair_list_eq_to_mapping(pair_list_t *list, PyObject *other)
{
    PyObject *key = NULL;
    PyObject *avalue = NULL;
    PyObject *bvalue;

    Py_ssize_t other_len;

    if (!PyMapping_Check(other)) {
        PyErr_Format(PyExc_TypeError,
                     "other argument must be a mapping, not %s",
                     Py_TYPE(other)->tp_name);
        return -1;
    }

    other_len = PyMapping_Size(other);
    if (other_len < 0) {
        return -1;
    }
    if (pair_list_len(list) != other_len) {
        return 0;
    }

    pair_list_pos_t pos;
    pair_list_init_pos(list, &pos);

    for(;;) {
        int ret = pair_list_next(list, &pos, &key, &avalue);
        if (ret < 0) {
            return -1;
        }
        if (ret == 0) {
            break;
        }
        ret = PyMapping_GetOptionalItem(other, key, &bvalue);
        Py_CLEAR(key);
        if (ret < 0) {
            Py_CLEAR(avalue);
            return -1;
        }

        if (bvalue == NULL) {
            Py_CLEAR(avalue);
            return 0;
        }

        int eq = PyObject_RichCompareBool(avalue, bvalue, Py_EQ);
        Py_CLEAR(bvalue);
        Py_CLEAR(avalue);

        if (eq <= 0) {
            return eq;
        }
    }

    return 1;
}


/***********************************************************************/

static inline int
pair_list_traverse(pair_list_t *list, visitproc visit, void *arg)
{
    pair_t *pair = NULL;
    Py_ssize_t pos;

    for (pos = 0; pos < list->size; pos++) {
        pair = list->pairs + pos;
        // Don't need traverse the identity: it is a terminal
        Py_VISIT(pair->key);
        Py_VISIT(pair->value);
    }

    return 0;
}


static inline int
pair_list_clear(pair_list_t *list)
{
    pair_t *pair = NULL;
    Py_ssize_t pos;

    if (list->size == 0) {
        return 0;
    }

    list->version = NEXT_VERSION();
    for (pos = 0; pos < list->size; pos++) {
        pair = list->pairs + pos;
        Py_CLEAR(pair->key);
        Py_CLEAR(pair->identity);
        Py_CLEAR(pair->value);
    }
    list->size = 0;
    if (list->pairs != list->buffer) {
        PyMem_Free(list->pairs);
        list->pairs = list->buffer;
    }

    return 0;
}


#ifdef __cplusplus
}
#endif
#endif
