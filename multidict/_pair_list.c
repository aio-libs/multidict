#include <string.h>
#include "_istr.h"
#include "_pair_list.h"

#include <Python.h>

#define MIN_LIST_CAPACITY 32

static PyObject * _istr_type;


_Py_IDENTIFIER(lower);


/* Global counter used to set ma_version_tag field of dictionary.
 * It is incremented each time that a dictionary is created and each
 * time that a dictionary is modified. */
static uint64_t pair_list_global_version = 0;

#define NEXT_VERSION() (++pair_list_global_version)


static inline int
str_cmp(PyObject *s1, PyObject *s2)
{
    PyObject *ret = PyUnicode_RichCompare(s1, s2, Py_EQ);
    if (ret == Py_True) {
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
    if (PyUnicode_CheckExact(key)) {
        Py_INCREF(key);
        return key;
    }
    if ((PyObject *)type == _istr_type) {
        return PyObject_Str(key);
    }
    if (PyUnicode_Check(key)) {
        return PyObject_Str(key);
    }
    PyErr_SetString(PyExc_TypeError,
                    "MultiDict keys should be either str "
                    "or subclasses of str");
    return NULL;
}


static PyObject *
ci_key_to_str(PyObject *key)
{
    PyObject *ret;
    PyTypeObject *type = Py_TYPE(key);
    if ((PyObject *)type == _istr_type) {
        ret = ((istrobject*)key)->canonical;
        Py_INCREF(ret);
        return ret;
    }
    if (PyUnicode_Check(key)) {
        return _PyObject_CallMethodId(key, &PyId_lower, NULL);
    }
    PyErr_SetString(PyExc_TypeError,
                    "CIMultiDict keys should be either str "
                    "or subclasses of str");
    return NULL;
}

static inline pair_t *
pair_list_get(pair_list_t *list, Py_ssize_t i)
{
    pair_t *item = list->pairs + i;
    return item;
}

static int
pair_list_resize(pair_list_t *list, Py_ssize_t new_capacity)
{
    pair_t *new_pairs;
    // TODO: use more smart algo for capacity grow
    if (new_capacity < MIN_LIST_CAPACITY) {
        new_capacity = MIN_LIST_CAPACITY;
    }
    if (list->capacity == new_capacity) {
        // No need to resize
        return 0;
    }

    new_pairs = PyMem_Resize(list->pairs, pair_t, (size_t)new_capacity);

    if (NULL == new_pairs) {
        // if not enought mem for realloc we do nothing, just return false
        return -1;
    }

    list->pairs = new_pairs;
    list->capacity = new_capacity;

    return 0;
}


static inline int
_pair_list_init(pair_list_t *list, calc_identity_func calc_identity)
{
    // TODO: align size of pair to the nearest power of 2
    list->pairs = PyMem_New(pair_t, MIN_LIST_CAPACITY);
    if (list->pairs == NULL) {
        return -1;
    }

    list->capacity = MIN_LIST_CAPACITY;
    list->size = 0;
    list->version = NEXT_VERSION();
    list->calc_identity = calc_identity;
    return 0;
}

int
pair_list_init(pair_list_t *list)
{
    return _pair_list_init(list, key_to_str);
}


int
ci_pair_list_init(pair_list_t *list)
{
    return _pair_list_init(list, ci_key_to_str);
}


void
pair_list_dealloc(pair_list_t *list)
{
    pair_t *pair;
    Py_ssize_t pos;

    for (pos = 0; pos < list->size; pos++) {
        pair = pair_list_get(list, pos);

        Py_XDECREF(pair->identity);
        Py_XDECREF(pair->key);
        Py_XDECREF(pair->value);
    }

    list->size = 0;
    if (list->pairs != NULL) {
        PyMem_Del(list->pairs);
        list->pairs = NULL;
    }
}


Py_ssize_t
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
    pair_t *pair;

    if (list->capacity < list->size + 1) {
        if (pair_list_resize(list, list->capacity + MIN_LIST_CAPACITY) < 0) {
            return -1;
        }
    }

    pair = pair_list_get(list, list->size);
    list->size += 1;

    Py_INCREF(identity);
    pair->identity = identity;

    Py_INCREF(key);
    pair->key = key;

    Py_INCREF(value);
    pair->value = value;

    pair->hash = hash;

    list->version = NEXT_VERSION();

    return 0;
}


int
pair_list_add(pair_list_t *list,
              PyObject *key,
              PyObject *value)
{
    Py_hash_t hash;
    PyObject *identity = NULL;
    int ret;

    identity = list->calc_identity(key);
    if (identity == NULL) {
        goto fail;
    }
    hash = PyObject_Hash(identity);
    if (hash == -1) {
        goto fail;
    }
    ret = _pair_list_add_with_hash(list, identity, key, value, hash);
    Py_DECREF(identity);
    return ret;
fail:
    Py_XDECREF(identity);
    return -1;
}


static int
pair_list_del_at(pair_list_t *list, Py_ssize_t pos)
{
    // return 1 on success, -1 on failure
    Py_ssize_t tail;
    pair_t *pair;

    pair = pair_list_get(list, pos);
    Py_DECREF(pair->identity);
    Py_DECREF(pair->key);
    Py_DECREF(pair->value);

    list->size -= 1;
    list->version = NEXT_VERSION();

    if (list->size == pos) {
        // remove from tail, no need to shift body
        return 0;
    }

    tail = list->size - pos;
    // TODO: raise an error if tail < 0
    memmove((void *)pair_list_get(list, pos),
            (void *)pair_list_get(list, pos + 1),
            sizeof(pair_t) * (size_t)tail);

    if (list->capacity - list->size > MIN_LIST_CAPACITY) {
        return pair_list_resize(list, list->capacity - MIN_LIST_CAPACITY);
    }

    return 0;
}


int
_pair_list_drop_tail(pair_list_t *list, PyObject *identity, Py_hash_t hash,
                     Py_ssize_t pos)
{
    // return 1 if deleted, 0 if not found
    pair_t *pair;
    int ret;
    int found = 0;

    if (pos >= list->size) {
        return 0;
    }

    for (; pos < list->size; pos++) {
        pair = pair_list_get(list, pos);
        if (pair->hash != hash) {
            continue;
        }
        ret = str_cmp(pair->identity, identity);
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

static int
_pair_list_del_hash(pair_list_t *list, PyObject *identity,
                    PyObject *key, Py_hash_t hash)
{
    int ret = _pair_list_drop_tail(list, identity, hash, 0);

    if (ret < 0) {
        return -1;
    }
    else if (ret == 0) {
        PyErr_SetObject(PyExc_KeyError, key);
        return -1;
    }
    else {
        list->version = NEXT_VERSION();
        return 0;
    }
}


int
pair_list_del(pair_list_t *list, PyObject *key)
{
    PyObject *identity = NULL;
    Py_hash_t hash;
    int ret;

    identity = list->calc_identity(key);
    if (identity == NULL) {
        goto fail;
    }

    hash = PyObject_Hash(identity);
    if (hash == -1) {
        goto fail;
    }

    ret = _pair_list_del_hash(list, identity, key, hash);
    Py_DECREF(identity);
    return ret;
fail:
    Py_XDECREF(identity);
    return -1;
}


uint64_t
pair_list_version(pair_list_t *list)
{
    return list->version;
}


inline int
_pair_list_next(pair_list_t *list, Py_ssize_t *ppos, PyObject **pidentity,
                PyObject **pkey, PyObject **pvalue, Py_hash_t *phash)
{
    pair_t *pair;

    if (*ppos >= list->size) {
        return 0;
    }

    pair = pair_list_get(list, *ppos);

    if (pidentity) {
        *pidentity = pair->identity;
    }
    if (pkey) {
        *pkey = pair->key;
    }
    if (pvalue) {
        *pvalue = pair->value;
    }
    if (phash) {
        *phash = pair->hash;
    }

    *ppos += 1;
    return 1;
}


inline int
pair_list_next(pair_list_t *list, Py_ssize_t *ppos, PyObject **pidentity,
               PyObject **pkey, PyObject **pvalue)
{
    Py_hash_t hash;
    return _pair_list_next(list, ppos, pidentity, pkey, pvalue, &hash);
}


int
pair_list_contains(pair_list_t *list, PyObject *key)
{
    Py_hash_t hash1, hash2;
    Py_ssize_t pos = 0;
    PyObject *ident = NULL;
    PyObject *identity = NULL;
    int tmp;

    ident = list->calc_identity(key);
    if (ident == NULL) {
        goto fail;
    }

    hash1 = PyObject_Hash(ident);
    if (hash1 == -1) {
        goto fail;
    }

    while (_pair_list_next(list, &pos, &identity, NULL, NULL, &hash2)) {
        if (hash1 != hash2) {
            continue;
        }
        tmp = str_cmp(ident, identity);
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


PyObject *
pair_list_get_one(pair_list_t *list, PyObject *key)
{
    Py_hash_t hash1, hash2;
    Py_ssize_t pos = 0;
    PyObject *ident = NULL;
    PyObject *identity = NULL;
    PyObject *value = NULL;
    int tmp;

    ident = list->calc_identity(key);
    if (ident == NULL) {
        goto fail;
    }

    hash1 = PyObject_Hash(ident);
    if (hash1 == -1) {
        goto fail;
    }

    while (_pair_list_next(list, &pos, &identity, NULL, &value, &hash2)) {
        if (hash1 != hash2) {
            continue;
        }
        tmp = str_cmp(ident, identity);
        if (tmp > 0) {
            Py_INCREF(value);
            Py_DECREF(ident);
            return value;
        }
        else if (tmp < 0) {
            goto fail;
        }
    }

    Py_DECREF(ident);
    PyErr_SetObject(PyExc_KeyError, key);
    return NULL;
fail:
    Py_XDECREF(ident);
    return NULL;
}


PyObject *
pair_list_get_all(pair_list_t *list, PyObject *key)
{
    Py_hash_t hash1, hash2;
    Py_ssize_t pos = 0;
    PyObject *ident = NULL;
    PyObject *identity = NULL;
    PyObject *value = NULL;
    PyObject *res = NULL;
    int tmp;

    ident = list->calc_identity(key);
    if (ident == NULL) {
        goto fail;
    }

    hash1 = PyObject_Hash(ident);
    if (hash1 == -1) {
        goto fail;
    }

    while (_pair_list_next(list, &pos, &identity, NULL, &value, &hash2)) {
        if (hash1 != hash2) {
            continue;
        }
        tmp = str_cmp(ident, identity);
        if (tmp > 0) {
            if (res == NULL) {
                res = PyList_New(1);
                if (res == NULL) {
                    goto fail;
                }
                if (PyList_SetItem(res, 0, value) < 0) {
                    goto fail;
                }
                Py_INCREF(value);
            }
            else if (PyList_Append(res, value) < 0) {
                goto fail;
            }
        }
        else if (tmp < 0) {
            goto fail;
        }
    }

    if (res == NULL) {
        PyErr_SetObject(PyExc_KeyError, key);
    }
    Py_DECREF(ident);
    return res;

fail:
    Py_XDECREF(ident);
    Py_XDECREF(res);
    return NULL;
}


PyObject *
pair_list_set_default(pair_list_t *list, PyObject *key, PyObject *value)
{
    Py_hash_t hash1, hash2;
    Py_ssize_t pos = 0;
    PyObject *ident = NULL;
    PyObject *identity = NULL;
    PyObject *value2 = NULL;
    int tmp;

    ident = list->calc_identity(key);
    if (ident == NULL) {
        goto fail;
    }

    hash1 = PyObject_Hash(ident);
    if (hash1 == -1) {
        goto fail;
    }

    while (_pair_list_next(list, &pos, &identity, NULL, &value2, &hash2)) {
        if (hash1 != hash2) {
            continue;
        }
        tmp = str_cmp(ident, identity);
        if (tmp > 0) {
            Py_INCREF(value2);
            Py_DECREF(ident);
            return value2;
        }
        else if (tmp < 0) {
            goto fail;
        }
    }

    if (_pair_list_add_with_hash(list, ident, key, value, hash1) < 0) {
        goto fail;
    }

    Py_INCREF(value);
    Py_DECREF(ident);
    return value;
fail:
    Py_XDECREF(ident);
    return NULL;
}


PyObject *
pair_list_pop_one(pair_list_t *list, PyObject *key)
{
    pair_t *pair;

    Py_hash_t hash;
    Py_ssize_t pos;
    PyObject *value = NULL;
    int tmp;
    PyObject *ident = NULL;

    ident = list->calc_identity(key);
    if (ident == NULL) {
        goto fail;
    }

    hash = PyObject_Hash(ident);
    if (hash == -1) {
        goto fail;
    }

    for (pos=0; pos < list->size; pos++) {
        pair = pair_list_get(list, pos);
        if (pair->hash != hash) {
            continue;
        }
        tmp = str_cmp(ident, pair->identity);
        if (tmp > 0) {
            value = pair->value;
            Py_INCREF(value);
            if (pair_list_del_at(list, pos) < 0) {
                goto fail;
            }
            Py_DECREF(ident);
            return value;
        }
        else if (tmp < 0) {
            goto fail;
        }
    }

    PyErr_SetObject(PyExc_KeyError, key);
    goto fail;

fail:
    Py_XDECREF(value);
    Py_XDECREF(ident);
    return NULL;
}


PyObject *
pair_list_pop_all(pair_list_t *list, PyObject *key)
{
    Py_hash_t hash;
    Py_ssize_t pos;
    pair_t *pair;
    int tmp;
    PyObject *res = NULL;
    PyObject *ident = NULL;

    ident = list->calc_identity(key);
    if (ident == NULL) {
        goto fail;
    }

    hash = PyObject_Hash(ident);
    if (hash == -1) {
        goto fail;
    }

    if (list->size == 0) {
        PyErr_SetObject(PyExc_KeyError, ident);
        goto fail;
    }

    for (pos = list->size - 1; pos >= 0; pos--) {
        pair = pair_list_get(list, pos);
        if (hash != pair->hash) {
            continue;
        }
        tmp = str_cmp(ident, pair->identity);
        if (tmp > 0) {
            if (res == NULL) {
                res = PyList_New(1);
                if (res == NULL) {
                    goto fail;
                }
                if (PyList_SetItem(res, 0, pair->value) < 0) {
                    goto fail;
                }
                Py_INCREF(pair->value);
            } else if (PyList_Append(res, pair->value) < 0) {
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

    if (res == NULL) {
        PyErr_SetObject(PyExc_KeyError, key);
    } else if (PyList_Reverse(res) < 0) {
        goto fail;
    }
    Py_DECREF(ident);
    return res;

fail:
    Py_XDECREF(ident);
    Py_XDECREF(res);
    return NULL;
}


PyObject *
pair_list_pop_item(pair_list_t *list)
{
    PyObject *ret;
    pair_t *pair;

    if (list->size == 0) {
        PyErr_SetString(PyExc_KeyError, "empty multidict");
        return NULL;
    }

    pair = pair_list_get(list, 0);
    ret = PyTuple_Pack(2, pair->key, pair->value);
    if (ret == NULL) {
        return NULL;
    }

    if (pair_list_del_at(list, 0) < 0) {
        Py_DECREF(ret);
        return NULL;
    }

    return ret;
}


int
pair_list_replace(pair_list_t *list, PyObject * key, PyObject *value)
{
    pair_t *pair;

    Py_ssize_t pos;
    int tmp;
    int found = 0;

    PyObject *identity = NULL;
    Py_hash_t hash;

    identity = list->calc_identity(key);
    if (identity == NULL) {
        goto fail;
    }

    hash = PyObject_Hash(identity);
    if (hash == -1) {
        goto fail;
    }


    for (pos = 0; pos < list->size; pos++) {
        pair = pair_list_get(list, pos);
        if (hash != pair->hash) {
            continue;
        }
        tmp = str_cmp(identity, pair->identity);
        if (tmp > 0) {
            found = 1;
            Py_INCREF(key);
            Py_DECREF(pair->key);
            pair->key = key;
            Py_INCREF(value);
            Py_DECREF(pair->value);
            pair->value = value;
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


static int
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


static int
_pair_list_post_update(pair_list_t *list, PyObject* used_keys, Py_ssize_t pos)
{
    pair_t *pair;
    PyObject *tmp;
    Py_ssize_t num;

    for (; pos < list->size; pos++) {
        pair = pair_list_get(list, pos);
        tmp = PyDict_GetItem(used_keys, pair->identity);
        if (tmp == NULL) {
            // not found
            continue;
        }

        num = PyLong_AsSsize_t(tmp);
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
    pair_t *pair = NULL;
    Py_ssize_t pos;
    int found;
    int ident_cmp_res;

    item = PyDict_GetItem(used_keys, identity);
    if (item == NULL) {
        pos = 0;
    }
    else {
        pos = PyLong_AsSsize_t(item);
        if (pos == -1) {
            if (!PyErr_Occurred()) {
                PyErr_SetString(PyExc_RuntimeError, "invalid internal state");
            }
            return -1;
        }
    }

    found = 0;
    for (; pos < list->size; pos++) {
        pair = pair_list_get(list, pos);
        if (pair->hash != hash) {
            continue;
        }

        ident_cmp_res = str_cmp(pair->identity, identity);
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


int
pair_list_update(pair_list_t *list, pair_list_t *other)
{
    PyObject *used_keys = NULL;
    pair_t *pair = NULL;

    Py_ssize_t pos;

    if (other->size == 0) {
        return 0;
    }

    used_keys = PyDict_New();
    if (used_keys == NULL) {
        return -1;
    }

    for (pos = 0; pos < other->size; pos++) {
        pair = pair_list_get(other, pos);
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


int
pair_list_update_from_seq(pair_list_t *list, PyObject *seq)
{
    PyObject *it = NULL; // iter(seq)
    PyObject *fast = NULL; // item as a 2-tuple or 2-list
    PyObject *item = NULL; // seq[i]
    PyObject *used_keys = NULL; // dict(<Identitty: Pos>)

    PyObject *key = NULL;
    PyObject *value = NULL;
    PyObject *identity = NULL;

    Py_hash_t hash;

    Py_ssize_t i;
    Py_ssize_t n;

    it = PyObject_GetIter(seq);
    if (it == NULL) {
        return -1;
    }

    used_keys = PyDict_New();
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
        fast = PySequence_Fast(item, "");
        if (fast == NULL) {
            if (PyErr_ExceptionMatches(PyExc_TypeError)) {
                PyErr_Format(PyExc_TypeError,
                             "multidict cannot convert sequence element #%zd"
                             " to a sequence",
                             i);
            }
            goto fail_1;
        }

        n = PySequence_Fast_GET_SIZE(fast);
        if (n != 2) {
            PyErr_Format(PyExc_ValueError,
                         "multidict update sequence element #%zd "
                         "has length %zd; 2 is required",
                         i, n);
            goto fail_1;
        }

        key = PySequence_Fast_GET_ITEM(fast, 0);
        value = PySequence_Fast_GET_ITEM(fast, 1);
        Py_INCREF(key);
        Py_INCREF(value);

        identity = list->calc_identity(key);
        if (identity == NULL) {
            goto fail_1;
        }

        hash = PyObject_Hash(identity);
        if (hash == -1) {
            goto fail_1;
        }

        if (_pair_list_update(list, key, value, used_keys, identity, hash) < 0) {
            goto fail_1;
        }

        Py_DECREF(key);
        Py_DECREF(value);
        Py_DECREF(fast);
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

int
pair_list_eq_to_mapping(pair_list_t *list, PyObject *other)
{
    PyObject *key = NULL;
    PyObject *avalue = NULL;
    PyObject *bvalue = NULL;

    Py_ssize_t pos;

    int cmp;

    if (!PyMapping_Check(other)) {
        PyErr_Format(PyExc_TypeError,
                     "other argument must be a mapping, not %s",
                     Py_TYPE(other)->tp_name);
        return -1;
    }

    if (pair_list_len(list) != PyMapping_Length(other)) {
        return 0;
    }

    pos = 0;
    while (pair_list_next(list, &pos, NULL, &key, &avalue)) {
        bvalue = PyObject_GetItem(other, key);
        if (bvalue == NULL) {
            PyErr_Clear();
            return 0;
        }

        cmp = PyObject_RichCompareBool(avalue, bvalue, Py_EQ);
        Py_DECREF(bvalue);

        if (cmp < 0) {
            return -1;
        }
        else if (cmp > 0) {
            continue;
        }
        else {
            return 0;
        }
    }

    return 1;
}


/***********************************************************************/

int
pair_list_traverse(pair_list_t *list, visitproc visit, void *arg)
{
    pair_t *pair = NULL;
    Py_ssize_t pos;

    for (pos = 0; pos < list->size; pos++) {
        pair = pair_list_get(list, pos);
        // Don't need traverse the identity: it is a terminal
        Py_VISIT(pair->key);
        Py_VISIT(pair->value);
    }

    return 0;
}


int
pair_list_clear(pair_list_t *list)
{
    pair_t *pair = NULL;
    Py_ssize_t pos;

    if (list->size == 0) {
        return 0;
    }

    list->version = NEXT_VERSION();
    for (pos = 0; pos < list->size; pos++) {
        pair = pair_list_get(list, pos);
        Py_CLEAR(pair->key);
        Py_CLEAR(pair->identity);
        Py_CLEAR(pair->value);
    }
    list->size = 0;
    return pair_list_resize(list, 0);
}



int
pair_list_global_init(PyObject *istr_type)
{
    Py_INCREF(istr_type);
    _istr_type = istr_type;
    return 0;
}
