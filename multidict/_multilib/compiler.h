#ifndef _MULTIDICT_COMPILER_H
#define _MULTIDICT_COMPILER_H

/* Py_NO_INLINE is 3.11+ */
#if defined(__GNUC__) || defined(__clang__)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define COLD __attribute__((cold, noinline))
#define ALWAYS_INLINE __attribute__((always_inline))
#define NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define UNLIKELY(x) (x)
#define COLD __declspec(noinline)
#define ALWAYS_INLINE __forceinline
#define NOINLINE __declspec(noinline)
#else
#define UNLIKELY(x) (x)
#define COLD
#define ALWAYS_INLINE
#define NOINLINE
#endif

/* initial-exec skips the __tls_get_addr() call the default model for a
   shared object makes on every access. Only glibc reserves static TLS
   for a dlopen()ed module to use it. */
#if defined(_MSC_VER)
#define THREAD_LOCAL __declspec(thread)
#elif defined(__GLIBC__)
#define THREAD_LOCAL _Thread_local __attribute__((tls_model("initial-exec")))
#else
#define THREAD_LOCAL _Thread_local
#endif

#endif
