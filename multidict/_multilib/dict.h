#ifndef _MULTIDICT_C_H
#define _MULTIDICT_C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

#include "compiler.h"
#include "freelist.h"
#include "htkeys.h"
#include "pythoncapi_compat.h"
#include "state.h"

#if PY_VERSION_HEX >= 0x030c00f0
#define MANAGED_WEAKREFS
#endif

/* Defined in watch.h, which needs MultiDictObject itself. NULL on an
   unwatched multidict, which is the only case the mutation paths test. */
typedef struct _md_watch md_watch_t;

typedef struct {
    PyObject_HEAD
    /* The fields a lookup reads come first, then the ones a mutation
       writes, then the cold ones. */
    mod_state* state;
    htkeys_t* keys;
#ifdef Py_GIL_DISABLED
    Py_ssize_t num_active_readers;
#endif
    bool is_ci;
    /* In is_ci's padding: the update() and merge() calls in flight, and
       a count of the tables md got while they were; see update_marks.h. */
    uint16_t batches;
    uint32_t layout_gen;

    Py_ssize_t used;
    uint64_t version;

    md_watch_t* watch;

#ifdef Py_GIL_DISABLED
    htkeys_t* retired;
#endif

#ifndef MANAGED_WEAKREFS
    PyObject* weaklist;
#endif
} MultiDictObject;

typedef struct {
    PyObject_HEAD
    MultiDictObject* md;
#ifndef MANAGED_WEAKREFS
    PyObject* weaklist;
#endif
} MultiDictProxyObject;

/* The multidict a proxy of the given class wraps for arg; NULL without an
   exception if arg is neither such a multidict nor a proxy of one. */
static MultiDictObject*
multidict_proxy_target(mod_state* state, PyObject* arg, bool is_ci)
{
    if (is_ci ? CIMultiDict_Check(state, arg)
              : AnyMultiDict_Check(state, arg)) {
        return (MultiDictObject*)arg;
    }
    if (is_ci ? CIMultiDictProxy_Check(state, arg)
              : AnyMultiDictProxy_Check(state, arg)) {
        return ((MultiDictProxyObject*)arg)->md;
    }
    return NULL;
}

NOINLINE static int
_err_version_changed(void)
{
    PyErr_SetString(PyExc_RuntimeError,
                    "MultiDict is changed during iteration");
    return -1;
}

/* Returns -1 with RuntimeError set if md was mutated, or got a new
   table, since `version` was read from it; 0 otherwise. */
ALWAYS_INLINE static inline int
md_check_version(MultiDictObject* md, uint64_t version)
{
    if (UNLIKELY(version != md->version)) {
        return _err_version_changed();
    }
    return 0;
}

/* Shells for the exact MultiDict, CIMultiDict and proxy types come from
   a module-state pool. Only those: a subclass has its own basicsize, and
   possibly its own __dict__ and __weakref__ preheader, so a pooled shell
   would be the wrong shape for it.

   MultiDict and CIMultiDict share a pool because they share a struct and
   differ only in md->is_ci, and the two proxy types likewise. */
static pool_t*
_md_pool_for(mod_state* state, PyTypeObject* tp)
{
    if (tp == state->MultiDictType || tp == state->CIMultiDictType) {
        return &state->md_pool;
    }
    if (tp == state->MultiDictProxyType || tp == state->CIMultiDictProxyType) {
        return &state->proxy_pool;
    }
    return NULL;
}

/* Out of line for the reason _multidict_view_alloc() gives: inlined,
   these two cost del d[key] its inlined md_calc_identity(). */
NOINLINE static PyObject*
md_shell_alloc(mod_state* state, PyTypeObject* tp)
{
    pool_t* pool = _md_pool_for(state, tp);
    PyObject* obj = pool == NULL ? NULL : pool_pop(pool);
    if (obj == NULL) {
        return tp->tp_alloc(tp, 0);
    }
    /* What PyType_GenericAlloc() does, less the allocation. The
       preheader needs nothing: PyObject_GC_UnTrack() left the GC header
       untracked, and PyObject_ClearWeakRefs() left the managed weakref
       slot NULL. */
    memset(obj, 0, (size_t)tp->tp_basicsize);
    PyObject_Init(obj, tp);
    PyObject_GC_Track(obj);
    return obj;
}

/* True once the shell is parked, false to leave the caller to free it. */
NOINLINE static bool
md_shell_recycle(mod_state* state, PyObject* obj)
{
    if (state == NULL) {
        return false;
    }
    pool_t* pool = _md_pool_for(state, Py_TYPE(obj));
    return pool != NULL && pool_push(pool, obj);
}

/* A MultiDictObject shell with its state set and its strong reference
   to state->mod taken, which keeps `state` addressable through teardown;
   see mod_state.mod. The caller still has to md_init() it. Out of line:
   inlined into a constructor it costs the insert loop more than the
   call, by pushing the compiler off a better layout. */
NOINLINE static MultiDictObject*
md_shell_new(mod_state* state, PyTypeObject* tp)
{
    MultiDictObject* md = (MultiDictObject*)md_shell_alloc(state, tp);
    if (md == NULL) {
        return NULL;
    }
    md->state = state;
    Py_INCREF(state->mod);
    return md;
}

#ifdef __cplusplus
}
#endif

#endif
