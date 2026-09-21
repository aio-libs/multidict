#ifndef _MULTIDICT_COMPILER_H
#define _MULTIDICT_COMPILER_H

/* Py_NO_INLINE is 3.11+ */
#if defined(__GNUC__) || defined(__clang__)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define COLD __attribute__((cold, noinline))
#define ALWAYS_INLINE __attribute__((always_inline))
#elif defined(_MSC_VER)
#define UNLIKELY(x) (x)
#define COLD __declspec(noinline)
#define ALWAYS_INLINE __forceinline
#else
#define UNLIKELY(x) (x)
#define COLD
#define ALWAYS_INLINE
#endif

#endif
