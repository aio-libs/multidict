#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_PAIR_H
#define _MULTIDICT_PAIR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>

/* list[i] as a new reference. On a free-threaded build another thread can
   drop the item between a borrow and its incref, or shrink the list after
   its length was checked, so PyList_GetItemRef takes the reference
   atomically (locking the list only if its lock-free attempt fails). An
   item that is gone by then means the list changed under the caller, and
   is reported as a RuntimeError: the caller sees NULL with the error set.
   GIL builds keep the macro and compile the check away: nothing can run
   between the length check and the borrow. */
#ifdef Py_GIL_DISABLED
static inline PyObject*
_list_getitem_ref(PyObject* list, Py_ssize_t i)
{
    PyObject* item = PyList_GetItemRef(list, i);
    if (item == NULL && PyErr_ExceptionMatches(PyExc_IndexError)) {
        PyErr_Clear();
        PyErr_SetString(PyExc_RuntimeError,
                        "list changed size during iteration");
    }
    return item;
}
#define _list_item_gone(item) ((item) == NULL)
#else
#define _list_getitem_ref(list, i) Py_NewRef(PyList_GET_ITEM((list), (i)))
#define _list_item_gone(item) (0)
#endif

typedef enum {
    UNPACK_OK,     /* *pkey and *pvalue are new references */
    UNPACK_OTHER,  /* not an exact tuple or list */
    UNPACK_LENGTH, /* an exact tuple or list of *plen elements, not 2 */
    UNPACK_ERROR,  /* exception set */
} unpack_t;

/* The (key, value) fast path shared by itemsview.__contains__ and the
   sequence parser behind extend()/update()/merge(). Both also accept an
   arbitrary two-element sequence, but disagree on what a wrong length or
   a foreign type means, so those are reported back rather than raised
   here. *plen is only written for UNPACK_LENGTH, which is an error path
   in both callers; the general path is not an option for an exact list,
   whose items have to be read through _list_getitem_ref(). */
static inline unpack_t
unpack_pair(PyObject* obj, PyObject** pkey, PyObject** pvalue,
            Py_ssize_t* plen)
{
    if (PyTuple_CheckExact(obj)) {
        if (PyTuple_GET_SIZE(obj) != 2) {
            *plen = PyTuple_GET_SIZE(obj);
            return UNPACK_LENGTH;
        }
        *pkey = Py_NewRef(PyTuple_GET_ITEM(obj, 0));
        *pvalue = Py_NewRef(PyTuple_GET_ITEM(obj, 1));
        return UNPACK_OK;
    }
    if (PyList_CheckExact(obj)) {
        if (PyList_GET_SIZE(obj) != 2) {
            *plen = PyList_GET_SIZE(obj);
            return UNPACK_LENGTH;
        }
        *pkey = _list_getitem_ref(obj, 0);
        if (_list_item_gone(*pkey)) {
            return UNPACK_ERROR;
        }
        *pvalue = _list_getitem_ref(obj, 1);
        if (_list_item_gone(*pvalue)) {
            Py_CLEAR(*pkey);
            return UNPACK_ERROR;
        }
        return UNPACK_OK;
    }
    return UNPACK_OTHER;
}

#ifdef __cplusplus
}
#endif

#endif
