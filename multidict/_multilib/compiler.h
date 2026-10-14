#ifndef _MULTIDICT_COMPILER_H
#define _MULTIDICT_COMPILER_H

#if defined(__GNUC__) || defined(__clang__)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define COLD __attribute__((cold, noinline))
#define NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define UNLIKELY(x) (x)
#define COLD __declspec(noinline)
#define NOINLINE __declspec(noinline)
#else
#define UNLIKELY(x) (x)
#define COLD
#define NOINLINE
#endif

/* Seen by every unit of the extension, and no further: _multidict.c and
   _multidict_general.c are linked together. */
#if defined(__GNUC__) || defined(__clang__)
#define MD_HIDDEN __attribute__((visibility("hidden")))
#else
#define MD_HIDDEN
#endif

/* A function of _multidict_general.c, called from _multidict.c. */
#define MD_GENERAL_API COLD MD_HIDDEN

/* A variable both units share: defined in _multidict.c, declared in
   _multidict_general.c, which defines MULTIDICT_GENERAL_UNIT. */
#ifdef MULTIDICT_GENERAL_UNIT
#define MD_SHARED extern MD_HIDDEN
#else
#define MD_SHARED MD_HIDDEN
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
