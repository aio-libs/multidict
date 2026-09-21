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
typedef size_t md_bitmap_word_t;
#define MD_BITMAP_WORD_BITS (SIZEOF_SIZE_T * 8)
#if SIZEOF_SIZE_T == 8
#define MD_BITMAP_WORD_SHIFT 6
#else
#define MD_BITMAP_WORD_SHIFT 5
#endif
#define MD_BITMAP_WORD_MASK (MD_BITMAP_WORD_BITS - 1)
#define MD_BITMAP_ONE ((md_bitmap_word_t)1)
#define MD_BITMAP_ALL (~(md_bitmap_word_t)0)

#define MD_BITMAP_INLINE_BYTES 4096
#define MD_BITMAP_DENSE_WORDS 64
#define MD_BITMAP_INLINE_WORDS \
    (MD_BITMAP_INLINE_BYTES / sizeof(md_bitmap_word_t))
#define MD_BITMAP_INLINE_SUMMARY (MD_BITMAP_INLINE_WORDS / MD_BITMAP_WORD_BITS)

typedef struct _md_bitmap {
    md_bitmap_word_t* words;
    md_bitmap_word_t* summary;
    Py_ssize_t nwords;
    htkeys_t* keys;
    bool dense;
    md_bitmap_word_t inline_summary[MD_BITMAP_INLINE_SUMMARY];
    md_bitmap_word_t inline_words[MD_BITMAP_INLINE_WORDS];
} md_bitmap_t;

static inline Py_ssize_t
_md_bitmap_nwords(Py_ssize_t nbits)
{
    return (nbits + MD_BITMAP_WORD_MASK) >> MD_BITMAP_WORD_SHIFT;
}

/* Can't fail and touches no storage: the first mark does that. */
static inline void
md_bitmap_init(md_bitmap_t* bm, htkeys_t* keys, Py_ssize_t nbits)
{
    bm->nwords = _md_bitmap_nwords(nbits);
    bm->keys = keys;
    bm->words = NULL;
    bm->summary = NULL;
    bm->dense = false;
}

COLD static int
_md_bitmap_alloc(md_bitmap_t* bm, Py_ssize_t nsummary)
{
    md_bitmap_word_t* block = PyMem_Malloc((size_t)(nsummary + bm->nwords) *
                                           sizeof(md_bitmap_word_t));
    if (block == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    bm->summary = block;
    bm->words = block + nsummary;
    return 0;
}

COLD static int
_md_bitmap_start(md_bitmap_t* bm)
{
    Py_ssize_t nsummary = _md_bitmap_nwords(bm->nwords);
    if (bm->nwords <= MD_BITMAP_DENSE_WORDS) {
        /* Small enough to zero outright, which spares every later access
           the summary lookup. */
        bm->dense = true;
        bm->summary = bm->inline_summary;
        bm->words = bm->inline_words;
        memset(bm->words, 0, (size_t)bm->nwords * sizeof(md_bitmap_word_t));
        return 0;
    }
    if ((size_t)bm->nwords <= MD_BITMAP_INLINE_WORDS) {
        bm->summary = bm->inline_summary;
        bm->words = bm->inline_words;
    } else if (_md_bitmap_alloc(bm, nsummary) < 0) {
        return -1;
    }
    memset(bm->summary, 0, (size_t)nsummary * sizeof(md_bitmap_word_t));
    return 0;
}

/* Sets up storage now, so that no later mark on this bitmap can fail. */
static inline int
md_bitmap_reserve(md_bitmap_t* bm)
{
    if (bm->summary != NULL) {
        return 0;
    }
    return _md_bitmap_start(bm);
}

/* Safe to call more than once, and on a bitmap whose init never ran,
   provided `summary` was set to NULL up front. */
static inline void
md_bitmap_release(md_bitmap_t* bm)
{
    if (bm->summary != NULL && bm->summary != bm->inline_summary) {
        PyMem_Free(bm->summary);
    }
    bm->summary = NULL;
    bm->words = NULL;
}

ALWAYS_INLINE static inline bool
_md_bitmap_word_ready(const md_bitmap_t* bm, Py_ssize_t wi)
{
    return bm->dense || ((bm->summary[wi >> MD_BITMAP_WORD_SHIFT] >>
                          (wi & MD_BITMAP_WORD_MASK)) &
                         1);
}

ALWAYS_INLINE static inline bool
_md_bitmap_dense_test(const md_bitmap_t* bm, Py_ssize_t i)
{
    return (bm->words[i >> MD_BITMAP_WORD_SHIFT] >>
            (i & MD_BITMAP_WORD_MASK)) &
           1;
}

/* The word holding `i`, zeroed first if nothing has touched it yet, or
   NULL with an exception set if heap storage couldn't be allocated. */
ALWAYS_INLINE static inline md_bitmap_word_t*
_md_bitmap_word(md_bitmap_t* bm, Py_ssize_t i)
{
    assert(i >= 0 && i < bm->nwords * MD_BITMAP_WORD_BITS);
    Py_ssize_t wi = i >> MD_BITMAP_WORD_SHIFT;
    if (bm->dense) {
        return bm->words + wi;
    }
    if (UNLIKELY(bm->summary == NULL)) {
        if (_md_bitmap_start(bm) < 0) {
            return NULL;
        }
        if (bm->dense) {
            return bm->words + wi;
        }
    }
    md_bitmap_word_t* s = bm->summary + (wi >> MD_BITMAP_WORD_SHIFT);
    md_bitmap_word_t sbit = MD_BITMAP_ONE << (wi & MD_BITMAP_WORD_MASK);
    if (!(*s & sbit)) {
        *s |= sbit;
        bm->words[wi] = 0;
    }
    return bm->words + wi;
}

/* Moves `src` into `dst`, releasing what `dst` held. `src` is left
   released. */
static inline void
md_bitmap_move(md_bitmap_t* dst, md_bitmap_t* src)
{
    md_bitmap_release(dst);
    dst->nwords = src->nwords;
    dst->keys = src->keys;
    dst->dense = src->dense;
    if (src->summary == src->inline_summary) {
        dst->words = dst->inline_words;
        dst->summary = dst->inline_summary;
        if (src->dense) {
            memcpy(dst->words,
                   src->words,
                   (size_t)src->nwords * sizeof(md_bitmap_word_t));
            src->words = NULL;
            src->summary = NULL;
            return;
        }
        memcpy(
            dst->summary,
            src->summary,
            (size_t)_md_bitmap_nwords(src->nwords) * sizeof(md_bitmap_word_t));
        for (Py_ssize_t wi = 0; wi < src->nwords; wi++) {
            if (_md_bitmap_word_ready(src, wi)) {
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
md_bitmap_test(const md_bitmap_t* bm, Py_ssize_t i)
{
    assert(i >= 0 && i < bm->nwords * MD_BITMAP_WORD_BITS);
    if (bm->dense) {
        return _md_bitmap_dense_test(bm, i);
    }
    if (bm->summary == NULL) {
        return false;
    }
    return _md_bitmap_word_ready(bm, i >> MD_BITMAP_WORD_SHIFT) &&
           _md_bitmap_dense_test(bm, i);
}

ALWAYS_INLINE static inline int
md_bitmap_set(md_bitmap_t* bm, Py_ssize_t i)
{
    md_bitmap_word_t* word = _md_bitmap_word(bm, i);
    if (word == NULL) {
        return -1;
    }
    *word |= MD_BITMAP_ONE << (i & MD_BITMAP_WORD_MASK);
    return 0;
}

ALWAYS_INLINE static inline void
md_bitmap_clear(md_bitmap_t* bm, Py_ssize_t i)
{
    assert(i >= 0 && i < bm->nwords * MD_BITMAP_WORD_BITS);
    if (bm->summary == NULL) {
        return;
    }
    Py_ssize_t wi = i >> MD_BITMAP_WORD_SHIFT;
    if (_md_bitmap_word_ready(bm, wi)) {
        bm->words[wi] &= ~(MD_BITMAP_ONE << (i & MD_BITMAP_WORD_MASK));
    }
}

/* 1 if `i` was already set, 0 if it wasn't (it is now), -1 on error. */
ALWAYS_INLINE static inline int
md_bitmap_test_and_set(md_bitmap_t* bm, Py_ssize_t i)
{
    md_bitmap_word_t* word = _md_bitmap_word(bm, i);
    if (word == NULL) {
        return -1;
    }
    md_bitmap_word_t bit = MD_BITMAP_ONE << (i & MD_BITMAP_WORD_MASK);
    int was_set = (*word & bit) != 0;
    *word |= bit;
    return was_set;
}

static inline int
_md_bitmap_ctz(md_bitmap_word_t w)
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
md_bitmap_next(const md_bitmap_t* bm, Py_ssize_t start)
{
    assert(start >= 0);
    Py_ssize_t wi = start >> MD_BITMAP_WORD_SHIFT;
    if (bm->summary == NULL || wi >= bm->nwords) {
        return -1;
    }
    md_bitmap_word_t w =
        _md_bitmap_word_ready(bm, wi)
            ? bm->words[wi] & (MD_BITMAP_ALL << (start & MD_BITMAP_WORD_MASK))
            : 0;
    for (;;) {
        if (w != 0) {
            return (wi << MD_BITMAP_WORD_SHIFT) + _md_bitmap_ctz(w);
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
        Py_ssize_t si = wi >> MD_BITMAP_WORD_SHIFT;
        Py_ssize_t nsummary = _md_bitmap_nwords(bm->nwords);
        if (si >= nsummary) {
            return -1;
        }
        md_bitmap_word_t s =
            bm->summary[si] & (MD_BITMAP_ALL << (wi & MD_BITMAP_WORD_MASK));
        while (s == 0) {
            if (++si >= nsummary) {
                return -1;
            }
            s = bm->summary[si];
        }
        wi = (si << MD_BITMAP_WORD_SHIFT) + _md_bitmap_ctz(s);
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
