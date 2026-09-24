#ifndef _MULTIDICT_C_H
#define _MULTIDICT_C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

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
#ifndef MANAGED_WEAKREFS
    PyObject* weaklist;
#endif
    mod_state* state;
    Py_ssize_t used;

    uint64_t version;
    bool is_ci;

    htkeys_t* keys;

    md_watch_t* watch;

#ifdef Py_GIL_DISABLED
    Py_ssize_t num_active_readers;

    htkeys_t* retired;
#endif

    /* Strong, and last so the hot fields stay in the first cache line.
       Keeps `state` addressable through teardown; see mod_state.mod. */
    PyObject* mod;
} MultiDictObject;

typedef struct {
    PyObject_HEAD
#ifndef MANAGED_WEAKREFS
    PyObject* weaklist;
#endif
    MultiDictObject* md;
} MultiDictProxyObject;

/* Shells for the exact MultiDict, CIMultiDict and proxy types come from
   a module-state pool. Only those: a subclass has its own basicsize, and
   possibly its own __dict__ and __weakref__ preheader, so a pooled shell
   would be the wrong shape for it.

   MultiDict and CIMultiDict share a pool because they share a struct and
   differ only in md->is_ci, and the two proxy types likewise. */
static inline pool_t*
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
_md_shell_alloc(mod_state* state, PyTypeObject* tp)
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
_md_shell_recycle(mod_state* state, PyObject* obj)
{
    if (state == NULL) {
        return false;
    }
    pool_t* pool = _md_pool_for(state, Py_TYPE(obj));
    return pool != NULL && pool_push(pool, obj);
}

/* Out of line: inlined into a constructor it costs the insert loop more
   than the call, by pushing the compiler off a better layout. */
NOINLINE static void
md_set_module(MultiDictObject* md, PyObject* mod)
{
    md->mod = Py_NewRef(mod);
}

#ifdef __cplusplus
}
#endif

#endif
