#include <string.h>
#include "_pair_list.h"

#include "Python.h"
#include "structmember.h"


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


static void pair_list_dealloc(istrobject *self)
{
    Py_XDECREF(self->canonical);
    PyUnicode_Type.tp_dealloc((PyObject*)self);
}

static PyObject *
pair_list_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyObject *x = NULL;
    static char *kwlist[] = {"object", "encoding", "errors", 0};
    char *encoding = NULL;
    char *errors = NULL;
    PyObject *s = NULL;
    PyObject *tmp = NULL;
    PyObject * new_args = NULL;
    PyObject * ret = NULL;

    ModData * state = global_state();

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|Oss:str",
                                     kwlist, &x, &encoding, &errors))
        return NULL;
    if (x == NULL) {
        s = state->emptystr;
        Py_INCREF(s);
    }
    else if (PyObject_IsInstance(x, (PyObject*)&istr_type)) {
        Py_INCREF(x);
        return x;
    }
    else {
        if (encoding == NULL && errors == NULL) {
            tmp = PyObject_Str(x);
        } else {
            tmp = PyUnicode_FromEncodedObject(x, encoding, errors);
        }
        if (!tmp) {
            goto finish;
        }
        s = PyObject_CallMethodObjArgs(tmp, state->title, NULL);
    }
    if (!s)
        goto finish;

    new_args = PyTuple_Pack(1, s);
    if (!new_args) {
        goto finish;
    }
    ret = PyUnicode_Type.tp_new(type, new_args, state->emptydict);
    if (!ret) {
        goto finish;
    }
    ((istrobject*)ret)->canonical = s;
    s = NULL;  /* the reference is stollen by .canonical */
finish:
    Py_XDECREF(tmp);
    Py_XDECREF(s);
    Py_XDECREF(new_args);
    return ret;
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
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    0,                                          /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                         /* tp_flags */
    0,                                          /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
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
    0,                                          /* tp_alloc */
    0,                                          /* tp_new */
};


static int mod_clear(PyObject *m)
{
  return 0;
}


static struct PyModuleDef _pair_list_module = {
    PyModuleDef_HEAD_INIT,
    "multidict._pair_list",
    pair_list__doc__,
    sizeof(ModData),
    NULL,  /* m_methods */
    NULL,  /* m_reload */
    NULL,  /* m_traverse */
    mod_clear,  /* m_clear */
    NULL   /* m_free */
};


PyObject* PyInit__istr(void)
{
    PyObject * tmp;
    PyObject *mod;

    mod = PyState_FindModule(&_istrmodule);
    if (mod) {
        Py_INCREF(mod);
        return mod;
    }

    istr_type.tp_base = &PyUnicode_Type;
    if (PyType_Ready(&istr_type) < 0) {
        return NULL;
    }

    mod = PyModule_Create(&_istrmodule);
    if (!mod) {
        return NULL;
    }
    tmp = PyUnicode_FromString("title");
    if (!tmp) {
        goto err;
    }
    modstate(mod)->title = tmp;
    tmp = PyUnicode_New(0, 0);
    if (!tmp) {
        goto err;
    }
    modstate(mod)->emptystr = tmp;
    tmp = PyUnicode_FromString("title");
    if(!tmp) {
        goto err;
    }
    modstate(mod)->title = tmp;

    Py_INCREF(&istr_type);
    if (PyModule_AddObject(mod, "istr", (PyObject *)&istr_type) < 0)
        goto err;

    return mod;
err:
    Py_DECREF(mod);
    return NULL;
}


