#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_HTKEYS_H
#define _MULTIDICT_HTKEYS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <stdbool.h>

#include "atomic_helpers.h"
#include "compiler.h"
#include "freelist.h"
#include "istr_object.h"

/* Implementation note.
identity always has exact PyUnicode_Type type, not a subclass.
It guarantees that identity hashing and comparison never calls
Python code back, and these operations has no weird side effects,
e.g. deletion the key from multidict.

Taking into account the fact that all multidict operations except
repr(md), repr(md_proxy), or repr(view) never access to the key
itself but identity instead, borrowed references during iteration
over pair_list for, e.g., md.get() or md.pop() is safe.
*/

/* The key and the value lead, so that a table of KIND_COMPACT, which stores
   only them, is a prefix of this layout. */
typedef struct entry {
    PyObject* key;
    PyObject* value;
    PyObject* identity;
    Py_hash_t hash;
} entry_t;

/* How a table stores its entries. KIND_ANYSTR stores the whole entry_t.
   KIND_COMPACT stores only the key and the value, and the class decides
   what the identity is: in a MultiDict every key is an exact str that is
   its own identity, in a CIMultiDict an exact istr whose canonical form is.
   Either way the hash is the one the identity caches. */
typedef enum {
    KIND_ANYSTR = 0,
    KIND_COMPACT = 1,
} htkeys_kind_t;

#define HTKEYS_KINDS 2

#define DKIX_EMPTY (-1) /* empty (never used) slot */
#define DKIX_DUMMY (-2) /* deleted slot */

#define HT_LOG_MINSIZE 3
#define HT_MINSIZE 8
#define HT_PERTURB_SHIFT 5

/* Tables are pooled by size class, since a block is reusable only for
   its own log2_size: every other field of the allocation, down to the
   width of an index slot, follows from it.

   The ladder runs from the smallest table up to the one a 100-item
   constructor pre-sizes to. Past that a table is big enough that the
   allocation is a small part of filling it, and deep enough pools
   would retain real memory. Depth falls as the class grows for the
   same reason: the whole ladder full is about 109 KB per interpreter,
   most of it in the three smallest classes, which are also the ones a
   dict grown by repeated add() passes through and discards. */
#define HTKEYS_POOL_MIN_LOG2 HT_LOG_MINSIZE
#define HTKEYS_POOL_MAX_LOG2 8
#define HTKEYS_POOL_CLASSES (HTKEYS_POOL_MAX_LOG2 - HTKEYS_POOL_MIN_LOG2 + 1)
// one ladder per kind: a block fits only its own kind's entry size
#define HTKEYS_POOLS (HTKEYS_KINDS * HTKEYS_POOL_CLASSES)

NOINLINE static void
htkeys_pools_init(pool_t* pools)
{
    static const uint8_t depths[HTKEYS_POOL_CLASSES] = {32, 32, 32, 16, 8, 4};
    for (int i = 0; i < HTKEYS_POOLS; i++) {
        pool_init(pools + i, depths[i % HTKEYS_POOL_CLASSES]);
    }
}

static inline void
htkeys_pools_clear(pool_t* pools)
{
    for (int i = 0; i < HTKEYS_POOLS; i++) {
        pool_clear(pools + i, PyMem_Free);
    }
}

/* NULL for a size class that isn't pooled. empty_htkeys never reaches
   here: it is never allocated, and every htkeys_free() call site guards
   on it. A build with no pools folds the whole thing away, since
   pool_pop() is then a bare NULL. */
static inline pool_t*
_htkeys_pool(pool_t* pools, uint8_t log2_size, uint8_t kind)
{
    assert(log2_size >= HTKEYS_POOL_MIN_LOG2);
    assert(kind < HTKEYS_KINDS);
    if (log2_size > HTKEYS_POOL_MAX_LOG2) {
        return NULL;
    }
    return pools + kind * HTKEYS_POOL_CLASSES +
           (log2_size - HTKEYS_POOL_MIN_LOG2);
}

#define HT_LOG_RESUME_SLOTS_MINSIZE 10
/* Probe steps after perturb is 0 before resume slots are allocated */
#define HT_RESUME_SLOTS_MIN_STEPS 32

typedef struct _htkeys {
    /* Size of the hash table (indices). It must be a power of 2. */
    uint8_t log2_size;

    /* Size of the hash table (indices) by bytes. */
    uint8_t log2_index_bytes;

    /* An htkeys_kind_t, fixed for the table's lifetime. */
    uint8_t kind;

    /* Number of usable entries in dk_entries. */
    Py_ssize_t usable;

    /* Number of used entries in dk_entries. */
    Py_ssize_t nentries;

    /* Allocated on the first long probe, see htkeys_resume_slots_bytes(). */
    void* resume_slots;

#ifdef Py_GIL_DISABLED
    Py_ssize_t num_readers;

    struct _htkeys* retired_next;
#endif

    /* Actual hash table of dk_size entries. It holds indices in dk_entries,
       or DKIX_EMPTY(-1) or DKIX_DUMMY(-2).

       Indices must be: 0 <= indice < USABLE_FRACTION(dk_size).

       The size in bytes of an indice depends on dk_size:

       - 1 byte if htkeys_nslots() <= 0xff (char*)
       - 2 bytes if htkeys_nslots() <= 0xffff (int16_t*)
       - 4 bytes if htkeys_nslots() <= 0xffffffff (int32_t*)
       - 8 bytes otherwise (int64_t*)

       Dynamically sized, SIZEOF_VOID_P is minimum. */
    char indices[]; /* char is required to avoid strict aliasing. */

} htkeys_t;

#if SIZEOF_VOID_P > 4
static inline Py_ssize_t
htkeys_nslots(const htkeys_t* keys)
{
    return ((int64_t)1) << keys->log2_size;
}
#else
static inline Py_ssize_t
htkeys_nslots(const htkeys_t* keys)
{
    return 1 << keys->log2_size;
}
#endif

static inline Py_ssize_t
_htkeys_mask(const htkeys_t* keys)
{
    return htkeys_nslots(keys) - 1;
}

static inline entry_t*
htkeys_entries(const htkeys_t* dk)
{
    int8_t* indices = (int8_t*)(dk->indices);
    size_t index = (size_t)1 << dk->log2_index_bytes;
    return (entry_t*)(&indices[index]);
}

/* Code reaches a neighbouring entry, an entry's identity and its hash only
   through these and the accessors in freethreading.h, passing the kind
   of the table the entry belongs to, which decides where they are.
   The key and the value are plain fields, at the same offsets in every
   kind. */
/* Both sizes are powers of two, so stepping is a shift, not a multiply. */
static inline unsigned
htkeys_kind_entry_shift(uint8_t kind)
{
    Py_BUILD_ASSERT(sizeof(entry_t) == 4 * sizeof(PyObject*));
    Py_BUILD_ASSERT(offsetof(entry_t, identity) == 2 * sizeof(PyObject*));
    return (SIZEOF_VOID_P == 8 ? 4 : 3) + (kind != KIND_COMPACT);
}

// A compact kind stores only the key and the value.
static inline bool
kind_is_compact(uint8_t kind)
{
    return kind == KIND_COMPACT;
}

static inline PyObject*
istr_canonical(PyObject* key)
{
    return ((istrobject*)key)->canonical;
}

/* The identity of a compact entry's key, `ci` being whether the table
   belongs to a CIMultiDict. */
static inline PyObject*
compact_key_identity(bool ci, PyObject* key)
{
    return ci ? istr_canonical(key) : key;
}

/* The identity of the object the identity slot holds (see
   entry_identity_slot()): the object itself, except for a CIMultiDict's
   compact table, whose slot holds the istr key. */
static inline PyObject*
slot_identity(uint8_t kind, bool ci, PyObject* held)
{
    return kind_is_compact(kind) ? compact_key_identity(ci, held) : held;
}

static inline size_t
htkeys_kind_entry_size(uint8_t kind)
{
    return (size_t)1 << htkeys_kind_entry_shift(kind);
}

/* These take the kind, not the table, so a function reads keys->kind
   once: with -fno-strict-aliasing any store may alias it, and a read per
   call would reload it every time. */
static inline entry_t*
entry_at(uint8_t kind, entry_t* entries, Py_ssize_t i)
{
    return (entry_t*)((char*)entries +
                      ((size_t)i << htkeys_kind_entry_shift(kind)));
}

static inline entry_t*
entry_next(uint8_t kind, entry_t* entry)
{
    return (entry_t*)((char*)entry + htkeys_kind_entry_size(kind));
}

static inline entry_t*
entry_prev(uint8_t kind, entry_t* entry)
{
    return (entry_t*)((char*)entry - htkeys_kind_entry_size(kind));
}

/* NULL for a hole. */
static inline PyObject*
entry_identity(uint8_t kind, bool ci, const entry_t* entry)
{
    if (kind_is_compact(kind)) {
        PyObject* key = entry->key;
        return key == NULL ? NULL : compact_key_identity(ci, key);
    }
    return entry->identity;
}

/* Whether entry is a hole (deleted or never filled). Cheaper than testing
   entry_identity() for NULL, which in a CIMultiDict's compact table
   reads through the key. */
static inline bool
entry_is_hole(uint8_t kind, const entry_t* entry)
{
    return kind_is_compact(kind) ? entry->key == NULL
                                 : entry->identity == NULL;
}

/* The field lock-free readers check first: the key in a compact table, so
   what it holds is the identity only in a MultiDict's. See
   slot_identity(). */
static inline PyObject**
entry_identity_slot(uint8_t kind, entry_t* entry)
{
    return kind_is_compact(kind) ? &entry->key : &entry->identity;
}

static inline void
entry_set_identity(uint8_t kind, entry_t* entry, PyObject* identity)
{
    if (!kind_is_compact(kind)) {
        entry->identity = identity;
    }
}

// The str caches its hash, set when the key went in.
static inline Py_hash_t
_str_cached_hash(PyObject* str)
{
    PyASCIIObject* ascii = (PyASCIIObject*)str;
#ifdef Py_GIL_DISABLED
    return atomic_load_ssize_relaxed(&ascii->hash);
#else
    return ascii->hash;
#endif
}

/* The hash of a compact entry's identity, read from the key itself: a
   str's cached hash, or the canonical form's hash an istr keeps. Never
   through istr->canonical, so a lock-free reader whose key a concurrent
   delete just freed reads one stale integer from it, not a stale pointer
   it would follow; see load_hash(). */
static inline Py_hash_t
compact_key_hash(bool ci, PyObject* key)
{
    if (ci) {
#ifdef Py_GIL_DISABLED
        return atomic_load_ssize_relaxed(&((istrobject*)key)->canonical_hash);
#else
        return ((istrobject*)key)->canonical_hash;
#endif
    }
    return _str_cached_hash(key);
}

static inline Py_hash_t
entry_hash(uint8_t kind, bool ci, const entry_t* entry)
{
    if (kind_is_compact(kind)) {
        return compact_key_hash(ci, entry->key);
    }
    return entry->hash;
}

static inline void
entry_set_hash(uint8_t kind, entry_t* entry, Py_hash_t hash)
{
    if (!kind_is_compact(kind)) {
        entry->hash = hash;
    }
}

static inline size_t
htkeys_entry_size(const htkeys_t* keys)
{
    return htkeys_kind_entry_size(keys->kind);
}

static inline entry_t*
htkeys_entry_at(const htkeys_t* keys, entry_t* entries, Py_ssize_t i)
{
    return entry_at(keys->kind, entries, i);
}

static inline entry_t*
htkeys_entry_next(const htkeys_t* keys, entry_t* entry)
{
    return entry_next(keys->kind, entry);
}

static inline entry_t*
htkeys_entry_prev(const htkeys_t* keys, entry_t* entry)
{
    return entry_prev(keys->kind, entry);
}

/* How many entries lie between `entries` and `entry`. */
static inline Py_ssize_t
htkeys_entry_index(const htkeys_t* keys, const entry_t* entries,
                   const entry_t* entry)
{
    return (Py_ssize_t)((size_t)((const char*)entry - (const char*)entries) /
                        htkeys_entry_size(keys));
}

static inline void
htkeys_entry_copy(const htkeys_t* keys, entry_t* dst, const entry_t* src)
{
    memcpy(dst, src, htkeys_entry_size(keys));
}

/* A slot in indices[] is written under md's critical section but read by
   lock-free walks too, so both sides go through a relaxed atomic; on the
   GIL build atomic_*_int*_relaxed() is the plain access. The slot width
   follows the table size (see the indices[] comment above), hence one
   pair per width rather than one generic pair. */
#ifdef Py_GIL_DISABLED
#define _MD_DEFINE_INDEX_ACCESSORS(bits)                                      \
    static inline int##bits##_t htkeys_load_index##bits(const htkeys_t* keys, \
                                                        Py_ssize_t i)         \
    {                                                                         \
        return atomic_load_int##bits##_relaxed(                               \
            &((const int##bits##_t*)(keys->indices))[i]);                     \
    }                                                                         \
    static inline void htkeys_store_index##bits(                              \
        htkeys_t* keys, Py_ssize_t i, Py_ssize_t ix)                          \
    {                                                                         \
        atomic_store_int##bits##_relaxed(                                     \
            &((int##bits##_t*)(keys->indices))[i], (int##bits##_t)ix);        \
    }
#else
#define _MD_DEFINE_INDEX_ACCESSORS(bits)                                      \
    static inline int##bits##_t htkeys_load_index##bits(const htkeys_t* keys, \
                                                        Py_ssize_t i)         \
    {                                                                         \
        return ((const int##bits##_t*)(keys->indices))[i];                    \
    }                                                                         \
    static inline void htkeys_store_index##bits(                              \
        htkeys_t* keys, Py_ssize_t i, Py_ssize_t ix)                          \
    {                                                                         \
        ((int##bits##_t*)(keys->indices))[i] = (int##bits##_t)ix;             \
    }
#endif

_MD_DEFINE_INDEX_ACCESSORS(8)
_MD_DEFINE_INDEX_ACCESSORS(16)
_MD_DEFINE_INDEX_ACCESSORS(32)
_MD_DEFINE_INDEX_ACCESSORS(64)
#undef _MD_DEFINE_INDEX_ACCESSORS

/* lookup indices.  returns DKIX_EMPTY, DKIX_DUMMY, or ix >=0 */
ALWAYS_INLINE static inline Py_ssize_t
htkeys_get_index(const htkeys_t* keys, Py_ssize_t i)
{
    uint8_t log2size = keys->log2_size;
    Py_ssize_t ix;

    if (log2size < 8) {
        ix = htkeys_load_index8(keys, i);
    } else if (log2size < 16) {
        ix = htkeys_load_index16(keys, i);
    }
#if SIZEOF_VOID_P > 4
    else if (log2size >= 32) {
        ix = htkeys_load_index64(keys, i);
    }
#endif
    else {
        ix = htkeys_load_index32(keys, i);
    }
    assert(ix >= DKIX_DUMMY);
    return ix;
}

/* write to indices. */
ALWAYS_INLINE static inline void
htkeys_set_index(htkeys_t* keys, Py_ssize_t i, Py_ssize_t ix)
{
    uint8_t log2size = keys->log2_size;

    assert(ix >= DKIX_DUMMY);

    if (log2size < 8) {
        assert(ix <= 0x7f);
        htkeys_store_index8(keys, i, ix);
    } else if (log2size < 16) {
        assert(ix <= 0x7fff);
        htkeys_store_index16(keys, i, ix);
    }
#if SIZEOF_VOID_P > 4
    else if (log2size >= 32) {
        htkeys_store_index64(keys, i, ix);
    }
#endif
    else {
        assert(ix <= 0x7fffffff);
        htkeys_store_index32(keys, i, ix);
    }
}

/* USABLE_FRACTION is the maximum dictionary load.
 * Increasing this ratio makes dictionaries more dense resulting in more
 * collisions.  Decreasing it improves sparseness at the expense of spreading
 * indices over more cache lines and at the cost of total memory consumed.
 *
 * USABLE_FRACTION must obey the following:
 *     (0 < USABLE_FRACTION(n) < n) for all n >= 2
 *
 * USABLE_FRACTION should be quick to calculate.
 * Fractions around 1/2 to 2/3 seem to work well in practice.
 */
static inline Py_ssize_t
USABLE_FRACTION(Py_ssize_t n)
{
    return (n << 1) / 3;
}

// Return the index of the most significant 1 bit in 'x'. This is the smallest
// integer k such that x < 2**k. Equivalent to floor(log2(x)) + 1 for x != 0.
static inline int
_ht_bit_length(unsigned long x)
{
#if (defined(__clang__) || defined(__GNUC__))
    if (x != 0) {
        // __builtin_clzl() is available since GCC 3.4.
        // Undefined behavior for x == 0.
        return (int)sizeof(unsigned long) * 8 - __builtin_clzl(x);
    } else {
        return 0;
    }
#elif defined(_MSC_VER)
    // _BitScanReverse() is documented to search 32 bits.
    Py_BUILD_ASSERT(sizeof(unsigned long) <= 4);
    unsigned long msb;
    if (_BitScanReverse(&msb, x)) {
        return (int)msb + 1;
    } else {
        return 0;
    }
#else
    const int BIT_LENGTH_TABLE[32] = {0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4,
                                      4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5,
                                      5, 5, 5, 5, 5, 5, 5, 5, 5, 5};
    int msb = 0;
    while (x >= 32) {
        msb += 6;
        x >>= 6;
    }
    msb += BIT_LENGTH_TABLE[x];
    return msb;
#endif
}

/* Find the smallest dk_size >= minsize. */
static inline uint8_t
calculate_log2_keysize(Py_ssize_t minsize)
{
#if SIZEOF_LONG == SIZEOF_SIZE_T
    minsize = (minsize | HT_MINSIZE) - 1;
    return (uint8_t)_ht_bit_length(
        (unsigned long)(minsize | (HT_MINSIZE - 1)));
#elif defined(_MSC_VER)
    // On 64bit Windows, sizeof(long) == 4.
    minsize = (minsize | HT_MINSIZE) - 1;
    unsigned long msb;
    _BitScanReverse64(&msb, (uint64_t)minsize);
    return (uint8_t)(msb + 1);
#else
    uint8_t log2_size;
    for (log2_size = HT_LOG_MINSIZE; (((Py_ssize_t)1) << log2_size) < minsize;
         log2_size++);
    return log2_size;
#endif
}

/* estimate_keysize is reverse function of USABLE_FRACTION.
 *
 * This can be used to reserve enough size to insert n entries without
 * resizing.
 */
static inline uint8_t
estimate_log2_keysize(Py_ssize_t n)
{
    return calculate_log2_keysize((n * 3 + 1) / 2);
}

/* This immutable, empty PyDictKeysObject is used for PyDict_Clear()
 * (which cannot fail and thus can do no allocation).
 *
 * See https://github.com/python/cpython/pull/127568#discussion_r1868070614
 * for the rationale of using log2_index_bytes=3 instead of 0.
 */
static const htkeys_t empty_htkeys = {
    .log2_size = 0,
    .log2_index_bytes = 3,
    .usable = 0, /* immutable */
    .nentries = 0,
    .resume_slots = NULL,
#ifdef Py_GIL_DISABLED
    .num_readers = 0,
    .retired_next = NULL,
#endif
    .indices = {DKIX_EMPTY,
                DKIX_EMPTY,
                DKIX_EMPTY,
                DKIX_EMPTY,
                DKIX_EMPTY,
                DKIX_EMPTY,
                DKIX_EMPTY,
                DKIX_EMPTY},
};

/* resume_slots[i] is 0 or 1 + the last slot used by a probe that reached slot
   i with perturb == 0. From there the probe sequence only depends on the slot
   and slots never become empty again, so probing can resume. Stored like the
   indices, uint16_t below 2**16 slots and uint32_t below 2**32, slot + 1 must
   fit. */
static inline size_t
htkeys_resume_slots_bytes(uint8_t log2_size)
{
    if (log2_size < HT_LOG_RESUME_SLOTS_MINSIZE || log2_size >= 32) {
        return 0;
    }
    if (log2_size < 16) {
        return sizeof(uint16_t) << log2_size;
    }
    return sizeof(uint32_t) << log2_size;
}

/* Width of an index slot, see the indices[] comment above. */
static inline uint8_t
_htkeys_log2_index_bytes(uint8_t log2_size)
{
    if (log2_size < 8) {
        return log2_size;
    }
    if (log2_size < 16) {
        return (uint8_t)(log2_size + 1);
    }
#if SIZEOF_VOID_P > 4
    if (log2_size >= 32) {
        return (uint8_t)(log2_size + 3);
    }
#endif
    return (uint8_t)(log2_size + 2);
}

/* Everything about the allocation follows from log2_size, which is what
   lets a pooled block be reused for any table of its own size class,
   and lets md_clone_from_ht() copy a table byte for byte. */
static inline size_t
_htkeys_alloc_size(uint8_t log2_size, uint8_t kind)
{
    size_t usable =
        (size_t)USABLE_FRACTION((Py_ssize_t)((size_t)1 << log2_size));
    return (sizeof(htkeys_t) +
            ((size_t)1 << _htkeys_log2_index_bytes(log2_size)) +
            htkeys_kind_entry_size(kind) * usable);
}

/* Whether _htkeys_alloc_size(log2_size) can be computed without a shift
   past the width of size_t or a sum past PY_SSIZE_T_MAX. */
static inline bool
htkeys_size_fits(uint8_t log2_size)
{
    uint8_t log2_bytes = _htkeys_log2_index_bytes(log2_size);
    if (log2_bytes >= SIZEOF_SIZE_T * 8 - 1) {
        return false;
    }
    size_t index_bytes = (size_t)1 << log2_bytes;
    size_t usable = (size_t)USABLE_FRACTION((Py_ssize_t)1 << log2_size);
    return usable <=
           ((size_t)PY_SSIZE_T_MAX - sizeof(htkeys_t) - index_bytes) /
               sizeof(entry_t);
}

/* The same number as _htkeys_alloc_size(keys->log2_size), read back off
   the table rather than recomputed. */
static inline Py_ssize_t
htkeys_sizeof(htkeys_t* keys)
{
    Py_ssize_t usable =
        USABLE_FRACTION((Py_ssize_t)((size_t)1 << keys->log2_size));
    Py_ssize_t size =
        (Py_ssize_t)(sizeof(htkeys_t) + ((size_t)1 << keys->log2_index_bytes) +
                     htkeys_entry_size(keys) * (size_t)usable);
    assert(size ==
           (Py_ssize_t)_htkeys_alloc_size(keys->log2_size, keys->kind));
    return size;
}

/* Uninitialized storage for a table of `log2_size`. The caller owns
   every byte and must write the header before anything reads it.
   `size` is the byte count, taken as an argument for the sake of a
   caller that already has it off an existing table. */
static inline htkeys_t*
htkeys_alloc_sized(pool_t* pools, uint8_t log2_size, uint8_t kind, size_t size)
{
    assert(log2_size >= HT_LOG_MINSIZE);
    assert(size == _htkeys_alloc_size(log2_size, kind));
    pool_t* pool = _htkeys_pool(pools, log2_size, kind);
    htkeys_t* keys = pool == NULL ? NULL : pool_pop(pool);
    if (keys == NULL) {
        keys = PyMem_Malloc(size);
        if (keys == NULL) {
            PyErr_NoMemory();
            return NULL;
        }
    }
    return keys;
}

static inline htkeys_t*
_htkeys_alloc_raw(pool_t* pools, uint8_t log2_size, uint8_t kind)
{
    return htkeys_alloc_sized(
        pools, log2_size, kind, _htkeys_alloc_size(log2_size, kind));
}

/* Zeroes the entries from `from` on. A caller that fills the front of
   the table itself needs this for the rest: ASSERT_CONSISTENT() reads
   every entry a table has room for, not just the used prefix. Out of line,
   like htkeys_new_unfilled(), so a COLD caller such as md_to_anystr(),
   built for size, calls this copy, whose memset is a library call: its own
   inlined one became rep stos, which costs a cycle per byte under the
   benchmarks' instruction counts. */
NOINLINE static void
htkeys_zero_entries(htkeys_t* keys, Py_ssize_t from)
{
    assert(from >= 0 && from <= keys->usable);
    memset(htkeys_entry_at(keys, htkeys_entries(keys), from),
           0,
           (size_t)(keys->usable - from) * htkeys_entry_size(keys));
}

/* An empty table whose entries are left as they came, for a caller that
   writes the front of the array itself and calls htkeys_zero_entries()
   for the rest. Nothing may read the table in between. Out of line; see
   htkeys_zero_entries(). */
NOINLINE static htkeys_t*
htkeys_new_unfilled(pool_t* pools, uint8_t log2_size, uint8_t kind)
{
    uint8_t log2_bytes = _htkeys_log2_index_bytes(log2_size);

    htkeys_t* keys = _htkeys_alloc_raw(pools, log2_size, kind);
    if (keys == NULL) {
        return NULL;
    }

    keys->log2_size = log2_size;
    keys->log2_index_bytes = log2_bytes;
    keys->kind = kind;
    keys->resume_slots = NULL;
    keys->nentries = 0;
    keys->usable = USABLE_FRACTION((Py_ssize_t)((size_t)1 << log2_size));
#ifdef Py_GIL_DISABLED
    keys->num_readers = 0;
    keys->retired_next = NULL;
#endif
    memset(&keys->indices[0], 0xff, ((size_t)1 << log2_bytes));
    return keys;
}

static inline htkeys_t*
htkeys_new(pool_t* pools, uint8_t log2_size, uint8_t kind)
{
    htkeys_t* keys = htkeys_new_unfilled(pools, log2_size, kind);
    if (keys != NULL) {
        htkeys_zero_entries(keys, 0);
    }
    return keys;
}

static inline void
htkeys_free(pool_t* pools, htkeys_t* dk)
{
    /* Always released, never pooled with the block: a pooled block must
       come back the way htkeys_new() leaves one, and resume_slots is
       only ever allocated well above the largest pooled class anyway. */
    if (dk->resume_slots != NULL) {
        PyMem_Free(dk->resume_slots);
    }
    pool_t* pool = _htkeys_pool(pools, dk->log2_size, dk->kind);
    if (pool == NULL || !pool_push(pool, dk)) {
        PyMem_Free(dk);
    }
}

/* Returns the identity's hash, or -1 if hashing raised. */
static inline Py_hash_t
unicode_hash(PyObject* o)
{
    assert(PyUnicode_CheckExact(o));
    PyASCIIObject* ascii = (PyASCIIObject*)o;
    /* Another thread may be filling in the cached hash concurrently. */
#ifdef Py_GIL_DISABLED
    Py_hash_t hash = atomic_load_ssize_relaxed(&ascii->hash);
#else
    Py_hash_t hash = ascii->hash;
#endif
    if (hash == -1) {
        hash = PyUnicode_Type.tp_hash(o);
        if (hash == -1) {
            return -1;
        }
    }
    return hash;
}

/* Values for the same key share a probe sequence, so without resume slots the
   n-th one walks past the n - 1 before it. Called once perturb is 0 and
   slot i is next to probe. Resume slots are only allocated once a probe here
   is long, so tables without long chains don't pay for them. */
COLD static Py_ssize_t
_htkeys_find_empty_slot_resume(htkeys_t* keys, size_t i)
{
    const size_t mask = (size_t)_htkeys_mask(keys);
    const size_t start = i;
    const bool small = keys->log2_size < 16;
    void* resume_slots = keys->resume_slots;
    if (resume_slots != NULL) {
        size_t resume = small ? ((uint16_t*)resume_slots)[start]
                              : ((uint32_t*)resume_slots)[start];
        if (resume != 0) {
            /* the resume slot is used, start after it */
            i = ((resume - 1) * 5 + 1) & mask;
        }
    }
    size_t steps = 0;
    while (htkeys_get_index(keys, (Py_ssize_t)i) != DKIX_EMPTY) {
        i = (i * 5 + 1) & mask;
        steps++;
    }
    if (resume_slots == NULL) {
        size_t nbytes = htkeys_resume_slots_bytes(keys->log2_size);
        if (steps < HT_RESUME_SLOTS_MIN_STEPS || nbytes == 0) {
            return (Py_ssize_t)i;
        }
        resume_slots = keys->resume_slots = PyMem_Malloc(nbytes);
        if (resume_slots == NULL) {
            return (Py_ssize_t)i;
        }
        memset(resume_slots, 0, nbytes);
    }
    if (small) {
        ((uint16_t*)resume_slots)[start] = (uint16_t)(i + 1);
    } else {
        ((uint32_t*)resume_slots)[start] = (uint32_t)(i + 1);
    }
    return (Py_ssize_t)i;
}

/*
Internal routine used by ht_resize() to build a hashtable of entries.
*/
ALWAYS_INLINE static inline void
_htkeys_build_indices(htkeys_t* keys, bool ci, entry_t* ep, Py_ssize_t n,
                      bool skip_holes)
{
    size_t mask = (size_t)_htkeys_mask(keys);
    if (keys->resume_slots != NULL) {
        memset(
            keys->resume_slots, 0, htkeys_resume_slots_bytes(keys->log2_size));
    }
    for (Py_ssize_t ix = 0; ix != n; ix++, ep = htkeys_entry_next(keys, ep)) {
        if (skip_holes && entry_is_hole(keys->kind, ep)) {
            continue;
        }
        Py_hash_t hash = entry_hash(keys->kind, ci, ep);
        size_t i = (size_t)hash & mask;
        for (size_t perturb = (size_t)hash;
             htkeys_get_index(keys, (Py_ssize_t)i) != DKIX_EMPTY;) {
            perturb >>= HT_PERTURB_SHIFT;
            i = mask & (i * 5 + perturb + 1);
            if (UNLIKELY(perturb == 0)) {
                i = (size_t)_htkeys_find_empty_slot_resume(keys, i);
                break;
            }
        }
        htkeys_set_index(keys, (Py_ssize_t)i, ix);
    }
}

static inline void
htkeys_build_indices(htkeys_t* keys, bool ci, entry_t* ep, Py_ssize_t n)
{
    _htkeys_build_indices(keys, ci, ep, n, false);
}

/* Leaves the deleted entries among `ep` (a NULL identity) unindexed. */
static inline void
htkeys_build_indices_with_holes(htkeys_t* keys, bool ci, entry_t* ep,
                                Py_ssize_t n)
{
    _htkeys_build_indices(keys, ci, ep, n, true);
}

/* Uses keys, mask, i and perturb from the caller and returns. */
#define _HT_FIND_EMPTY_SLOT(size)                                        \
    while (htkeys_load_index##size(keys, (Py_ssize_t)i) != DKIX_EMPTY) { \
        perturb >>= HT_PERTURB_SHIFT;                                    \
        i = (i * 5 + perturb + 1) & mask;                                \
        if (UNLIKELY(perturb == 0)) {                                    \
            return _htkeys_find_empty_slot_resume(keys, i);              \
        }                                                                \
    }                                                                    \
    return (Py_ssize_t)i;

/* Internal function to find slot for an item from its hash
   when it is known that the key is not present in the dict.
   The caller must fill the returned slot, resume slots skip past it.

   Unswitched by index size by hand so the perturb check stays cheap.
 */
static inline Py_ssize_t
htkeys_find_empty_slot(htkeys_t* keys, Py_hash_t hash)
{
    const size_t mask = (size_t)_htkeys_mask(keys);
    size_t i = (size_t)hash & mask;
    size_t perturb = (size_t)hash;
    uint8_t log2size = keys->log2_size;
    if (log2size < 8) {
        while (htkeys_load_index8(keys, (Py_ssize_t)i) != DKIX_EMPTY) {
            perturb >>= HT_PERTURB_SHIFT;
            i = (i * 5 + perturb + 1) & mask;
        }
        return (Py_ssize_t)i;
    } else if (log2size < 16) {
        _HT_FIND_EMPTY_SLOT(16)
    }
#if SIZEOF_VOID_P > 4
    else if (log2size >= 32) {
        _HT_FIND_EMPTY_SLOT(64)
    }
#endif
    else {
        _HT_FIND_EMPTY_SLOT(32)
    }
}

#undef _HT_FIND_EMPTY_SLOT

/* Iterator over slots/indexes for given hash.
   N.B. The iterator MIGHT return the same slot
   multiple times, eiter consequently (1, 2, 2, 3)
   or with different slots in the middle (1, 2, 3, 1).

   The caller is responsible for skipping repeats; md_walk() in
   walk.h does it with a bitmap of visited entries.
*/

typedef struct _htkeysiter {
    htkeys_t* keys;
    size_t mask;  // _htkeys_mask(keys)
    size_t slot;  // masked hash, Py_hash_t h & mask;
    size_t perturb;
    Py_ssize_t index;
} htkeysiter_t;

/* Always inlined: left to itself GCC emits it out of line, and then
   every probe in the extension opens with a call for five stores. */
ALWAYS_INLINE static inline void
htkeysiter_init(htkeysiter_t* iter, htkeys_t* keys, Py_hash_t hash)
{
    iter->keys = keys;
    iter->mask = (size_t)_htkeys_mask(keys);
    iter->perturb = (size_t)hash;
    iter->slot = (size_t)hash & iter->mask;
    iter->index = htkeys_get_index(iter->keys, (Py_ssize_t)iter->slot);
}

static inline void
htkeysiter_next(htkeysiter_t* iter)
{
    iter->perturb >>= HT_PERTURB_SHIFT;
    iter->slot = (iter->slot * 5 + iter->perturb + 1) & iter->mask;
    iter->index = htkeys_get_index(iter->keys, (Py_ssize_t)iter->slot);
}

#ifdef __cplusplus
}
#endif
#endif
