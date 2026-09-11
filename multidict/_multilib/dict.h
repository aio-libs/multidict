#ifndef _MULTIDICT_C_H
#define _MULTIDICT_C_H

#ifdef __cplusplus
extern "C" {
#endif

#include "htkeys.h"
#include "pythoncapi_compat.h"
#include "state.h"

#if PY_VERSION_HEX >= 0x030c00f0
#define MANAGED_WEAKREFS
#endif

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

#ifdef Py_GIL_DISABLED
    Py_ssize_t active_readers;

    /* Singly-linked list (via htkeys_t.retired_next) of tables retired
       by a resize/shrink/clear while active_readers was nonzero.
       Writer-only (always under this object's critical section), so a
       plain pointer, not atomic. Drained opportunistically at the
       start of the next resize/shrink/reserve/clear. */
    htkeys_t* retired;
#endif
} MultiDictObject;

typedef struct {
    PyObject_HEAD
#ifndef MANAGED_WEAKREFS
    PyObject* weaklist;
#endif
    MultiDictObject* md;
} MultiDictProxyObject;

#ifdef __cplusplus
}
#endif

#endif
