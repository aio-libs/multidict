#ifndef _MULTIDICT_VIEWS_H
#define _MULTIDICT_VIEWS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "debug.h"
#include "dict.h"
#include "hashtable.h"
#include "state.h"
#include "unpack.h"

typedef struct {
    PyObject_HEAD
    MultiDictObject* md;
} _Multidict_ViewObject;

/********** Base **********/

/* A pooled shell keeps the GC preheader it was allocated with, which
   PyObject_GC_UnTrack() left in the untracked state, so only the object
   header has to be put back the way PyObject_GC_New() leaves it.

   Out of line on purpose: inlined into all three view constructors it
   grows the translation unit enough that GCC stops inlining
   md_calc_identity() into get(), which costs more on every lookup than
   the call saves here. */
NOINLINE static _Multidict_ViewObject*
_multidict_view_alloc(MultiDictObject* md, PyTypeObject* tp)
{
    _Multidict_ViewObject* mv = pool_pop(&md->state->view_pool);
    if (mv == NULL) {
        return PyObject_GC_New(_Multidict_ViewObject, tp);
    }
    /* PyObject_Init() rather than setting the two fields by hand: it
       also does the refcount-total bookkeeping a debug interpreter
       expects of a newly live object. */
    PyObject_Init((PyObject*)mv, tp);
    return mv;
}

NOINLINE static PyObject*
_multidict_view_new(MultiDictObject* md, PyTypeObject* tp)
{
    _Multidict_ViewObject* mv = _multidict_view_alloc(md, tp);
    if (mv == NULL) {
        return NULL;
    }
    mv->md = (MultiDictObject*)Py_NewRef(md);
    PyObject_GC_Track(mv);
    return (PyObject*)mv;
}

NOINLINE static void
multidict_view_tp_dealloc(_Multidict_ViewObject* self)
{
    PyTypeObject* tp = Py_TYPE(self);
    PyObject_GC_UnTrack(self);
    /* The pool is reached through md, so a view the GC already cleared
       cannot be pooled; that only happens to one caught in a cycle. */
    MultiDictObject* md = self->md;
    bool pooled = md != NULL && pool_push(&md->state->view_pool, self);
    Py_XDECREF(md);
    if (!pooled) {
        tp->tp_free(self);
    }
    Py_DECREF(tp);
}

NOINLINE static int
multidict_view_tp_traverse(_Multidict_ViewObject* self, visitproc visit,
                           void* arg)
{
    Py_VISIT(Py_TYPE(self));
    Py_VISIT(self->md);
    return 0;
}

NOINLINE static int
multidict_view_tp_clear(_Multidict_ViewObject* self)
{
    Py_CLEAR(self->md);
    return 0;
}

NOINLINE static Py_ssize_t
multidict_view_sq_length(_Multidict_ViewObject* self)
{
    return md_len(self->md);
}

/* Whether every item of `items` is in `container`: a bool, or NULL. */
NOINLINE static PyObject*
_view_all_in(PyObject* items, PyObject* container)
{
    PyObject* iter = PyObject_GetIter(items);
    if (iter == NULL) {
        return NULL;
    }
    PyObject* item = NULL;
    int st;
    while ((st = PyIter_NextItem(iter, &item)) > 0) {
        int tmp = PySequence_Contains(container, item);
        Py_DECREF(item);
        if (tmp <= 0) {
            Py_DECREF(iter);
            if (tmp < 0) {
                return NULL;
            }
            Py_RETURN_FALSE;
        }
    }
    Py_DECREF(iter);
    if (st < 0) {
        return NULL;
    }
    Py_RETURN_TRUE;
}

NOINLINE static PyObject*
multidict_view_richcompare(_Multidict_ViewObject* self, PyObject* other,
                           int op)
{
    Py_ssize_t self_size = md_len(self->md);
    Py_ssize_t size = PyObject_Length(other);
    if (size < 0) {
        if (!PyErr_ExceptionMatches(PyExc_TypeError)) {
            return NULL;  // propagate MemoryError / KeyboardInterrupt / etc.
        }
        PyErr_Clear();
        Py_RETURN_NOTIMPLEMENTED;
    }
    // The sizes rule most answers out; the rest is a subset test.
    bool sizes_fit;
    PyObject* subset = (PyObject*)self;
    PyObject* superset = other;
    switch (op) {
        case Py_LT:
            sizes_fit = self_size < size;
            break;
        case Py_LE:
            sizes_fit = self_size <= size;
            break;
        case Py_EQ:
        case Py_NE:
            sizes_fit = self_size == size;
            break;
        case Py_GT:
            sizes_fit = self_size > size;
            subset = other;
            superset = (PyObject*)self;
            break;
        case Py_GE:
            sizes_fit = self_size >= size;
            subset = other;
            superset = (PyObject*)self;
            break;
        default:
            Py_UNREACHABLE();
    }
    if (!sizes_fit) {
        return PyBool_FromLong(op == Py_NE);
    }
    PyObject* ret = _view_all_in(subset, superset);
    if (ret != NULL && op == Py_NE) {
        bool equal = Py_IsTrue(ret);
        Py_DECREF(ret);
        return PyBool_FromLong(!equal);
    }
    return ret;
}

/* The answer of a set operator to an operand it cannot iterate over. */
NOINLINE static PyObject*
_type_error_to_notimplemented(void)
{
    if (PyErr_ExceptionMatches(PyExc_TypeError)) {
        PyErr_Clear();
        Py_RETURN_NOTIMPLEMENTED;
    }
    return NULL;
}

/* The set operators below are COLD on purpose. Compiled for size, they
   leave GCC's inlining budget to the lookups and iterators: as plain
   functions they cost FT builds 3% on `del d[key]` and on items iteration
   (#1669). */

/* `reflected` tells the view is the right operand. */
typedef PyObject* (*view_binop)(_Multidict_ViewObject* self, PyObject* other,
                                bool reflected);

/* Runs `op` for whichever operand is an items view (`items`) or a keys
   view, under its multidict's critical section if `lock`. */
COLD static PyObject*
_view_binop(PyObject* lft, PyObject* rht, bool items, bool lock, view_binop op)
{
    mod_state* state;
    int tmp = get_mod_state_by_def_checked(lft, &state);
    if (tmp < 0) {
        return NULL;
    } else if (tmp == 0) {
        tmp = get_mod_state_by_def_checked(rht, &state);
        if (tmp < 0) {
            return NULL;
        } else if (tmp == 0) {
            Py_RETURN_NOTIMPLEMENTED;
        }
    }
    assert(state != NULL);
    PyTypeObject* tp = items ? state->ItemsViewType : state->KeysViewType;
    _Multidict_ViewObject* self;
    PyObject* other;
    bool reflected;
    if (Py_IS_TYPE(lft, tp)) {
        self = (_Multidict_ViewObject*)lft;
        other = rht;
        reflected = false;
    } else if (Py_IS_TYPE(rht, tp)) {
        self = (_Multidict_ViewObject*)rht;
        other = lft;
        reflected = true;
    } else {
        Py_RETURN_NOTIMPLEMENTED;
    }
    if (!lock) {
        return op(self, other, reflected);
    }
    PyObject* ret;
    Py_BEGIN_CRITICAL_SECTION(self->md);
    ret = op(self, other, reflected);
    Py_END_CRITICAL_SECTION();
    return ret;
}

// Not locked: the operators it is built from take the lock themselves.
COLD static PyObject*
_view_xor(_Multidict_ViewObject* self, PyObject* other, bool reflected)
{
    (void)reflected;  // symmetric
    PyObject* ret = NULL;
    PyObject* tmp1 = NULL;
    PyObject* tmp2 = NULL;
    PyObject* rht = PySet_New(other);
    if (rht == NULL) {
        return _type_error_to_notimplemented();
    }
    tmp1 = PyNumber_Subtract((PyObject*)self, rht);
    if (tmp1 == NULL) {
        goto done;
    }
    tmp2 = PyNumber_Subtract(rht, (PyObject*)self);
    if (tmp2 == NULL) {
        goto done;
    }
    ret = PyNumber_InPlaceOr(tmp1, tmp2);
done:
    Py_XDECREF(tmp1);
    Py_XDECREF(tmp2);
    Py_DECREF(rht);
    return ret;
}

/********** Items **********/

static inline PyObject*
multidict_itemsview_new(MultiDictObject* md)
{
    return _multidict_view_new(md, md->state->ItemsViewType);
}

NOINLINE static PyObject*
multidict_itemsview_tp_iter(_Multidict_ViewObject* self)
{
    return multidict_items_iter_new(self->md, 0);
}

NOINLINE static PyObject*
multidict_itemsview_tp_repr(_Multidict_ViewObject* self)
{
    PyObject* ret;
    Py_BEGIN_CRITICAL_SECTION(self->md);
    ret = md_repr(self->md, (PyObject*)self, true, true);
    Py_END_CRITICAL_SECTION();
    return ret;
}

static inline int
_multidict_itemsview_parse_item(_Multidict_ViewObject* self, PyObject* arg,
                                PyObject** pidentity, PyObject** pkey,
                                PyObject** pvalue)
{
    assert(pidentity != NULL);
    if (!PyTuple_Check(arg)) {
        return 0;
    }

    Py_ssize_t size = PyTuple_Size(arg);
    if (size != 2) {
        return 0;
    }

    PyObject* key = Py_NewRef(PyTuple_GET_ITEM(arg, 0));

    if (pkey != NULL) {
        *pkey = Py_NewRef(key);
    }
    if (pvalue != NULL) {
        *pvalue = Py_NewRef(PyTuple_GET_ITEM(arg, 1));
    }

    *pidentity = md_calc_identity(self->md, key, self->md->is_ci);
    Py_DECREF(key);
    if (*pidentity == NULL) {
        if (pkey != NULL) {
            Py_CLEAR(*pkey);
        }
        if (pvalue != NULL) {
            Py_CLEAR(*pvalue);
        }
        if (PyErr_ExceptionMatches(PyExc_TypeError)) {
            PyErr_Clear();
            return 0;
        } else {
            return -1;
        }
    }
    return 1;
}

static inline int
_set_add(PyObject* set, PyObject* key, PyObject* value)
{
    PyObject* tpl = PyTuple_Pack(2, key, value);
    if (tpl == NULL) {
        return -1;
    }
    int tmp = PySet_Add(set, tpl);
    Py_DECREF(tpl);
    return tmp;
}

static int
_multidict_collect_visit(void* user_data, PyObject* identity, Py_hash_t hash,
                         PyObject* key, PyObject* value)
{
    (void)identity;
    (void)hash;
    PyObject* item;
    if (key != NULL) {
        item = PyTuple_Pack(2, key, value);
        if (item == NULL) {
            return -1;
        }
    } else {
        item = Py_NewRef(value);
    }
    int tmp = PyList_Append((PyObject*)user_data, item);
    Py_DECREF(item);
    return tmp < 0 ? -1 : 1;
}

/* Collect every (key, value) pair matching `identity` into a fresh list,
   so that callers can run a custom __eq__ against it after the walk.

   `with_keys` selects values (false) or (key, value) tuples (true). */
static inline PyObject*
_multidict_collect_matches(MultiDictObject* md, PyObject* identity,
                           bool with_keys)
{
    PyObject* ret = PyList_New(0);
    if (ret == NULL) {
        return NULL;
    }
    if (md_walk(md, identity, with_keys, _multidict_collect_visit, ret) < 0) {
        Py_DECREF(ret);
        return NULL;
    }
    return ret;
}

/* Whether md has `value` under `identity`: 1, 0, or -1 on error.

   The matches are materialized before PyObject_RichCompareBool() runs: a
   custom __eq__ on `value` could re-enter and mutate md mid-walk. */
COLD static int
_itemsview_has(MultiDictObject* md, PyObject* identity, PyObject* value)
{
    PyObject* matches = _multidict_collect_matches(md, identity, false);
    if (matches == NULL) {
        return -1;
    }
    int ret = 0;
    Py_ssize_t n = PyList_GET_SIZE(matches);
    for (Py_ssize_t i = 0; i < n && ret == 0; i++) {
        ret = PyObject_RichCompareBool(
            value, PyList_GET_ITEM(matches, i), Py_EQ);
    }
    Py_DECREF(matches);
    return ret;
}

/* `view & other` answers with the view's keys, `other & view` with
   other's. */
COLD static PyObject*
_itemsview_and(_Multidict_ViewObject* self, PyObject* other, bool reflected)
{
    PyObject* identity = NULL;
    PyObject* key = NULL;
    PyObject* value = NULL;
    PyObject* arg = NULL;
    PyObject* ret = NULL;
    PyObject* matches = NULL;
    int st;

    PyObject* iter = PyObject_GetIter(other);
    if (iter == NULL) {
        return _type_error_to_notimplemented();
    }
    ret = PySet_New(NULL);
    if (ret == NULL) {
        goto fail;
    }
    while ((st = PyIter_NextItem(iter, &arg)) > 0) {
        int tmp = _multidict_itemsview_parse_item(
            self, arg, &identity, &key, &value);
        if (tmp < 0) {
            goto fail;
        } else if (tmp == 0) {
            Py_DECREF(arg);
            continue;
        }

        // See _itemsview_has() on why the matches are materialized.
        matches = _multidict_collect_matches(self->md, identity, !reflected);
        if (matches == NULL) {
            goto fail;
        }

        Py_ssize_t n = PyList_GET_SIZE(matches);
        for (Py_ssize_t i = 0; i < n; i++) {
            PyObject* match = PyList_GET_ITEM(matches, i);  // borrowed
            PyObject* key2 = reflected ? key : PyTuple_GET_ITEM(match, 0);
            PyObject* value2 = reflected ? match : PyTuple_GET_ITEM(match, 1);
            tmp = PyObject_RichCompareBool(value, value2, Py_EQ);
            if (tmp < 0) {
                goto fail;
            }
            if (tmp > 0) {
                if (_set_add(ret, key2, value2) < 0) {
                    goto fail;
                }
            }
        }
        Py_CLEAR(matches);
        Py_DECREF(arg);
        Py_CLEAR(identity);
        Py_CLEAR(key);
        Py_CLEAR(value);
    }
    if (st < 0) {
        goto fail;
    }
    Py_DECREF(iter);
    return ret;
fail:
    Py_CLEAR(matches);
    Py_CLEAR(arg);
    Py_CLEAR(identity);
    Py_CLEAR(key);
    Py_CLEAR(value);
    Py_CLEAR(iter);
    Py_CLEAR(ret);
    return NULL;
}

NOINLINE static PyObject*
multidict_itemsview_nb_and(PyObject* lft, PyObject* rht)
{
    return _view_binop(lft, rht, true, true, _itemsview_and);
}

/* The items of `other` the view has no equal pair for, added to the
   view's own if `with_self`: `view | other`, or `other - view`. */
COLD static PyObject*
_itemsview_missing(_Multidict_ViewObject* self, PyObject* other,
                   bool with_self)
{
    PyObject* identity = NULL;
    PyObject* value = NULL;
    PyObject* arg = NULL;
    PyObject* ret = NULL;
    int st;

    PyObject* iter = PyObject_GetIter(other);
    if (iter == NULL) {
        return _type_error_to_notimplemented();
    }
    ret = PySet_New(with_self ? (PyObject*)self : NULL);
    if (ret == NULL) {
        goto fail;
    }
    while ((st = PyIter_NextItem(iter, &arg)) > 0) {
        // an item that is not a pair is not in the view either
        int found = _multidict_itemsview_parse_item(
            self, arg, &identity, NULL, &value);
        if (found > 0) {
            found = _itemsview_has(self->md, identity, value);
            Py_CLEAR(identity);
            Py_CLEAR(value);
        }
        if (found < 0) {
            goto fail;
        }
        if (found == 0) {
            if (PySet_Add(ret, arg) < 0) {
                goto fail;
            }
        }
        Py_DECREF(arg);
    }
    if (st < 0) {
        goto fail;
    }
    Py_DECREF(iter);
    return ret;
fail:
    Py_XDECREF(arg);
    Py_DECREF(iter);
    Py_XDECREF(ret);
    return NULL;
}

/* The view's pairs that `other` has no equal item for, added to other's
   own if `with_other`: `other | view`, or `view - other`. */
COLD static PyObject*
_itemsview_unmatched(_Multidict_ViewObject* self, PyObject* other,
                     bool with_other)
{
    PyObject* identity = NULL;
    PyObject* iter = NULL;
    PyObject* key = NULL;
    PyObject* value = NULL;
    PyObject* arg = NULL;
    PyObject* tmp_set = NULL;
    PyObject* ret = NULL;
    int st;

    if (with_other) {
        ret = PySet_New(other);
        if (ret == NULL) {
            return _type_error_to_notimplemented();
        }
    }
    iter = PyObject_GetIter(other);
    if (iter == NULL) {
        if (!with_other) {
            return _type_error_to_notimplemented();
        }
        goto fail;
    }
    if (!with_other) {
        ret = PySet_New(NULL);
        if (ret == NULL) {
            goto fail;
        }
    }
    tmp_set = PySet_New(NULL);
    if (tmp_set == NULL) {
        goto fail;
    }
    while ((st = PyIter_NextItem(iter, &arg)) > 0) {
        int tmp = _multidict_itemsview_parse_item(
            self, arg, &identity, NULL, &value);
        if (tmp < 0) {
            goto fail;
        } else if (tmp > 0) {
            if (_set_add(tmp_set, identity, value) < 0) {
                goto fail;
            }
        }
        Py_DECREF(arg);
        Py_CLEAR(identity);
        Py_CLEAR(value);
    }
    if (st < 0) {
        goto fail;
    }
    Py_CLEAR(iter);

    MultiDictObject* md = self->md;
    uint64_t version = md->version;
    htkeys_t* keys = md->keys;
    entry_t* entries = htkeys_entries(keys);
    uint8_t kind = keys->kind;

    for (Py_ssize_t pos = 0; pos < keys->nentries; ++pos) {
        entry_t* entry = entry_at(kind, entries, pos);
        if (entry_is_hole(entry)) {
            continue;
        }
        identity = Py_NewRef(entry_identity(kind, md->is_ci, entry));
        value = Py_NewRef(entry->value);
        key = md_ensure_key(md, entry);  // last entry access
        if (key == NULL) {
            goto fail;
        }
        PyObject* tpl = PyTuple_Pack(2, identity, value);
        if (tpl == NULL) {
            goto fail;
        }
        int tmp = PySet_Contains(tmp_set, tpl);
        Py_DECREF(tpl);
        if (tmp < 0) {
            goto fail;
        }
        if (tmp == 0) {
            if (_set_add(ret, key, value) < 0) {
                goto fail;
            }
        }
        Py_CLEAR(identity);
        Py_CLEAR(key);
        Py_CLEAR(value);
        /* Hashing and comparing run Python code; once the version checks
           out, `keys` is still md's table. */
        if (md_check_version(md, version) < 0) {
            goto fail;
        }
    }
    Py_DECREF(tmp_set);
    return ret;
fail:
    Py_CLEAR(arg);
    Py_CLEAR(identity);
    Py_CLEAR(key);
    Py_CLEAR(value);
    Py_CLEAR(iter);
    Py_CLEAR(ret);
    Py_CLEAR(tmp_set);
    return NULL;
}

static PyObject*
_itemsview_or(_Multidict_ViewObject* self, PyObject* other, bool reflected)
{
    if (reflected) {
        return _itemsview_unmatched(self, other, true);
    }
    return _itemsview_missing(self, other, true);
}

NOINLINE static PyObject*
multidict_itemsview_nb_or(PyObject* lft, PyObject* rht)
{
    return _view_binop(lft, rht, true, true, _itemsview_or);
}

static PyObject*
_itemsview_sub(_Multidict_ViewObject* self, PyObject* other, bool reflected)
{
    if (reflected) {
        return _itemsview_missing(self, other, false);
    }
    return _itemsview_unmatched(self, other, false);
}

NOINLINE static PyObject*
multidict_itemsview_nb_subtract(PyObject* lft, PyObject* rht)
{
    return _view_binop(lft, rht, true, true, _itemsview_sub);
}

NOINLINE static PyObject*
multidict_itemsview_xor(PyObject* lft, PyObject* rht)
{
    return _view_binop(lft, rht, true, false, _view_xor);
}

static inline int
_multidict_itemsview_contains_impl(_Multidict_ViewObject* self, PyObject* obj)
{
    PyObject* identity = NULL;
    PyObject* key = NULL;
    PyObject* value = NULL;
    Py_ssize_t len;
    int ret = 0;

    switch (unpack_pair(obj, &key, &value, &len)) {
        case UNPACK_OK:
            break;
        case UNPACK_LENGTH:
            return 0;
        case UNPACK_ERROR:
            return -1;
        case UNPACK_OTHER:
            len = PyObject_Length(obj);
            if (len < 0) {
                if (!PyErr_ExceptionMatches(PyExc_TypeError)) {
                    // propagate MemoryError / KeyboardInterrupt / etc.
                    return -1;
                }
                PyErr_Clear();
                return 0;
            }
            if (len != 2) {
                return 0;
            }
            key = PySequence_GetItem(obj, 0);
            if (key == NULL) {
                return -1;
            }
            value = PySequence_GetItem(obj, 1);
            if (value == NULL) {
                Py_DECREF(key);  // key is owned here; do not leak it
                return -1;
            }
            break;
    }

    identity = md_calc_identity(self->md, key, self->md->is_ci);
    if (identity == NULL) {
        if (!PyErr_ExceptionMatches(PyExc_TypeError)) {
            ret = -1;  // propagate MemoryError / KeyboardInterrupt / etc.
        } else {
            PyErr_Clear();
        }
    } else {
        ret = _itemsview_has(self->md, identity, value);
        Py_DECREF(identity);
    }
    Py_DECREF(key);
    Py_DECREF(value);
    ASSERT_CONSISTENT(self->md);
    return ret;
}

NOINLINE static int
multidict_itemsview_sq_contains(_Multidict_ViewObject* self, PyObject* obj)
{
    int ret;
    Py_BEGIN_CRITICAL_SECTION(self->md);
    ret = _multidict_itemsview_contains_impl(self, obj);
    Py_END_CRITICAL_SECTION();
    return ret;
}

static inline PyObject*
_multidict_itemsview_isdisjoint_impl(_Multidict_ViewObject* self,
                                     PyObject* other)
{
    PyObject* iter = PyObject_GetIter(other);
    if (iter == NULL) {
        return NULL;
    }
    PyObject* arg = NULL;
    PyObject* identity = NULL;
    PyObject* value = NULL;
    int st;
    int found = 0;

    while (found == 0 && (st = PyIter_NextItem(iter, &arg)) > 0) {
        found = _multidict_itemsview_parse_item(
            self, arg, &identity, NULL, &value);
        if (found > 0) {
            found = _itemsview_has(self->md, identity, value);
            Py_CLEAR(identity);
            Py_CLEAR(value);
        }
        Py_DECREF(arg);
    }
    Py_DECREF(iter);
    ASSERT_CONSISTENT(self->md);
    if (found < 0 || st < 0) {
        return NULL;
    }
    return PyBool_FromLong(found == 0);
}

COLD static PyObject*
multidict_itemsview_isdisjoint(_Multidict_ViewObject* self, PyObject* other)
{
    PyObject* ret;
    Py_BEGIN_CRITICAL_SECTION(self->md);
    ret = _multidict_itemsview_isdisjoint_impl(self, other);
    Py_END_CRITICAL_SECTION();
    return ret;
}

PyDoc_STRVAR(itemsview_isdisjoint_doc,
             "Return True if two sets have a null intersection.");

NOINLINE static PyObject*
multidict_itemsview_reversed(_Multidict_ViewObject* self,
                             PyObject* Py_UNUSED(ignored))
{
    return multidict_items_iter_new(self->md, 1);
}

PyDoc_STRVAR(view_reversed_doc, "Return a reverse iterator over the view.");

static PyMethodDef multidict_itemsview_methods[] = {
    {"isdisjoint",
     (PyCFunction)multidict_itemsview_isdisjoint,
     METH_O,
     itemsview_isdisjoint_doc},
    {"__reversed__",
     (PyCFunction)multidict_itemsview_reversed,
     METH_NOARGS,
     view_reversed_doc},
    {NULL, NULL} /* sentinel */
};

static PyType_Slot multidict_itemsview_slots[] = {
    {Py_tp_dealloc, multidict_view_tp_dealloc},
    {Py_tp_repr, multidict_itemsview_tp_repr},

    {Py_nb_subtract, multidict_itemsview_nb_subtract},
    {Py_nb_and, multidict_itemsview_nb_and},
    {Py_nb_xor, multidict_itemsview_xor},
    {Py_nb_or, multidict_itemsview_nb_or},
    {Py_sq_length, multidict_view_sq_length},
    {Py_sq_contains, multidict_itemsview_sq_contains},
    {Py_tp_getattro, PyObject_GenericGetAttr},
    {Py_tp_traverse, multidict_view_tp_traverse},
    {Py_tp_clear, multidict_view_tp_clear},
    {Py_tp_richcompare, multidict_view_richcompare},
    {Py_tp_iter, multidict_itemsview_tp_iter},
    {Py_tp_methods, multidict_itemsview_methods},
    {0, NULL},
};

static PyType_Spec multidict_itemsview_spec = {
    .name = "multidict._multidict._ItemsView",
    .basicsize = sizeof(_Multidict_ViewObject),
    .flags = (Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE |
              Py_TPFLAGS_DISALLOW_INSTANTIATION | Py_TPFLAGS_HAVE_GC),
    .slots = multidict_itemsview_slots,
};

/********** Keys **********/

static inline PyObject*
multidict_keysview_new(MultiDictObject* md)
{
    return _multidict_view_new(md, md->state->KeysViewType);
}

NOINLINE static PyObject*
multidict_keysview_tp_iter(_Multidict_ViewObject* self)
{
    return multidict_keys_iter_new(self->md, 0);
}

NOINLINE static PyObject*
multidict_keysview_tp_repr(_Multidict_ViewObject* self)
{
    PyObject* ret;
    Py_BEGIN_CRITICAL_SECTION(self->md);
    ret = md_repr(self->md, (PyObject*)self, true, false);
    Py_END_CRITICAL_SECTION();
    return ret;
}

// Out of line: a set operator is not worth a copy of the lookup.
NOINLINE static int
_keysview_has(MultiDictObject* md, PyObject* key)
{
    return md_contains(md, key, md->is_ci);
}

/* `&`, or `-` if `subtract`. `view & other` and `view - other` go by the
   view's own keys, `other & view` and `other - view` by other's. */
COLD static PyObject*
_keysview_filter(_Multidict_ViewObject* self, PyObject* other, bool reflected,
                 bool subtract)
{
    PyObject* key = NULL;
    PyObject* own = NULL;
    PyObject* ret = NULL;
    int st;
    PyObject* iter = PyObject_GetIter(other);
    if (iter == NULL) {
        return _type_error_to_notimplemented();
    }
    if (subtract) {
        ret = PySet_New(reflected ? other : (PyObject*)self);
    } else {
        ret = PySet_New(NULL);
    }
    if (ret == NULL) {
        goto fail;
    }
    while ((st = PyIter_NextItem(iter, &key)) > 0) {
        if (!PyUnicode_Check(key)) {
            Py_DECREF(key);
            continue;
        }
        int found = reflected ? _keysview_has(self->md, key)
                              : md_find_key(self->md, key, &own);
        if (found < 0) {
            goto fail;
        }
        if (found > 0) {
            PyObject* hit = reflected ? key : own;
            int tmp = subtract ? PySet_Discard(ret, hit) : PySet_Add(ret, hit);
            if (tmp < 0) {
                goto fail;
            }
        }
        Py_DECREF(key);
        Py_CLEAR(own);
    }
    if (st < 0) {
        goto fail;
    }
    Py_DECREF(iter);
    return ret;
fail:
    Py_CLEAR(key);
    Py_CLEAR(own);
    Py_CLEAR(iter);
    Py_CLEAR(ret);
    return NULL;
}

static PyObject*
_keysview_and(_Multidict_ViewObject* self, PyObject* other, bool reflected)
{
    return _keysview_filter(self, other, reflected, false);
}

NOINLINE static PyObject*
multidict_keysview_nb_and(PyObject* lft, PyObject* rht)
{
    return _view_binop(lft, rht, false, true, _keysview_and);
}

// view | other
COLD static PyObject*
_keysview_or_lft(_Multidict_ViewObject* self, PyObject* other)
{
    PyObject* key = NULL;
    PyObject* ret = NULL;
    int st;
    PyObject* iter = PyObject_GetIter(other);
    if (iter == NULL) {
        return _type_error_to_notimplemented();
    }
    ret = PySet_New((PyObject*)self);
    if (ret == NULL) {
        goto fail;
    }
    while ((st = PyIter_NextItem(iter, &key)) > 0) {
        // _keysview_has() answers 0 for a key that is not a str
        int tmp = _keysview_has(self->md, key);
        if (tmp < 0) {
            goto fail;
        }
        if (tmp == 0) {
            if (PySet_Add(ret, key) < 0) {
                goto fail;
            }
        }
        Py_DECREF(key);
    }
    if (st < 0) {
        goto fail;
    }
    Py_DECREF(iter);
    return ret;
fail:
    Py_CLEAR(key);
    Py_CLEAR(iter);
    Py_CLEAR(ret);
    return NULL;
}

// other | view
COLD static PyObject*
_keysview_or_rht(_Multidict_ViewObject* self, PyObject* other)
{
    PyObject* iter = NULL;
    PyObject* identity = NULL;
    PyObject* key = NULL;
    PyObject* tmp_set = NULL;
    int st;
    PyObject* ret = PySet_New(other);
    if (ret == NULL) {
        if (PyErr_ExceptionMatches(PyExc_TypeError)) {
            PyErr_Clear();
            Py_RETURN_NOTIMPLEMENTED;
        }
        goto fail;
    }
    iter = PyObject_GetIter(ret);
    if (iter == NULL) {
        goto fail;
    }
    tmp_set = PySet_New(NULL);
    if (tmp_set == NULL) {
        goto fail;
    }
    while ((st = PyIter_NextItem(iter, &key)) > 0) {
        if (!PyUnicode_Check(key)) {
            Py_DECREF(key);
            continue;
        }
        identity = md_calc_identity(self->md, key, self->md->is_ci);
        if (identity == NULL) {
            goto fail;
        }
        if (PySet_Add(tmp_set, identity) < 0) {
            goto fail;
        }
        Py_CLEAR(identity);
        Py_DECREF(key);
    }
    if (st < 0) {
        goto fail;
    }
    Py_CLEAR(iter);

    MultiDictObject* md = self->md;
    uint64_t version = md->version;
    htkeys_t* keys = md->keys;
    entry_t* entries = htkeys_entries(keys);
    uint8_t kind = keys->kind;

    for (Py_ssize_t pos = 0; pos < keys->nentries; ++pos) {
        entry_t* entry = entry_at(kind, entries, pos);
        if (entry_is_hole(entry)) {
            continue;
        }
        identity = Py_NewRef(entry_identity(kind, md->is_ci, entry));
        key = md_ensure_key(md, entry);  // last entry access
        if (key == NULL) {
            goto fail;
        }
        int tmp = PySet_Contains(tmp_set, identity);
        if (tmp < 0) {
            goto fail;
        }
        if (tmp == 0) {
            if (PySet_Add(ret, key) < 0) {
                goto fail;
            }
        }
        Py_CLEAR(identity);
        Py_CLEAR(key);
        // See _itemsview_unmatched().
        if (md_check_version(md, version) < 0) {
            goto fail;
        }
    }
    Py_DECREF(tmp_set);
    return ret;
fail:
    Py_CLEAR(identity);
    Py_CLEAR(key);
    Py_CLEAR(iter);
    Py_CLEAR(ret);
    Py_CLEAR(tmp_set);
    return NULL;
}

static PyObject*
_keysview_or(_Multidict_ViewObject* self, PyObject* other, bool reflected)
{
    if (reflected) {
        return _keysview_or_rht(self, other);
    }
    return _keysview_or_lft(self, other);
}

NOINLINE static PyObject*
multidict_keysview_nb_or(PyObject* lft, PyObject* rht)
{
    return _view_binop(lft, rht, false, true, _keysview_or);
}

static PyObject*
_keysview_sub(_Multidict_ViewObject* self, PyObject* other, bool reflected)
{
    return _keysview_filter(self, other, reflected, true);
}

NOINLINE static PyObject*
multidict_keysview_nb_subtract(PyObject* lft, PyObject* rht)
{
    return _view_binop(lft, rht, false, true, _keysview_sub);
}

NOINLINE static PyObject*
multidict_keysview_xor(PyObject* lft, PyObject* rht)
{
    return _view_binop(lft, rht, false, false, _view_xor);
}

NOINLINE static int
multidict_keysview_sq_contains(_Multidict_ViewObject* self, PyObject* key)
{
    return md_contains(self->md, key, self->md->is_ci);
}

static inline PyObject*
_multidict_keysview_isdisjoint_impl(_Multidict_ViewObject* self,
                                    PyObject* other)
{
    PyObject* iter = PyObject_GetIter(other);
    if (iter == NULL) {
        return NULL;
    }
    PyObject* key = NULL;
    int st;
    while ((st = PyIter_NextItem(iter, &key)) > 0) {
        int tmp = md_contains(self->md, key, self->md->is_ci);
        Py_DECREF(key);
        if (tmp < 0) {
            Py_CLEAR(iter);
            return NULL;
        }
        if (tmp > 0) {
            Py_DECREF(iter);
            Py_RETURN_FALSE;
        }
    }
    Py_DECREF(iter);
    if (st < 0) {
        return NULL;
    }
    Py_RETURN_TRUE;
}

NOINLINE static PyObject*
multidict_keysview_isdisjoint(_Multidict_ViewObject* self, PyObject* other)
{
    PyObject* ret;
    Py_BEGIN_CRITICAL_SECTION(self->md);
    ret = _multidict_keysview_isdisjoint_impl(self, other);
    Py_END_CRITICAL_SECTION();
    return ret;
}

PyDoc_STRVAR(keysview_isdisjoint_doc,
             "Return True if two sets have a null intersection.");

NOINLINE static PyObject*
multidict_keysview_reversed(_Multidict_ViewObject* self,
                            PyObject* Py_UNUSED(ignored))
{
    return multidict_keys_iter_new(self->md, 1);
}

static PyMethodDef multidict_keysview_methods[] = {
    {"isdisjoint",
     (PyCFunction)multidict_keysview_isdisjoint,
     METH_O,
     keysview_isdisjoint_doc},
    {"__reversed__",
     (PyCFunction)multidict_keysview_reversed,
     METH_NOARGS,
     view_reversed_doc},
    {NULL, NULL} /* sentinel */
};

static PyType_Slot multidict_keysview_slots[] = {
    {Py_tp_dealloc, multidict_view_tp_dealloc},
    {Py_tp_repr, multidict_keysview_tp_repr},

    {Py_nb_subtract, multidict_keysview_nb_subtract},
    {Py_nb_and, multidict_keysview_nb_and},
    {Py_nb_xor, multidict_keysview_xor},
    {Py_nb_or, multidict_keysview_nb_or},
    {Py_sq_length, multidict_view_sq_length},
    {Py_sq_contains, multidict_keysview_sq_contains},
    {Py_tp_getattro, PyObject_GenericGetAttr},
    {Py_tp_traverse, multidict_view_tp_traverse},
    {Py_tp_clear, multidict_view_tp_clear},
    {Py_tp_richcompare, multidict_view_richcompare},
    {Py_tp_iter, multidict_keysview_tp_iter},
    {Py_tp_methods, multidict_keysview_methods},
    {0, NULL},
};

static PyType_Spec multidict_keysview_spec = {
    .name = "multidict._multidict._KeysView",
    .basicsize = sizeof(_Multidict_ViewObject),
    .flags = (Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE |
              Py_TPFLAGS_DISALLOW_INSTANTIATION | Py_TPFLAGS_HAVE_GC),
    .slots = multidict_keysview_slots,
};

/********** Values **********/

static inline PyObject*
multidict_valuesview_new(MultiDictObject* md)
{
    return _multidict_view_new(md, md->state->ValuesViewType);
}

NOINLINE static PyObject*
multidict_valuesview_tp_iter(_Multidict_ViewObject* self)
{
    return multidict_values_iter_new(self->md, 0);
}

NOINLINE static PyObject*
multidict_valuesview_tp_repr(_Multidict_ViewObject* self)
{
    PyObject* ret;
    Py_BEGIN_CRITICAL_SECTION(self->md);
    ret = md_repr(self->md, (PyObject*)self, false, true);
    Py_END_CRITICAL_SECTION();
    return ret;
}

NOINLINE static PyObject*
multidict_valuesview_reversed(_Multidict_ViewObject* self,
                              PyObject* Py_UNUSED(ignored))
{
    return multidict_values_iter_new(self->md, 1);
}

static PyMethodDef multidict_valuesview_methods[] = {
    {"__reversed__",
     (PyCFunction)multidict_valuesview_reversed,
     METH_NOARGS,
     view_reversed_doc},
    {NULL, NULL} /* sentinel */
};

static PyType_Slot multidict_valuesview_slots[] = {
    {Py_tp_dealloc, multidict_view_tp_dealloc},
    {Py_tp_repr, multidict_valuesview_tp_repr},

    {Py_sq_length, multidict_view_sq_length},
    {Py_tp_getattro, PyObject_GenericGetAttr},
    {Py_tp_traverse, multidict_view_tp_traverse},
    {Py_tp_clear, multidict_view_tp_clear},
    {Py_tp_iter, multidict_valuesview_tp_iter},
    {Py_tp_methods, multidict_valuesview_methods},
    {0, NULL},
};

static PyType_Spec multidict_valuesview_spec = {
    .name = "multidict._multidict._ValuesView",
    .basicsize = sizeof(_Multidict_ViewObject),
    .flags = (Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE |
              Py_TPFLAGS_DISALLOW_INSTANTIATION | Py_TPFLAGS_HAVE_GC),
    .slots = multidict_valuesview_slots,
};

NOINLINE static int
multidict_views_init(PyObject* module, mod_state* state)
{
    PyObject* tmp;
    tmp = PyType_FromModuleAndSpec(module, &multidict_itemsview_spec, NULL);
    if (tmp == NULL) {
        return -1;
    }
    state->ItemsViewType = (PyTypeObject*)tmp;

    tmp = PyType_FromModuleAndSpec(module, &multidict_valuesview_spec, NULL);
    if (tmp == NULL) {
        return -1;
    }
    state->ValuesViewType = (PyTypeObject*)tmp;

    tmp = PyType_FromModuleAndSpec(module, &multidict_keysview_spec, NULL);
    if (tmp == NULL) {
        return -1;
    }
    state->KeysViewType = (PyTypeObject*)tmp;

    return 0;
}

#ifdef __cplusplus
}
#endif
#endif
