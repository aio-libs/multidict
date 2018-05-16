#include <string.h>
#include "_pair_list.h"

#include "Python.h"
#include "object.h"
#include "structmember.h"

#include <stdio.h>
#define MIN_LIST_CAPACITY 32

static PyTypeObject pair_list_type;


/*Global counter used to set ma_version_tag field of dictionary.
 * It is incremented each time that a dictionary is created and each
 * time that a dictionary is modified. */
static uint64_t pair_list_global_version = 0;

#define NEXT_VERSION() (++pair_list_global_version)


typedef struct pair {
    PyObject  *identity;  // 8
    PyObject  *key;       // 8
    PyObject  *value;     // 8
    Py_hash_t  hash;      // 8 ssize_t
} pair_t;


typedef struct pair_list {
    PyObject_HEAD
    Py_ssize_t  capacity;
    Py_ssize_t  size;
    Py_ssize_t  version;
    pair_t *pairs;
} pair_list_t;


inline static int str_cmp(PyObject *s1, PyObject *s2)
{
    PyObject *ret;
    ret = PyUnicode_RichCompare(s1, s2, Py_EQ);
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


static void
pair_clear(pair_t *pair)
{
    Py_DECREF(pair->identity);
    Py_DECREF(pair->key);
    Py_DECREF(pair->value);
}

static inline pair_t*
pair_list_get(pair_list_t *list, Py_ssize_t i)
{
    pair_t *item = list->pairs + i;
    return item;
}

static int
pair_list_resize(pair_list_t *list, Py_ssize_t new_capacity)
{
    printf("resize\n");
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

PyObject *
pair_list_new(void)
{
    pair_list_t *list = PyObject_GC_New(pair_list_t, &pair_list_type);
    if (list == NULL) {
        return NULL;
    }

    // TODO: align size of pair to the nearest power of 2
    list->pairs = PyMem_Malloc(MIN_LIST_CAPACITY * sizeof(pair_t));
    if (list->pairs == NULL) {
        return NULL;
    }

    list->capacity = MIN_LIST_CAPACITY;
    list->size = 0;
    list->version = NEXT_VERSION();

    return (PyObject *)list;
}


void
pair_list_dealloc(pair_list_t *list)
{
    PyObject_GC_UnTrack(list);
    Py_TRASHCAN_SAFE_BEGIN(list);
    for (size_t i = 0; i < list->size; i++) {
        pair_t *pair = pair_list_get(list, i);

        Py_XDECREF(pair->identity);
        Py_XDECREF(pair->key);
        Py_XDECREF(pair->value);
    }
    list->size = 0;

    PyMem_Free(list->pairs);
    list->pairs = NULL;

    Py_TYPE(list)->tp_free((PyObject *)list);
    Py_TRASHCAN_SAFE_END(list);
}


Py_ssize_t
pair_list_len(PyObject *op)
{
    pair_list_t *list = (pair_list_t *) op;
    return list->size;
}


int
pair_list_add_with_hash(PyObject *op,
              PyObject *identity,
              PyObject *key,
              PyObject *value,
              Py_hash_t hash)
{
    pair_list_t *list = (pair_list_t *) op;
    if (list->capacity <= list->size) {
        if (pair_list_resize(list, list->capacity + MIN_LIST_CAPACITY) < 0) {
            return -1;
        }
    }

    pair_t *pair = pair_list_get(list, list->size);

    Py_INCREF(identity);
    pair->identity = identity;

    Py_INCREF(key);
    pair->key = key;

    Py_INCREF(value);
    pair->value = value;

    pair->hash = hash;

    list->size += 1;
    list->version = NEXT_VERSION();

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
    list->version = NEXT_VERSION();

    if (list->size == pos) {
	// remove from tail, no need to shift body
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
_pair_list_drop_tail(PyObject *op, PyObject *identity, Py_hash_t hash,
		     Py_ssize_t pos)
{
    // return 1 if deleted, 0 if not found
    pair_t *pair;
    int ret;
    pair_list_t *list = (pair_list_t *) op;

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
	    pos--;
	}
	else if (ret == -1) {
	    return -1;
	}
    }
}

int
pair_list_del_hash(PyObject *op, PyObject *identity, PyObject *key, Py_hash_t hash)
{
    pair_list_t *list = (pair_list_t *)op;
    int ret = _pair_list_drop_tail(op, identity, hash, 0);
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
pair_list_del(PyObject *list, PyObject *identity, PyObject *key)
{
    Py_hash_t hash;
    hash = PyObject_Hash(identity);
    if (hash == -1) {
	return -1;
    }
    return pair_list_del_hash(list, identity, key, hash);
}


uint64_t 
pair_list_version(PyObject *op)
{
    pair_list_t *list = (pair_list_t *) op;
    return list->version;
}


int 
_pair_list_next(PyObject *op, Py_ssize_t *ppos,
		PyObject **pidentity, PyObject **pkey,
		PyObject **pvalue, Py_hash_t *phash)
{
    pair_list_t *list = (pair_list_t *) op;
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

    *ppos = *ppos + 1;
    return 1;
}


int 
pair_list_next(PyObject *op, Py_ssize_t *ppos,
	       PyObject **pidentity, PyObject **pkey,
	       PyObject **pvalue)
{
    Py_hash_t hash;
    return _pair_list_next(op, ppos, pidentity, pkey, pvalue, &hash);
}


int
pair_list_contains(PyObject *op, PyObject *ident)
{
    Py_hash_t hash1, hash2;
    Py_ssize_t pos = 0;
    PyObject *identity;
    int tmp;

    hash1 = PyObject_Hash(ident);
    if (hash1 == -1) {
	return -1;
    }
    while (_pair_list_next(op, &pos, &identity, NULL, NULL, &hash2)) {
        if (hash1 != hash2) {
	    continue;
	}
	tmp = str_cmp(ident, identity);
	if (tmp > 0) {
	    return 1;
	}
	else if (tmp < 0) {
	    return -1;
	}
    }
    return 0;
}


PyObject *
pair_list_get_one(PyObject *op, PyObject *ident, PyObject *key)
{
    Py_hash_t hash1, hash2;
    Py_ssize_t pos = 0;
    PyObject *identity;
    PyObject *value;
    int tmp;

    hash1 = PyObject_Hash(ident);
    if (hash1 == -1) {
	return NULL;
    }
    while (_pair_list_next(op, &pos, &identity, NULL, &value, &hash2)) {
        if (hash1 != hash2) {
	    continue;
	}
	tmp = str_cmp(ident, identity);
	if (tmp > 0) {
	    Py_INCREF(value);
	    return value;
	}
	else if (tmp < 0) {
	    return NULL;
	}
    }
    PyErr_SetObject(PyExc_KeyError, key);
    return NULL;
}


PyObject *
pair_list_get_all(PyObject *op, PyObject *ident, PyObject *key)
{
    Py_hash_t hash1, hash2;
    Py_ssize_t pos = 0;
    PyObject *identity;
    PyObject *value;
    int tmp;
    PyObject *res = NULL;

    hash1 = PyObject_Hash(ident);
    if (hash1 == -1) {
	return NULL;
    }
    while (_pair_list_next(op, &pos, &identity, NULL, &value, &hash2)) {
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
	    } else {
		if (PyList_Append(res, value) < 0) {
		    goto fail;
		}
	    }
	}
	else if (tmp < 0) {
	    goto fail;
	}
    }
    if (res == NULL) {
	PyErr_SetObject(PyExc_KeyError, key);
    }
    return res;

fail:
    Py_CLEAR(res);
    return NULL;
}


PyObject *
pair_list_set_default(PyObject *op, PyObject *ident,
		      PyObject *key, PyObject *value)
{
    Py_hash_t hash1, hash2;
    Py_ssize_t pos = 0;
    PyObject *identity;
    PyObject *value2;
    int tmp;

    hash1 = PyObject_Hash(ident);
    if (hash1 == -1) {
	return NULL;
    }
    while (_pair_list_next(op, &pos, &identity, NULL, &value2, &hash2)) {
        if (hash1 != hash2) {
	    continue;
	}
	tmp = str_cmp(ident, identity);
	if (tmp > 0) {
	    Py_INCREF(value);
	    return value2;
	}
	else if (tmp < 0) {
	    return NULL;
	}
    }
    if (pair_list_add_with_hash(op, ident, key, value, hash1) < 0) {
	return NULL;
    }
    Py_INCREF(value);
    return value;
}


PyObject *
pair_list_pop_one(PyObject *op, PyObject *ident, PyObject *key)
{
    pair_list_t *list = (pair_list_t *) op;
    Py_hash_t hash1, hash2;
    Py_ssize_t pos = 0;
    PyObject *identity;
    PyObject *value = NULL;
    int tmp;

    hash1 = PyObject_Hash(ident);
    if (hash1 == -1) {
	return NULL;
    }
    while (_pair_list_next(op, &pos, &identity, NULL, &value, &hash2)) {
        if (hash1 != hash2) {
	    continue;
	}
	tmp = str_cmp(ident, identity);
	if (tmp > 0) {
	    Py_INCREF(value);
	    if (pair_list_del_at(list, pos) < 0) {
		goto fail;
	    }
	    return value;
	}
	else if (tmp < 0) {
	    return NULL;
	}
    }
    PyErr_SetObject(PyExc_KeyError, key);
    return NULL;
fail:
    Py_CLEAR(value);
    return NULL;
}


PyObject *
pair_list_pop_all(PyObject *op, PyObject *ident, PyObject *key)
{
    pair_list_t *list = (pair_list_t *) op;
    Py_hash_t hash;
    Py_ssize_t pos;
    pair_t *pair;
    int tmp;
    PyObject *res = NULL;

    hash = PyObject_Hash(ident);
    if (hash == -1) {
	return NULL;
    }

    if (list->size == 0) {
	PyErr_SetObject(PyExc_KeyError, ident);
	return NULL;
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
	    } else {
		if (PyList_Append(res, pair->value) < 0) {
		    goto fail;
		}
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
    } else {
	if (PyList_Reverse(res) < 0) {
	    goto fail;
	}
    }
    return res;

fail:
    Py_CLEAR(res);
    return NULL;
}


PyObject *
pair_list_pop_item(PyObject *op)
{
    pair_list_t *list = (pair_list_t *) op;
    PyObject *ret;
    pair_t * pair;
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
	Py_CLEAR(ret);
	return NULL;
    }
    return ret;
}


int
pair_list_replace(PyObject *op, PyObject *identity, PyObject * key,
		  PyObject *value, Py_hash_t hash)
{
    pair_list_t *list = (pair_list_t *)op;
    pair_t *pair;

    Py_ssize_t pos;
    int tmp;
    int found = 0;

    for (pos = 0; pos < list->size; pos++) {
	pair = pair_list_get(list, pos);
        if (hash != pair->hash) {
	    continue;
	}
	tmp = str_cmp(identity, pair->identity);
	if (tmp > 0) {
	    found = 1;
	    Py_INCREF(key);
	    pair->key = key;
	    Py_INCREF(value);
	    pair->value = value;
	    break;
	}
	else if (tmp < 0) {
	    return -1;
	}
    }
    if (!found) {
	return pair_list_add_with_hash(op, identity, key, value, hash);
    }
    else {
	list->version = NEXT_VERSION();
	return _pair_list_drop_tail(op, identity, hash, pos+1);
    }
}

/***********************************************************************/

PyDoc_STRVAR(pair_list__doc__, "pair_list implementation");

/* We link this module statically for convenience.  If compiled as a shared
   library instead, some compilers don't allow addresses of Python objects
   defined in other libraries to be used in static initializers here.  The
   DEFERRED_ADDRESS macro is used to tag the slots where such addresses
   appear; the module init function must fill in the tagged slots at runtime.
   The argument is for documentation -- the macro ignores it.
*/
#define DEFERRED_ADDRESS(ADDR) 0



static int
pair_list_traverse(PyObject *op, visitproc visit, void *arg)
{
    pair_list_t *list = (pair_list_t *)op;
    pair_t * pair;
    Py_ssize_t pos;
    for (pos = 0; pos < list->size; pos++) {
	pair = pair_list_get(list, pos);
	// Don't need traverse key and identity: they are terminals
	Py_VISIT(pair->value);
    }
    return 0;
}


int
pair_list_clear(PyObject *op)
{
    pair_list_t *list = (pair_list_t *)op;
    pair_t * pair;
    Py_ssize_t pos;

    list->version = NEXT_VERSION();
    for (pos = 0; pos < list->size; pos++) {
	pair = pair_list_get(list, pos);
	Py_CLEAR(pair->key);
	Py_CLEAR(pair->identity);
	Py_CLEAR(pair->value);
    }

    return 0;
}


static PyTypeObject pair_list_type = {
    PyVarObject_HEAD_INIT(DEFERRED_ADDRESS(&PyType_Type), 0)
    "multidict._multidict._pair_list",
    sizeof(pair_list_t),
    0,
    (destructor)pair_list_dealloc,              /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_reserved */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    PyObject_HashNotImplemented,                /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    0,                                          /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,    /* tp_flags */
    0,                                          /* tp_doc */
    pair_list_traverse,                         /* tp_traverse */
    pair_list_clear,                            /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    0,                                          /* tp_methods */
    0,                                          /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    0,                                          /* tp_init */
    PyType_GenericAlloc,                        /* tp_alloc */
    0,                                          /* tp_new */
    PyObject_GC_Del,                            /* tp_free */
};



int pair_list_init(void)
{
    pair_list_type.tp_base = &PyUnicode_Type;
    if (PyType_Ready(&pair_list_type) < 0) {
        return -1;
    }
}


