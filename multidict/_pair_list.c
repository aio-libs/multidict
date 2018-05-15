#include <string.h>
#include "_pair_list.h"

#include "Python.h"
#include "object.h"
#include "structmember.h"


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
    pair_t *pairs;
    Py_ssize_t  capacity;
    Py_ssize_t  size;
    Py_ssize_t  version;
} pair_list_t;


int inline static cmp_str(PyObject *s1, PyObject *s2)
{
    int ret;
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
pair_list_resize(pair_list_t *list, Py_ssize_t new_capacity)
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

PyObject *
pair_list_new(void)
{
    pair_list_t *list = PyObject_GC_New(pair_list_t, &pair_list_type);
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
pair_list_add(PyObject *op,
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

    pair_t *new_pair = pair_list_get(list, list->size);

    pair_set(new_pair, identity, key, value, hash);

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
pair_list_del_hash(PyObject *op, PyObject *identity, Py_hash_t hash)
{
    // return 1 if deleted, 0 if not found
    Py_ssize_t pos;
    pair_t *pair;
    int ret;
    pair_list_t *list = (pair_list_t *) op;

    for (pos = 0; pos < list->size; pos++) {
        pair = pair_list_get(list, pos);
	if (pair->hash != hash) {
	    continue;
	}
	ret = str_cmp(pair->identity, identity);
	if (ret) {
	    return pair_list_del_at(list, pos);
	}
	else if (ret == -1) {
	    return -1;
	}
    }
    return 0;
}


int
pair_list_del(PyObject *list, PyObject *identity)
{
    Py_hash_t hash;
    hash = PyObject_Hash(identity);
    if (hash == -1) {
	return -1;
    }
    return pair_list_del_hash(list, identity, hash);
}


int
pair_list_at(PyObject *op, size_t idx, pair_t *pair)
{
    pair_list_t *list = (pair_list_t *) op;
    if (idx > list->size) {
        return -1;
    }

    pair_t *tmp_pair = pair_list_get(list, idx);

    memcpy(pair, tmp_pair, sizeof(pair_t));

    return 0;
}


uint64_t 
pair_list_version(PyObject *op)
{
    pair_list_t *list = (pair_list_t *) op;
    return list->version;
}


int 
_pair_list_next(pair_list_t *list, Py_ssize_t *ppos,
		PyObject **pkey, PyObject **pvalue, Py_hash_t *hash)
{
    pair_t *pair = pair_list_get(list, *ppos);
    *ppos = *ppos + 1;
    if (*ppos >= list->size) {
	return 0;
    }
    if (pkey) {
	*pkey = pair->key;
    }
    if (pvalue) {
	*pvalue = pair->value;
    }
    *hash = pair->hash;
    return 1;
}


PyObject *
pair_list_get_one(PyObject *op, PyObject *ident)
{
    pair_list_t *list = (pair_list_t *) op;
    Py_hash_t hash1, hash2;
    Py_ssize_t pos = 0;
    PyObject *identity;
    PyObject *value;
    int ret;

    hash1 = PyObject_Hash(identity);
    if (hash1 == -1) {
	return NULL;
    }
    while (_pair_list_next(list, &pos, &identity, &value, &hash2)) {
        if (hash1 != hash2) {
	    continue;
	}
	ret = str_cmp(ident, identity, Py_EQ);
	if (ret == Py_True) {
	    Py_DECREF(ret);
	    Py_INCREF(value);
	    return value;
	}
	else if (ret == NULL) {
	    return NULL;
	}
	else {
	    Py_DECREF(ret);
	}
    }
    return NULL;
}


int 
pair_list_next(PyObject *op, Py_ssize_t *ppos,
		   PyObject **pkey, PyObject **pvalue)
{
    pair_list_t *list = (pair_list_t *) op;
    Py_hash_t hash;
    return _pair_list_next(list, ppos, pkey, pvalue, &hash);
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
     Py_ssize_t i;
     for (i = 0; i < list->size; i++) {
	 pair = pair_list_get(list, i);
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
    Py_ssize_t i;

    list->version = NEXT_VERSION();
    for (i = 0; i < list->size; i++) {
	pair = pair_list_get(list, i);
	Py_CLEAR(pair->key);
	Py_CLEAR(pair->identity);
	Py_CLEAR(pair->value);
    }

    return 0;
}


static PyTypeObject pair_list_type = {
    PyVarObject_HEAD_INIT(DEFERRED_ADDRESS(&PyType_Type), 0)
    "multidict._pair_list._pair_list",
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
    PyObject_GenericGetAttr,                    /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                         /* tp_flags */
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


static int mod_clear(PyObject *m)
{
  return 0;
}


static struct PyModuleDef _pair_list_module = {
    PyModuleDef_HEAD_INIT,
    "multidict._pair_list",
    pair_list__doc__,
    0,
    NULL,  /* m_methods */
    NULL,  /* m_reload */
    NULL,  /* m_traverse */
    mod_clear,  /* m_clear */
    NULL   /* m_free */
};


PyObject* PyInit__pair_list(void)
{
    PyObject *mod;

    pair_list_type.tp_base = &PyUnicode_Type;
    if (PyType_Ready(&pair_list_type) < 0) {
        return NULL;
    }

    mod = PyModule_Create(&_pair_list_module);
    if (!mod) {
        return NULL;
    }
    return mod;
}


