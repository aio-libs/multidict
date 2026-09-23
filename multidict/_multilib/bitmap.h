#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_BITMAP_H
#define _MULTIDICT_BITMAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "compiler.h"
#include "htkeys.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

/* A set of entry indices, private to one walk over a table.

   Marks live here rather than in the table itself, so a concurrent
   reader never sees them. Indices are only meaningful against the
   table the bitmap was built for (`keys`); a resize that moves entries
   must rebuild the bitmap.

   Words are zeroed lazily: `summary` has one bit per word of `words`,
   and a word is only read once its summary bit says it was zeroed.
   Nothing is zeroed or allocated until the first mark, and then only
   the summary (one bit per word). That keeps a walk that marks little from
   paying for the whole table.

   `summary == NULL` means no storage yet: either nothing was marked
   (reads as empty), or the bitmap was released. */

/* Words are register-sized: 64-bit arithmetic on a 32-bit target is
   emulated with register pairs. */
typedef size_t bitmap_word_t;
#define BITMAP_WORD_BITS (SIZEOF_SIZE_T * 8)
#if SIZEOF_SIZE_T == 8
#define BITMAP_WORD_SHIFT 6
#else
#define BITMAP_WORD_SHIFT 5
#endif
#define BITMAP_WORD_MASK (BITMAP_WORD_BITS - 1)
#define BITMAP_ONE ((bitmap_word_t)1)
#define BITMAP_ALL (~(bitmap_word_t)0)

/* The inline buffer sits in the caller's frame: once in md_walk(), which
   is always inlined, so it lands in getall(), the views' match collector
   and the C API's foreach-key; once in md_to_dict(); and twice over
   inside update_marks_t, on every update(). Past about 1 KB it costs
   those callers more in the inlining it crowds out than it saves in
   allocations: dropping it from 4096 takes 2.7% off a getall() that
   hits, 1.4% off one over a 16384-entry table, and nothing off anything
   else. */
#define BITMAP_INLINE_BYTES 1024
#define BITMAP_DENSE_WORDS 64
#define BITMAP_INLINE_WORDS (BITMAP_INLINE_BYTES / sizeof(bitmap_word_t))
#define BITMAP_INLINE_BITS \
    ((Py_ssize_t)(BITMAP_INLINE_WORDS * BITMAP_WORD_BITS))
#define BITMAP_INLINE_SUMMARY (BITMAP_INLINE_WORDS / BITMAP_WORD_BITS)

typedef struct _bitmap {
    bitmap_word_t* words;
    bitmap_word_t* summary;
    Py_ssize_t nwords;
    htkeys_t* keys;
    bool dense;
    bitmap_word_t inline_summary[BITMAP_INLINE_SUMMARY];
    bitmap_word_t inline_words[BITMAP_INLINE_WORDS];
} bitmap_t;

static inline Py_ssize_t
_bitmap_nwords(Py_ssize_t nbits)
{
    return (nbits + BITMAP_WORD_MASK) >> BITMAP_WORD_SHIFT;
}

/* Can't fail and touches no storage: the first mark does that. */
static inline void
bitmap_init(bitmap_t* bm, htkeys_t* keys, Py_ssize_t nbits)
{
    bm->nwords = _bitmap_nwords(nbits);
    bm->keys = keys;
    bm->words = NULL;
    bm->summary = NULL;
    bm->dense = false;
}

COLD static int
_bitmap_alloc(bitmap_t* bm, Py_ssize_t nsummary)
{
    bitmap_word_t* block =
        PyMem_Malloc((size_t)(nsummary + bm->nwords) * sizeof(bitmap_word_t));
    if (block == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    bm->summary = block;
    bm->words = block + nsummary;
    return 0;
}

COLD static int
_bitmap_start(bitmap_t* bm)
{
    Py_ssize_t nsummary = _bitmap_nwords(bm->nwords);
    if (bm->nwords <= BITMAP_DENSE_WORDS) {
        /* Small enough to zero outright, which spares every later access
           the summary lookup. */
        bm->dense = true;
        bm->summary = bm->inline_summary;
        bm->words = bm->inline_words;
        memset(bm->words, 0, (size_t)bm->nwords * sizeof(bitmap_word_t));
        return 0;
    }
    if ((size_t)bm->nwords <= BITMAP_INLINE_WORDS) {
        bm->summary = bm->inline_summary;
        bm->words = bm->inline_words;
    } else if (_bitmap_alloc(bm, nsummary) < 0) {
        return -1;
    }
    memset(bm->summary, 0, (size_t)nsummary * sizeof(bitmap_word_t));
    return 0;
}

/* Sets up storage now, so that no later mark on this bitmap can fail. */
static inline int
bitmap_reserve(bitmap_t* bm)
{
    if (bm->summary != NULL) {
        return 0;
    }
    return _bitmap_start(bm);
}

/* Safe to call more than once, and on a bitmap whose init never ran,
   provided `summary` was set to NULL up front. */
static inline void
bitmap_release(bitmap_t* bm)
{
    if (bm->summary != NULL && bm->summary != bm->inline_summary) {
        PyMem_Free(bm->summary);
    }
    bm->summary = NULL;
    bm->words = NULL;
}

ALWAYS_INLINE static inline bool
_bitmap_word_ready(const bitmap_t* bm, Py_ssize_t wi)
{
    return bm->dense ||
           ((bm->summary[wi >> BITMAP_WORD_SHIFT] >> (wi & BITMAP_WORD_MASK)) &
            1);
}

ALWAYS_INLINE static inline bool
_bitmap_dense_test(const bitmap_t* bm, Py_ssize_t i)
{
    return (bm->words[i >> BITMAP_WORD_SHIFT] >> (i & BITMAP_WORD_MASK)) & 1;
}

/* The word holding `i`, zeroed first if nothing has touched it yet, or
   NULL with an exception set if heap storage couldn't be allocated. */
ALWAYS_INLINE static inline bitmap_word_t*
_bitmap_word(bitmap_t* bm, Py_ssize_t i)
{
    assert(i >= 0 && i < bm->nwords * BITMAP_WORD_BITS);
    Py_ssize_t wi = i >> BITMAP_WORD_SHIFT;
    if (bm->dense) {
        return bm->words + wi;
    }
    if (UNLIKELY(bm->summary == NULL)) {
        if (_bitmap_start(bm) < 0) {
            return NULL;
        }
        if (bm->dense) {
            return bm->words + wi;
        }
    }
    bitmap_word_t* s = bm->summary + (wi >> BITMAP_WORD_SHIFT);
    bitmap_word_t sbit = BITMAP_ONE << (wi & BITMAP_WORD_MASK);
    if (!(*s & sbit)) {
        *s |= sbit;
        bm->words[wi] = 0;
    }
    return bm->words + wi;
}

/* Moves `src` into `dst`, releasing what `dst` held. `src` is left
   released. */
static inline void
bitmap_move(bitmap_t* dst, bitmap_t* src)
{
    bitmap_release(dst);
    dst->nwords = src->nwords;
    dst->keys = src->keys;
    dst->dense = src->dense;
    if (src->summary == src->inline_summary) {
        dst->words = dst->inline_words;
        dst->summary = dst->inline_summary;
        if (src->dense) {
            memcpy(dst->words,
                   src->words,
                   (size_t)src->nwords * sizeof(bitmap_word_t));
            src->words = NULL;
            src->summary = NULL;
            return;
        }
        memcpy(dst->summary,
               src->summary,
               (size_t)_bitmap_nwords(src->nwords) * sizeof(bitmap_word_t));
        for (Py_ssize_t wi = 0; wi < src->nwords; wi++) {
            if (_bitmap_word_ready(src, wi)) {
                dst->words[wi] = src->words[wi];
            }
        }
    } else {
        dst->words = src->words;
        dst->summary = src->summary;
    }
    src->words = NULL;
    src->summary = NULL;
}

ALWAYS_INLINE static inline bool
bitmap_test(const bitmap_t* bm, Py_ssize_t i)
{
    assert(i >= 0 && i < bm->nwords * BITMAP_WORD_BITS);
    if (bm->dense) {
        return _bitmap_dense_test(bm, i);
    }
    if (bm->summary == NULL) {
        return false;
    }
    return _bitmap_word_ready(bm, i >> BITMAP_WORD_SHIFT) &&
           _bitmap_dense_test(bm, i);
}

ALWAYS_INLINE static inline int
bitmap_set(bitmap_t* bm, Py_ssize_t i)
{
    bitmap_word_t* word = _bitmap_word(bm, i);
    if (word == NULL) {
        return -1;
    }
    *word |= BITMAP_ONE << (i & BITMAP_WORD_MASK);
    return 0;
}

ALWAYS_INLINE static inline void
bitmap_clear(bitmap_t* bm, Py_ssize_t i)
{
    assert(i >= 0 && i < bm->nwords * BITMAP_WORD_BITS);
    if (bm->summary == NULL) {
        return;
    }
    Py_ssize_t wi = i >> BITMAP_WORD_SHIFT;
    if (_bitmap_word_ready(bm, wi)) {
        bm->words[wi] &= ~(BITMAP_ONE << (i & BITMAP_WORD_MASK));
    }
}

/* 1 if `i` was already set, 0 if it wasn't (it is now), -1 on error. */
ALWAYS_INLINE static inline int
bitmap_test_and_set(bitmap_t* bm, Py_ssize_t i)
{
    bitmap_word_t* word = _bitmap_word(bm, i);
    if (word == NULL) {
        return -1;
    }
    bitmap_word_t bit = BITMAP_ONE << (i & BITMAP_WORD_MASK);
    int was_set = (*word & bit) != 0;
    *word |= bit;
    return was_set;
}

static inline int
_bitmap_ctz(bitmap_word_t w)
{
    assert(w != 0);
#if SIZEOF_LONG == SIZEOF_SIZE_T
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ctzl((unsigned long)w);
#elif defined(_MSC_VER)
    unsigned long idx;
    _BitScanForward(&idx, (unsigned long)w);
    return (int)idx;
#else
    int n = 0;
    while (!(w & 1)) {
        w >>= 1;
        n++;
    }
    return n;
#endif
#elif defined(_MSC_VER)
    // On 64bit Windows, sizeof(long) == 4.
    unsigned long idx;
    _BitScanForward64(&idx, (uint64_t)w);
    return (int)idx;
#else
    return __builtin_ctzll((unsigned long long)w);
#endif
}

/* The first set index at or after `start`, or -1 if there is none.
   Untouched words are skipped a whole summary word at a time. */
static inline Py_ssize_t
bitmap_next(const bitmap_t* bm, Py_ssize_t start)
{
    assert(start >= 0);
    Py_ssize_t wi = start >> BITMAP_WORD_SHIFT;
    if (bm->summary == NULL || wi >= bm->nwords) {
        return -1;
    }
    bitmap_word_t w =
        _bitmap_word_ready(bm, wi)
            ? bm->words[wi] & (BITMAP_ALL << (start & BITMAP_WORD_MASK))
            : 0;
    for (;;) {
        if (w != 0) {
            return (wi << BITMAP_WORD_SHIFT) + _bitmap_ctz(w);
        }
        wi++;
        if (bm->dense) {
            if (wi >= bm->nwords) {
                return -1;
            }
            w = bm->words[wi];
            continue;
        }
        /* Next ready word at or after wi, via the summary. */
        Py_ssize_t si = wi >> BITMAP_WORD_SHIFT;
        Py_ssize_t nsummary = _bitmap_nwords(bm->nwords);
        if (si >= nsummary) {
            return -1;
        }
        bitmap_word_t s =
            bm->summary[si] & (BITMAP_ALL << (wi & BITMAP_WORD_MASK));
        while (s == 0) {
            if (++si >= nsummary) {
                return -1;
            }
            s = bm->summary[si];
        }
        wi = (si << BITMAP_WORD_SHIFT) + _bitmap_ctz(s);
        if (wi >= bm->nwords) {
            return -1;
        }
        w = bm->words[wi];
    }
}

#ifdef __cplusplus
}
#endif

#endif
