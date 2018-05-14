#include <string.h>
#include "_c_pair_list.h"

#define MIN_LIST_CAPACITY 32

static void
pair_set(pair_t *pair,
	 PyObject *identity,
	 PyObject *key,
	 PyObject *value,
	 Py_hash_t hash)
{
    Py_INCREF(identity);
    pair->identity = identity;

    Py_INCREF(key);
    pair->key = key;

    Py_INCREF(value);
    pair->value = value;

    pair->hash = hash;
}


static void
pair_clear(pair_t *pair)
{
    Py_DECREF(pair->identity);
    Py_DECREF(pair->key);
    Py_DECREF(pair->value);
}

static pair_t*
pair_list_get(pair_list_t *list, size_t i)
{
    pair_t *item = list->pairs + i;
    return item;
}

static int
pair_list_realloc(pair_list_t *list, Py_ssize_t new_capacity)
{
    // TODO: use more smart algo for capacity grow
    pair_t *new_pairs = PyMem_Realloc(list->pairs, new_capacity);

    if (NULL == new_pairs) {
        // if not enought mem for realloc we do nothing, just return false
        return -1;
    }

    list->pairs = new_pairs;
    list->capacity = new_capacity;

    return 0;
}

pair_list_t *
pair_list_new()
{
    pair_list_t *list = (pair_list_t*)PyMem_Malloc(sizeof(pair_list_t));
    if (NULL == list) {
        return NULL;
    }

    // TODO: align size of pair to the nearest power of 2
    list->pairs = PyMem_Calloc(MIN_LIST_CAPACITY, sizeof(pair_t));
    if (NULL == list->pairs) {
        PyMem_Free(list);
        return NULL;
    }

    list->capacity = MIN_LIST_CAPACITY;
    list->size = 0;

    return list;
}

void
pair_list_free(pair_list_t *list)
{
    for (size_t i = 0; i < list->size; i++) {
        pair_t *pair = pair_list_get(list, i);

        Py_XDECREF(pair->identity);
        Py_XDECREF(pair->key);
        Py_XDECREF(pair->value);
    }

    PyMem_Free(list->pairs);
    list->pairs = NULL;

    PyMem_Free(list);
    list = NULL;
}

Py_ssize_t
pair_list_len(pair_list_t *list)
{
    return list->size;
}

int
pair_list_add(pair_list_t *list,
              PyObject *identity,
              PyObject *key,
              PyObject *value,
              Py_hash_t hash)
{
    if (list->capacity <= list->size) {
        if (pair_list_resize(list, list->capacity + MIN_LIST_CAPACITY) < 0) {
            return -1;
        }
    }

    pair_t *new_pair = pair_list_get(list, list->size);

    pair_set(new_pair, identity, key, value, hash);

    list->size += 1;

    return 0;
}


int
pair_list_del(pair_list_t *list, PyObject *identity)
{
    Py_hash_t hash;
    hash = PyObject_Hash(identity);
    if (hash == -1) {
	return -1;
    }
    return pair_list_del_hash(list, identity, hash);
}


int
pair_list_del_hash(pair_list_t *list, PyObject *identity, Py_hash_t hash)
{
    // return 1 if deleted, 0 if not found
    Py_ssize_t pos;
    pair_t * pair;
    int ret;

    for(pos=0; pos < list->size; pos++) {
        pair = pair_list_get(list, pos);
	if (pair->hash != hash) {
	    continue;
	}
	ret = PyUnicode_Compare(pair->identity, identity);
	if (ret == 0) {
	    return pair_list_del_at(pos);
	}
	if (ret == -1) {
	    if (PyErr_Occurred() != NULL) {
		return -1;
	    }
	}
    }
    return 0;
}


static int
pair_list_del_at(pair_list_t *list, Py_ssize_t pos)
{
    // return 1 on success, -1 on failure
    Py_ssize_t tail;
    pair_t *pair;

    pair = pair_list_get(list, pos);
    pair_clear(pair);

    list->size -= 1;

    if (list->size == 0) {
        return 1;
    }
    tail = list->size - pos;
    memcpy(pair_list_get(list, pos +1),
	   pair_list_get(list, pos),
	   sizeof(pair_t) * tail);
    if (list->capacity - list->size > MIN_LIST_CAPACITY) {
	if (list->capacity > MIN_LIST_CAPACITY) {
	    return pair_list_resize(list, list->capacity - MIN_LIST_CAPACITY);
	}
    }
    return 0;
}


int
pair_list_at(pair_list_t *list, size_t idx, pair_t *pair)
{
    if (idx > list->size) {
        return -1;
    }

    pair_t *tmp_pair = pair_list_get(list, idx);

    memcpy(pair, tmp_pair, sizeof(pair_t));

    return 0;
}
