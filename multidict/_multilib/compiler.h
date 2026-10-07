#ifndef _MULTIDICT_COMPILER_H
#define _MULTIDICT_COMPILER_H

#include <Python.h>

/* Py_NO_INLINE is 3.11+ */
#if defined(__GNUC__) || defined(__clang__)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define COLD __attribute__((cold, noinline))
#define ALWAYS_INLINE __attribute__((always_inline))
#define NOINLINE __attribute__((noinline))
#define FLATTEN __attribute__((flatten))
#elif defined(_MSC_VER)
#define UNLIKELY(x) (x)
#define COLD __declspec(noinline)
#define ALWAYS_INLINE __forceinline
#define NOINLINE __declspec(noinline)
#define FLATTEN
#else
#define UNLIKELY(x) (x)
#define COLD
#define ALWAYS_INLINE
#define NOINLINE
#define FLATTEN
#endif

/* FLATTEN inlines every call in the function, past GCC's inlining budget,
   which on FT otherwise leaves Py_DECREF() out of line on the hot paths.
   FT_FLATTEN is for functions where it costs GIL time. */
#ifdef Py_GIL_DISABLED
#define FT_FLATTEN FLATTEN
#else
#define FT_FLATTEN
#endif

/* Not initial-exec, the TLS model that reaches a variable at a fixed
   offset from the thread pointer instead of calling __tls_get_addr()
   (https://www.akkadia.org/drepper/tls.pdf, section 4.3). It saves a few
   ns per access, but draws on the small static TLS reserve glibc keeps
   for dlopen(), and once that is used up the import fails and multidict
   silently falls back to pure Python. */
#if defined(_MSC_VER)
#define THREAD_LOCAL __declspec(thread)
#else
#define THREAD_LOCAL _Thread_local
#endif

#endif
