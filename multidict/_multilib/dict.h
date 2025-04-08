#ifndef _MULTIDICT_C_H
#define _MULTIDICT_C_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pythoncapi_compat.h"
#include "htkeys.h"
#include "state.h"

#if PY_VERSION_HEX >= 0x030c00f0
#define MANAGED_WEAKREFS
#endif


typedef struct {
    PyObject_HEAD
#ifndef MANAGED_WEAKREFS
    PyObject *weaklist;
#endif
    mod_state *state;
    Py_ssize_t used;

    uint64_t version;
    unsigned int flags;

    htkeys_t * keys;
} MultiDictObject;

typedef struct {
    PyObject_HEAD
#ifndef MANAGED_WEAKREFS
    PyObject *weaklist;
#endif
    MultiDictObject *md;
} MultiDictProxyObject;


#define IS_CI      (1)
#define IS_TRACKED (2)


static inline bool md_is_ci(MultiDictObject *md)
{
    return md->flags & IS_CI;
}

static inline bool md_set_ci(MultiDictObject *md, bool set)
{
    if (set) {
        md->flags |= IS_CI;
    } else {
        md->flags &= ~IS_CI;
    }
}

static inline bool md_is_tracked(MultiDictObject *md)
{
    return md->flags & IS_TRACKED;
}

static inline bool md_set_tracked(MultiDictObject *md, bool set)
{
    if (set) {
        md->flags |= IS_TRACKED;
        PyObject_GC_Track(md);
    } else {
        md->flags &= ~IS_TRACKED;
        PyObject_GC_UnTrack(md);
    }
}


#ifdef __cplusplus
}
#endif

#endif
