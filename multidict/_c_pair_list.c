#include <string.h>
#include "_c_pair_list.h"

#define MIN_LIST_CAPACITY 32

static void
pair_init(pair_t *pair,
          PyObject *identity,
          PyObject *key,
          PyObject *value,
          Py_hash_t hash)
{
    pair->identity = identity;
    pair->key = key;
    pair->value = value;
    pair->hash = hash;
}

static pair_t*
pair_list_item_at(pair_list_t *list, size_t i)
{
    pair_t *item = (pair_t*)(list->pairs + (sizeof(pair_t) * i));
    return item;
}

static bool
pair_list_incr_cap(pair_list_t *list)
{
    // TODO: use more smart algo for capacity grow
    size_t new_capacity = list->capacity * 2;
    pair_t *new_pairs = PyMem_RawRealloc(list->pairs, new_capacity);

    if (NULL == new_pairs) {
        // if not enought mem for realloc we do nothing, just return false
        return false;
    }

    list->pairs = new_pairs;
    list->capacity = new_capacity;

    return true;
}

pair_list_t *
pair_list_new()
{
    pair_list_t *list = (pair_list_t*)PyMem_RawMalloc(sizeof(pair_list_t));
    if (NULL == list) {
        return NULL;
    }

    // TODO: align size of pair to the nearest power of 2
    list->pairs = PyMem_RawCalloc(MIN_LIST_CAPACITY, sizeof(pair_t));
    if (NULL == list->pairs) {
        PyMem_RawFree(list);
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
        pair_t *pair = pair_list_item_at(list, i);

        Py_XDECREF(pair->identity);
        Py_XDECREF(pair->key);
        Py_XDECREF(pair->value);
    }

    PyMem_RawFree(list->pairs);
    list->pairs = NULL;

    PyMem_RawFree(list);
    list = NULL;
}

size_t
pair_list_len(pair_list_t *list)
{
    return list->size;
}

bool
pair_list_add(pair_list_t *list,
              PyObject *identity,
              PyObject *key,
              PyObject *value,
              Py_hash_t hash)
{
    // TODO: add incr all PyObjects

    if (list->capacity <= list->size) {
        if (!pair_list_incr_cap(list)) {
            return false;
        }
    }

    pair_t *new_pair = pair_list_item_at(list, list->size);

    Py_XINCREF(identity);
    Py_XINCREF(key);
    Py_XINCREF(value);

    pair_init(new_pair, identity, key, value, hash);

    list->size += 1;

    return true;
}

bool
pair_list_at(pair_list_t *list, size_t idx, pair_t *pair)
{
    if (idx > list->size) {
        return false;
    }

    pair_t *tmp_pair = pair_list_item_at(list, idx);

    memcpy(pair, tmp_pair, sizeof(pair_t));

    return true;
}
