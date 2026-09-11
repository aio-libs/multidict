#ifndef _MULTIDICT_HELPERS_H
#define _MULTIDICT_HELPERS_H

#include <Python.h>

#ifdef __cplusplus
extern "C++" {
#include <atomic>
}
#define _USING_STD using namespace std
#define _Atomic(tp) atomic<tp>
#else
#define _USING_STD
#include <stdatomic.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef Py_GIL_DISABLED

static inline Py_ssize_t
atomic_load_ssize_relaxed(const Py_ssize_t* obj)
{
    _USING_STD;
    return atomic_load_explicit((const _Atomic(Py_ssize_t)*)obj,
                                memory_order_relaxed);
}

#else

static inline Py_ssize_t
atomic_load_ssize_relaxed(const Py_ssize_t* obj)
{
    return *obj;
}

#endif

#ifdef __cplusplus
}
#endif
#endif
