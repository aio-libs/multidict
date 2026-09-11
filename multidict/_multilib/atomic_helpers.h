#ifndef _MULTIDICT_ATOMIC_HELPERS_H
#define _MULTIDICT_ATOMIC_HELPERS_H

#include <Python.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef Py_GIL_DISABLED

/* Atomic load backend selection, mirroring CPython's own
   Include/cpython/pyatomic.h: prefer GCC/Clang builtins, fall back to
   C11 stdatomic.h, and use MSVC intrinsics only when neither is
   available (plain cl.exe, which supports neither). */

#ifndef _MULTIDICT_USE_GCC_BUILTIN_ATOMICS
#if defined(__GNUC__) && \
    (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8))
#define _MULTIDICT_USE_GCC_BUILTIN_ATOMICS 1
#elif defined(__clang__) && __has_builtin(__atomic_load)
#define _MULTIDICT_USE_GCC_BUILTIN_ATOMICS 1
#else
#define _MULTIDICT_USE_GCC_BUILTIN_ATOMICS 0
#endif
#endif

#if _MULTIDICT_USE_GCC_BUILTIN_ATOMICS

static inline Py_ssize_t
atomic_load_ssize_relaxed(const Py_ssize_t* obj)
{
    return __atomic_load_n(obj, __ATOMIC_RELAXED);
}

#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && \
    !defined(__STDC_NO_ATOMICS__)

#include <stdatomic.h>

static inline Py_ssize_t
atomic_load_ssize_relaxed(const Py_ssize_t* obj)
{
    return atomic_load_explicit((const _Atomic(Py_ssize_t)*)obj,
                                memory_order_relaxed);
}

#elif defined(_MSC_VER)

/* MSVC has no __atomic_* builtins and (at least on the toolset multidict
   targets) no usable <stdatomic.h>. A plain volatile read is enough for
   relaxed semantics: on x86/x86_64 volatile accesses already have
   acquire-release ordering, and on ARM64 MSVC treats them as
   memory_order_relaxed -- exactly what's needed here. See the comment
   at the top of CPython's Include/cpython/pyatomic_msc.h. */

static inline Py_ssize_t
atomic_load_ssize_relaxed(const Py_ssize_t* obj)
{
    return *(volatile const Py_ssize_t*)obj;
}

#else
#error "no available atomic load implementation for this platform/compiler"
#endif

#else /* Py_GIL_DISABLED */

static inline Py_ssize_t
atomic_load_ssize_relaxed(const Py_ssize_t* obj)
{
    return *obj;
}

#endif /* Py_GIL_DISABLED */

#ifdef __cplusplus
}
#endif
#endif
