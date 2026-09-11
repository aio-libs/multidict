#ifndef _MULTIDICT_ATOMIC_HELPERS_H
#define _MULTIDICT_ATOMIC_HELPERS_H

#include <Python.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef Py_GIL_DISABLED

/* Atomic backend selection, mirroring CPython's own
   Include/cpython/pyatomic.h: prefer GCC/Clang builtins, fall back to
   C11 stdatomic.h, and use MSVC intrinsics only when neither is
   available (plain cl.exe, which supports neither).

   Only the operations the lock-free read path actually needs are
   provided: relaxed load/fetch-add for advisory, non-load-bearing
   counters (htkeys_t.readers), and seq_cst load/store/fetch-add for
   the md->keys <-> active_readers pair, where anything weaker than a
   full fence leaves a real (if narrow, architecture-dependent) race --
   see the reasoning in md_reader_enter()/md_reader_exit()/md_retire()
   in hashtable.h. */

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

static inline Py_ssize_t
atomic_load_ssize(const Py_ssize_t* obj)
{
    return __atomic_load_n(obj, __ATOMIC_SEQ_CST);
}

static inline Py_ssize_t
atomic_fetch_add_ssize_relaxed(Py_ssize_t* obj, Py_ssize_t value)
{
    return __atomic_fetch_add(obj, value, __ATOMIC_RELAXED);
}

static inline Py_ssize_t
atomic_fetch_add_ssize(Py_ssize_t* obj, Py_ssize_t value)
{
    return __atomic_fetch_add(obj, value, __ATOMIC_SEQ_CST);
}

static inline void*
atomic_load_ptr(void* const* obj)
{
    return __atomic_load_n(obj, __ATOMIC_SEQ_CST);
}

static inline void
atomic_store_ptr(void** obj, void* value)
{
    __atomic_store_n(obj, value, __ATOMIC_SEQ_CST);
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

static inline Py_ssize_t
atomic_load_ssize(const Py_ssize_t* obj)
{
    return atomic_load_explicit((const _Atomic(Py_ssize_t)*)obj,
                                memory_order_seq_cst);
}

static inline Py_ssize_t
atomic_fetch_add_ssize_relaxed(Py_ssize_t* obj, Py_ssize_t value)
{
    return atomic_fetch_add_explicit(
        (_Atomic(Py_ssize_t)*)obj, value, memory_order_relaxed);
}

static inline Py_ssize_t
atomic_fetch_add_ssize(Py_ssize_t* obj, Py_ssize_t value)
{
    return atomic_fetch_add_explicit(
        (_Atomic(Py_ssize_t)*)obj, value, memory_order_seq_cst);
}

static inline void*
atomic_load_ptr(void* const* obj)
{
    return atomic_load_explicit((void* const _Atomic*)obj,
                                memory_order_seq_cst);
}

static inline void
atomic_store_ptr(void** obj, void* value)
{
    atomic_store_explicit((void* _Atomic*)obj, value, memory_order_seq_cst);
}

#elif defined(_MSC_VER)

/* MSVC has no __atomic_* builtins and (at least on the toolset multidict
   targets) no usable <stdatomic.h>. A plain volatile read is enough for
   relaxed semantics: on x86/x86_64 volatile accesses already have
   acquire-release ordering, and on ARM64 MSVC treats them as
   memory_order_relaxed -- exactly what's needed here. See the comment
   at the top of CPython's Include/cpython/pyatomic_msc.h.

   A seq_cst *load* is the same plain volatile read: x86/x86_64's TSO
   already orders a load after any earlier store on the same thread,
   and ARM64 MSVC volatile loads carry load-acquire semantics, which is
   enough once the writer side is properly fenced. A seq_cst *store*
   is NOT just a volatile write, though: TSO explicitly allows a store
   to be reordered after a later, independent load (StoreLoad
   reordering) -- exactly the reordering this module's Dekker-style
   md->keys / active_readers pair depends on being absent. CPython's
   own _Py_atomic_store_ptr() avoids this the same way: route the
   store through _InterlockedExchange*, which carries a full fence on
   every architecture MSVC targets. */

#include <intrin.h>

static inline Py_ssize_t
atomic_load_ssize_relaxed(const Py_ssize_t* obj)
{
    return *(volatile const Py_ssize_t*)obj;
}

static inline Py_ssize_t
atomic_load_ssize(const Py_ssize_t* obj)
{
    return *(volatile const Py_ssize_t*)obj;
}

static inline Py_ssize_t
atomic_fetch_add_ssize_relaxed(Py_ssize_t* obj, Py_ssize_t value)
{
#if SIZEOF_VOID_P == 8
    return (Py_ssize_t)_InterlockedExchangeAdd64((volatile __int64*)obj,
                                                 (__int64)value);
#else
    return (Py_ssize_t)_InterlockedExchangeAdd((volatile long*)obj,
                                               (long)value);
#endif
}

static inline Py_ssize_t
atomic_fetch_add_ssize(Py_ssize_t* obj, Py_ssize_t value)
{
    /* _InterlockedExchangeAdd* already carries a full fence. */
    return atomic_fetch_add_ssize_relaxed(obj, value);
}

static inline void*
atomic_load_ptr(void* const* obj)
{
    return *(void* const volatile*)obj;
}

static inline void
atomic_store_ptr(void** obj, void* value)
{
    (void)_InterlockedExchangePointer((void* volatile*)obj, value);
}

#else
#error "no available atomic implementation for this platform/compiler"
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
