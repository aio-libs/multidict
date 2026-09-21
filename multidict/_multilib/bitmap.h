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
   the summary (nwords/64). That keeps a walk that marks little from
   paying for the whole table.

   `summary == NULL` means no storage yet: either nothing was marked
   (reads as empty), or the bitmap was released. */

#define MD_BITMAP_INLINE_BYTES 4096
#define MD_BITMAP_INLINE_WORDS (MD_BITMAP_INLINE_BYTES / 8)
#define MD_BITMAP_INLINE_SUMMARY (MD_BITMAP_INLINE_WORDS / 64)

typedef struct _md_bitmap {
    uint64_t* words;
    uint64_t* summary;
    Py_ssize_t nwords;
    htkeys_t* keys;
    uint64_t inline_summary[MD_BITMAP_INLINE_SUMMARY];
    uint64_t inline_words[MD_BITMAP_INLINE_WORDS];
} md_bitmap_t;

static inline Py_ssize_t
_md_bitmap_nwords(Py_ssize_t nbits)
{
    return (nbits + 63) / 64;
}

/* Can't fail and touches no storage: the first mark does that. */
static inline void
md_bitmap_init(md_bitmap_t* bm, htkeys_t* keys, Py_ssize_t nbits)
{
    bm->nwords = _md_bitmap_nwords(nbits);
    bm->keys = keys;
    bm->words = NULL;
    bm->summary = NULL;
}

COLD static int
_md_bitmap_alloc(md_bitmap_t* bm, Py_ssize_t nsummary)
{
    uint64_t* block =
        PyMem_Malloc((size_t)(nsummary + bm->nwords) * sizeof(uint64_t));
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
    if (bm->nwords <= MD_BITMAP_INLINE_WORDS) {
        bm->summary = bm->inline_summary;
        bm->words = bm->inline_words;
    } else if (_md_bitmap_alloc(bm, nsummary) < 0) {
        return -1;
    }
    memset(bm->summary, 0, (size_t)nsummary * sizeof(uint64_t));
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
    return (bm->summary[wi >> 6] >> (wi & 63)) & 1;
}

/* The word holding `i`, zeroed first if nothing has touched it yet, or
   NULL with an exception set if heap storage couldn't be allocated. */
ALWAYS_INLINE static inline uint64_t*
_md_bitmap_word(md_bitmap_t* bm, Py_ssize_t i)
{
    assert(i >= 0 && i < bm->nwords * 64);
    if (UNLIKELY(bm->summary == NULL)) {
        if (_md_bitmap_start(bm) < 0) {
            return NULL;
        }
    }
    Py_ssize_t wi = i >> 6;
    uint64_t* s = bm->summary + (wi >> 6);
    uint64_t sbit = (uint64_t)1 << (wi & 63);
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
    if (src->summary == src->inline_summary) {
        dst->words = dst->inline_words;
        dst->summary = dst->inline_summary;
        memcpy(dst->summary,
               src->summary,
               (size_t)_md_bitmap_nwords(src->nwords) * sizeof(uint64_t));
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
    assert(i >= 0 && i < bm->nwords * 64);
    if (bm->summary == NULL) {
        return false;
    }
    Py_ssize_t wi = i >> 6;
    return _md_bitmap_word_ready(bm, wi) && ((bm->words[wi] >> (i & 63)) & 1);
}

ALWAYS_INLINE static inline int
md_bitmap_set(md_bitmap_t* bm, Py_ssize_t i)
{
    uint64_t* word = _md_bitmap_word(bm, i);
    if (word == NULL) {
        return -1;
    }
    *word |= (uint64_t)1 << (i & 63);
    return 0;
}

ALWAYS_INLINE static inline void
md_bitmap_clear(md_bitmap_t* bm, Py_ssize_t i)
{
    assert(i >= 0 && i < bm->nwords * 64);
    if (bm->summary == NULL) {
        return;
    }
    Py_ssize_t wi = i >> 6;
    if (_md_bitmap_word_ready(bm, wi)) {
        bm->words[wi] &= ~((uint64_t)1 << (i & 63));
    }
}

/* 1 if `i` was already set, 0 if it wasn't (it is now), -1 on error. */
ALWAYS_INLINE static inline int
md_bitmap_test_and_set(md_bitmap_t* bm, Py_ssize_t i)
{
    uint64_t* word = _md_bitmap_word(bm, i);
    if (word == NULL) {
        return -1;
    }
    uint64_t bit = (uint64_t)1 << (i & 63);
    int was_set = (*word & bit) != 0;
    *word |= bit;
    return was_set;
}

static inline int
_md_bitmap_ctz(uint64_t w)
{
    assert(w != 0);
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ctzll(w);
#elif defined(_MSC_VER) && defined(_WIN64)
    unsigned long idx;
    _BitScanForward64(&idx, w);
    return (int)idx;
#else
    int n = 0;
    while (!(w & 1)) {
        w >>= 1;
        n++;
    }
    return n;
#endif
}

/* The first set index at or after `start`, or -1 if there is none.
   Untouched words are skipped a summary word (64 words) at a time. */
static inline Py_ssize_t
md_bitmap_next(const md_bitmap_t* bm, Py_ssize_t start)
{
    assert(start >= 0);
    Py_ssize_t wi = start >> 6;
    if (bm->summary == NULL || wi >= bm->nwords) {
        return -1;
    }
    uint64_t w = _md_bitmap_word_ready(bm, wi)
                     ? bm->words[wi] & (~(uint64_t)0 << (start & 63))
                     : 0;
    for (;;) {
        if (w != 0) {
            return (wi << 6) + _md_bitmap_ctz(w);
        }
        wi++;
        /* Next ready word at or after wi, via the summary. */
        Py_ssize_t si = wi >> 6;
        Py_ssize_t nsummary = _md_bitmap_nwords(bm->nwords);
        if (si >= nsummary) {
            return -1;
        }
        uint64_t s = bm->summary[si] & (~(uint64_t)0 << (wi & 63));
        while (s == 0) {
            if (++si >= nsummary) {
                return -1;
            }
            s = bm->summary[si];
        }
        wi = (si << 6) + _md_bitmap_ctz(s);
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
