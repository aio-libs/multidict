#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_HASHTABLE_H
#define _MULTIDICT_HASHTABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "atomic_helpers.h"
#include "bitmap.h"
#include "compiler.h"
#include "debug.h"
#include "dict.h"
#include "freethreading.h"
#include "htkeys.h"
#include "identity.h"
#include "istr.h"
#include "reflist.h"
#include "state.h"
#include "update_marks.h"
#include "walk.h"
#include "watch.h"

#define MD_POOLS(md) ((md)->state->htkeys_pools)

/* The kind a new, empty table of md starts with. */
static inline uint8_t
md_fresh_kind(const MultiDictObject* md)
{
    /* A table starts compact and moves to KIND_ANYSTR on the first key
       that does not fit (see md_key_fits()). */
    return KIND_COMPACT;
}

/* The kind of a table replacing `old`: a table with no entries yet starts
   afresh, so a clear() lets md return to the compact kind. */
static inline uint8_t
md_next_kind(const MultiDictObject* md, const htkeys_t* old)
{
    return old->nentries == 0 ? md_fresh_kind(md) : old->kind;
}

/* Whether a compact table of md can hold `key`: a MultiDict's key must be
   its own identity, a CIMultiDict's an exact istr, whose identity is then
   its canonical form. */
ALWAYS_INLINE static inline bool
md_key_fits(const MultiDictObject* md, PyObject* key, PyObject* identity,
            bool ci)
{
    assert(ci == md->is_ci);
    if (ci) {
        assert(!IStr_CheckExact(md->state, key) ||
               istr_canonical(key) == identity);
        return IStr_CheckExact(md->state, key);
    }
    return key == identity;
}

typedef struct _md_pos {
    Py_ssize_t pos;
    uint64_t version;
} md_pos_t;

/*
The multidict's implementation is close to Python's dict except for multiple
keys.

It starts from the empty hashtable, which grows by a power of 2 starting from
8: 8, 16, 32, 64, 128, ...  The amount of items is 2/3 of the hashtable size
(1/3 of the table is never allocated).

The table is resized if needed, and bulk updates (extend(), update(), and
constructor calls) pre-allocate many items at once, reducing the amount of
potential hashtable resizes.

Item deletion puts DKIX_DUMMY special index in the hashtable. In opposite to
the standard dict, DKIX_DUMMY is never replaced with an index of the new entry
except by hashtable indices rebuild. It allows to keep the insertion order for
multiple equal keys. The index table rebuild happens on the keys table size
changeing and if the number of DKIX_DUMMY slots grows to 1/4 of the total
amount.

The iteration for operations like getall() is a little tricky. The next index
calculation could return the already visited index before reaching the end. To
eliminate duplicates, the code records visited entry indices in a bitmap
private to the walk (see bitmap.h); the table itself is never marked. Double
iteration over the indices still has O(1) amortized time, it is ok.

`.add()`, `val = md[key]`, `md[key] = val`, `md.setdefault()` all have O(1).
`.getall()` / `.popall()` have O(N) where N is the amount of returned items.
`.update()` / `extend()` have O(N+M) where N and M are amount of items
in the left and right arguments.

`.copy()` and constuction from multidict is super fast.
*/

/* GROWTH_RATE. Growth rate upon hitting maximum load.
 * Currently set to used*3.
 * This means that dicts double in size when growing without deletions,
 * but have more head room when the number of deletions is on a par with the
 * number of insertions.  See also bpo-17563 and bpo-33205.
 *
 * GROWTH_RATE was set to used*4 up to version 3.2.
 * GROWTH_RATE was set to used*2 in version 3.3.0
 * GROWTH_RATE was set to used*2 + capacity/2 in 3.4.0-3.6.0.
 */
static inline Py_ssize_t
GROWTH_RATE(const MultiDictObject* md)
{
    return md->used * 3;
}

/* Frees a table no longer published, releasing the references its
   entries still hold: only md_clear()'s and _md_install_keys()'s
   tables have any, since _md_rebuild() moves its old table's entries
   and zeroes nentries. */
NOINLINE static void
_htkeys_dispose(pool_t* pools, htkeys_t* keys)
{
    /* Nothing can reach the table any more, so a finalizer run by a decref
       cannot see the stale pointers: no need to clear them, nor to reload
       nentries after each call. */
    entry_t* entry = htkeys_entries(keys);
    Py_ssize_t nentries = keys->nentries;
    if (kind_is_compact(keys->kind)) {
        for (Py_ssize_t i = 0; i < nentries;
             i++, entry = entry_next(KIND_COMPACT, entry)) {
            Py_XDECREF(entry->key);  // also owns the identity
            Py_XDECREF(entry->value);
        }
    } else {
        for (Py_ssize_t i = 0; i < nentries;
             i++, entry = entry_next(KIND_ANYSTR, entry)) {
            Py_XDECREF(as_anystr(entry)->identity);
            Py_XDECREF(entry->key);
            Py_XDECREF(entry->value);
        }
    }
    htkeys_free(pools, keys);
}

#ifdef Py_GIL_DISABLED

/*
Lock-free read support.

md->num_active_readers is a coarse "some lock-free reader is in flight on
this object" gate: incremented before a reader ever dereferences a
keys table, decremented once it's done. A table is only ever freed
once this reads 0 at a point synchronized (seq_cst on both sides, see
atomic_helpers.h) with the swap that retired it. That ordering is the
safety argument in full:

  reader:  num_active_readers += 1        (A, seq_cst)
           keys = load(md->keys)      (B, seq_cst)
           keys->num_readers += 1         (C, relaxed; skipped for
                                        &empty_htkeys, which is never
                                        retired or freed)
           ... walk keys ...
           keys->num_readers -= 1         (D, release)
           num_active_readers -= 1        (E, seq_cst)

  writer:  store(md->keys, new)       (seq_cst)
           if load(num_active_readers) == 0 (seq_cst): free old immediately
           else: move old onto md->retired, freed by a later drain

A is always sequenced-before B on the reader's own thread, so if the
writer's check observes num_active_readers == 0, no reader can be
*starting* a walk of any table that was retired before that check --
none can be caught between A and B for the table being freed. Anything
weaker than seq_cst here (plain acquire/release, or relaxed) is not
enough: the writer's store to md->keys and its read of num_active_readers,
versus the reader's write to num_active_readers and its read of
md->keys, is a criss-cross on two independent atomics (the same shape
as Dekker's algorithm), and only a single global seq_cst order over all
four operations closes it -- see the design discussion that produced
this file for the specific interleaving that a weaker order permits.

That guarantee is coarser than it looks, though: num_active_readers
reaching zero does not mean every reader that incremented it has also
reached its own D/E -- a reader can be preempted between C and D for
an arbitrary stretch. So keys->num_readers (C/D) is the actual
per-table authority on whether a specific table is safe to free, not
a redundant check of what the coarse gate already guarantees.
_md_drain_retired_slow() treats it that way: a table whose own num_readers
is still nonzero is pushed back onto md->retired for a later attempt
instead of freed.

Unlike A/B/E above, C/D is not a criss-cross between two independent
atomics -- it is a one-directional handoff: a reader finishes reading
this specific table's fields, then signals "done" via D; the drainer
reads that signal and, once it sees zero, may free the table. That is
the textbook release/acquire pattern, not Dekker's algorithm, and
release/acquire is sufficient (seq_cst is not required): D is a
release atomic_fetch_add_ssize_release(), so every ordinary read the
reader performed while walking keys (in _md_get_one_lockfree() and
friends) is ordered-before D becomes visible to another thread. The
drain's own check, atomic_load_ssize_acquire(&t->num_readers) == 0 in
_md_drain_retired_slow(), pairs with that release: observing the
post-decrement value there means the drainer also observes everything
the departing reader read before D, so freeing the table (via
htkeys_free(), reached through _htkeys_dispose()) cannot race the
reader's now-finished walk. C itself (the increment in
_md_reader_enter()) stays a plain relaxed
atomic_fetch_add_ssize_relaxed(): no other thread's correctness
depends on observing the increment's ordering relative to any other
memory location -- only the decrement side of the handoff needs to
publish anything.

_md_reader_exit()'s own num_active_readers decrement (E above) is a
seq_cst atomic_fetch_add_ssize(), which hands back the pre-decrement
count for free. When that count was 1, this reader's decrement is the
one that brings the global gate to 0, at the exact same linearization
point a writer's atomic_load_ssize(&md->num_active_readers) == 0 check
would observe. The safety argument above never distinguished who
performs that check; it only depends on num_active_readers reaching 0
under seq_cst. So the reader may drain md->retired right there instead
of leaving every table on it stranded until some future writer happens
to retire another one and observe the same zero (see md->retired below
for why a concurrent drain from this path is safe against a writer
retiring into the same list at the same time).

md->retired is a lock-free stack (Treiber-style), not a plain
writer-owned list: with readers now able to drain it too, pushes
(_md_retire()) and pop-alls (_md_drain_retired_slow()) can run concurrently
with each other, on different threads, with no lock in common. A
pop-all is a single atomic_exchange_ptr() that swaps the whole chain
out for NULL and hands the caller sole ownership of whatever it
returns; any push racing that exchange either lands before it (and
gets swept up in the same pop) or after it (and starts a fresh chain
from NULL), never in between, because there is no "in between" for a
single atomic RMW. A push cannot use that same trick: it has to link
the new node's ->retired_next to the current head before publishing
the node, and if it read that head with a plain load, a pop-all could
slip in after the load and free the very chain the push is about to
link to, publishing a node whose ->retired_next dangles. The
atomic_compare_exchange_ptr() loop in _md_retire() closes that window
instead of merely narrowing it: the head is only published once the
CAS confirms nothing changed it since the read that fed
->retired_next, and if something did (a pop-all ran, or another
push), the loop rereads the new head and relinks before retrying, so
->retired_next is never stale at the moment the node actually becomes
visible.
*/

static inline htkeys_t*
_md_reader_enter(MultiDictObject* md)
{
    atomic_fetch_add_ssize(&md->num_active_readers, 1);
    htkeys_t* keys = (htkeys_t*)atomic_load_ptr((void* const*)&md->keys);
    if (keys != &empty_htkeys) {
        atomic_fetch_add_ssize_relaxed(&keys->num_readers, 1);
    }
    return keys;
}

static inline void
_md_drain_retired(MultiDictObject* md);

static inline void
_md_reader_exit(MultiDictObject* md, htkeys_t* keys)
{
    if (keys != &empty_htkeys) {
        atomic_fetch_add_ssize_release(&keys->num_readers, -1);
    }
    Py_ssize_t prev_active_readers =
        atomic_fetch_add_ssize(&md->num_active_readers, -1);
    if (prev_active_readers == 1) {
        _md_drain_retired(md);
    }
}

NOINLINE static void
_md_drain_retired_slow(MultiDictObject* md)
{
retry:
    if (atomic_load_ssize(&md->num_active_readers) != 0) {
        return;
    }
    htkeys_t* t = (htkeys_t*)atomic_exchange_ptr((void**)&md->retired, NULL);

    /* The check above says nothing about a table retired after it: a
       reader may have loaded it as md->keys but not yet counted itself
       in its num_readers. Every reader of a table taken here entered
       before that table's retirement, so a zero read now means each of
       them has also exited. */
    bool readers_active = atomic_load_ssize(&md->num_active_readers) != 0;

    /* The coarse gate above can read zero while a specific table's own
       num_readers is still nonzero -- a reader can be preempted between
       incrementing it and decrementing it. A table is only actually
       safe to free once its own count is zero, so anything still
       nonzero goes back onto md->retired for a later attempt instead of
       being freed here. */
    htkeys_t* pending_head = NULL;
    htkeys_t* pending_tail = NULL;
    while (t != NULL) {
        htkeys_t* next = t->retired_next;
        if (!readers_active &&
            atomic_load_ssize_acquire(&t->num_readers) == 0) {
            _htkeys_dispose(MD_POOLS(md), t);
        } else {
            t->retired_next = pending_head;
            pending_head = t;
            if (pending_tail == NULL) {
                pending_tail = t;
            }
        }
        t = next;
    }

    if (pending_head != NULL) {
        htkeys_t* old_head =
            (htkeys_t*)atomic_load_ptr((void* const*)&md->retired);
        for (;;) {
            pending_tail->retired_next = old_head;
            if (atomic_compare_exchange_ptr(
                    (void**)&md->retired, (void**)&old_head, pending_head)) {
                break;
            }
        }
        /* Pushing back leaves the tables to the reader the gate showed, but
           that reader can have looked already, between the exchange above
           and this push-back, and found the list empty. Rereading the gate
           is what tells the two apart: nonzero means the reader it sees
           decrements after this read, so after the push-back, and its own
           drain observes them; zero means nothing else will. Each pass
           either frees or leaves a reader for that read to find, so this
           does not spin. */
        goto retry;
    }
}

/* Every reader that brings num_active_readers back to zero drains, which
   without a second thread is every lookup, and the list is almost always
   empty; the work it guards sits behind a call so that a lookup pays a load
   instead. The load stays seq_cst, not relaxed: a writer that pushes and
   then reads num_active_readers as nonzero leaves the table for whoever
   brings that count to zero, so the reader doing so must not be able to
   miss the push. Both are seq_cst, so the push precedes the writer's read,
   which precedes this reader's decrement, which precedes this load in the
   single total order -- a relaxed load here has no such guarantee, and the
   table would be stranded until md's next operation. On x86-64 a seq_cst
   load is a plain mov; the cost this removes is the exchange below it. */
static inline void
_md_drain_retired(MultiDictObject* md)
{
    if (atomic_load_ptr((void* const*)&md->retired) == NULL) {
        return;
    }
    _md_drain_retired_slow(md);
}

static inline void
_md_retire(MultiDictObject* md, htkeys_t* keys)
{
    if (keys == &empty_htkeys) {
        return;
    }
    _md_drain_retired(md);

    htkeys_t* old_head =
        (htkeys_t*)atomic_load_ptr((void* const*)&md->retired);
    for (;;) {
        keys->retired_next = old_head;
        if (atomic_compare_exchange_ptr(
                (void**)&md->retired, (void**)&old_head, keys)) {
            break;
        }
    }
    _md_drain_retired(md);
}

#endif /* Py_GIL_DISABLED */

/* Drops a table md no longer publishes: at once on GIL builds, once no
   lock-free reader can still reach it on free-threaded ones. */
static inline void
_md_release_keys(MultiDictObject* md, htkeys_t* keys)
{
#ifdef Py_GIL_DISABLED
    _md_retire(md, keys);
#else
    if (keys != &empty_htkeys) {
        _htkeys_dispose(MD_POOLS(md), keys);
    }
#endif
}

/* Publishes a rebuilt table: the old one's entries have all moved over. */
ALWAYS_INLINE static inline void
_md_publish_rebuilt(MultiDictObject* md, htkeys_t* oldkeys, htkeys_t* newkeys)
{
    store_keys(md, newkeys);
    update_marks_moved(md);

#ifdef Py_GIL_DISABLED
    /* Bump the version on every resize, not just when a caller's
       own insert/delete/replace would bump it anyway: a freed
       htkeys_t can get reallocated at the very same address by a
       later resize (same size class, common in practice), so code
       elsewhere that detects "did md->keys change under me" by
       comparing the raw pointer alone (see _md_replace_locked()'s
       comment) needs a companion signal that can't coincidentally
       repeat. */
    bump_version(md);
#endif

    /* Ownership of oldkeys's entries has already moved to newkeys;
       zeroing nentries tells _htkeys_dispose() there is nothing left to
       decref, only memory to free. */
    if (oldkeys != &empty_htkeys) {
        oldkeys->nentries = 0;
    }
    _md_release_keys(md, oldkeys);

    ASSERT_CONSISTENT(md);
}

/* _md_rebuild() while an update() or merge() is in flight: every entry
   keeps its index, holes and all, since the batch's marks name entries by
   index; see update_marks.h. */
COLD static int
_md_rebuild_keeping_indices(MultiDictObject* md, uint8_t log2_newsize)
{
    htkeys_t* oldkeys = md->keys;
    Py_ssize_t nentries = oldkeys->nentries;
    if (!htkeys_size_fits(log2_newsize)) {
        PyErr_NoMemory();
        return -1;
    }
    // the room the caller asked for, on top of the holes
    Py_ssize_t want =
        USABLE_FRACTION((Py_ssize_t)1 << log2_newsize) - md->used + nentries;
    while (USABLE_FRACTION((Py_ssize_t)1 << log2_newsize) < want) {
        log2_newsize++;
        if (!htkeys_size_fits(log2_newsize)) {
            PyErr_NoMemory();
            return -1;
        }
    }

    htkeys_t* newkeys = htkeys_new_unfilled(
        MD_POOLS(md), log2_newsize, md_next_kind(md, oldkeys));
    if (newkeys == NULL) {
        return -1;
    }
    entry_t* newentries = htkeys_entries(newkeys);
    memcpy(newentries,
           htkeys_entries(oldkeys),
           (size_t)nentries * htkeys_entry_size(oldkeys));
    htkeys_zero_entries(newkeys, nentries);
    htkeys_build_indices_with_holes(newkeys, md->is_ci, newentries, nentries);
    newkeys->usable -= nentries;
    newkeys->nentries = nentries;

    _md_publish_rebuilt(md, oldkeys, newkeys);
    return 0;
}

/* A table for md's new contents while an update() or merge() is in
   flight: it starts with as many holes as md has entries now, so the new
   entries take indices none of the batch's marks name. Holds `extra` more.
   */
COLD static htkeys_t*
_md_new_keys_after_holes(MultiDictObject* md, Py_ssize_t extra, uint8_t kind)
{
    Py_ssize_t nholes = md->keys->nentries;
    uint8_t log2_size = estimate_log2_keysize(nholes + extra);
    if (!htkeys_size_fits(log2_size)) {
        PyErr_NoMemory();
        return NULL;
    }
    htkeys_t* keys = htkeys_new(MD_POOLS(md), log2_size, kind);
    if (keys == NULL) {
        return NULL;
    }
    keys->usable -= nholes;
    keys->nentries = nholes;
    return keys;
}

NOINLINE static int
_md_rebuild(MultiDictObject* md, uint8_t log2_newsize)
{
    if (UNLIKELY(md->batches != 0)) {
        return _md_rebuild_keeping_indices(md, log2_newsize);
    }
    if (!htkeys_size_fits(log2_newsize)) {
        PyErr_NoMemory();
        return -1;
    }
    assert(log2_newsize >= HT_LOG_MINSIZE);

    /* The copy below writes the front of the entries array, so only
       what it leaves over has to be zeroed. */
    htkeys_t* newkeys = htkeys_new_unfilled(
        MD_POOLS(md), log2_newsize, md_next_kind(md, md->keys));
    if (newkeys == NULL) {
        return -1;
    }

    htkeys_t* oldkeys = md->keys;
    Py_ssize_t numentries = md->used;
    entry_t* oldentries = htkeys_entries(oldkeys);
    uint8_t kind = oldkeys->kind;
    entry_t* newentries = htkeys_entries(newkeys);
    Py_ssize_t filled;
    if (oldkeys->nentries == numentries) {
        memcpy(newentries,
               oldentries,
               (size_t)numentries * htkeys_entry_size(oldkeys));
        filled = numentries;
    } else {
        entry_t* new_ep = newentries;
        entry_t* old_ep = oldentries;
        Py_ssize_t oldnumentries = oldkeys->nentries;
        for (Py_ssize_t i = 0; i < oldnumentries;
             ++i, old_ep = entry_next(kind, old_ep)) {
            if (!entry_is_hole(kind, old_ep)) {
                htkeys_entry_copy(newkeys, new_ep, old_ep);
                new_ep = htkeys_entry_next(newkeys, new_ep);
            }
        }
        filled = htkeys_entry_index(newkeys, newentries, new_ep);
    }
    /* What the copy actually wrote, rather than md->used: the two agree,
       but taking the count from the copy means a table can never be
       published over entries nothing has written. */
    assert(filled == numentries);
    htkeys_zero_entries(newkeys, filled);

    htkeys_build_indices(newkeys, md->is_ci, newentries, numentries);

    newkeys->usable = newkeys->usable - numentries;
    newkeys->nentries = numentries;

    _md_publish_rebuilt(md, oldkeys, newkeys);
    return 0;
}

// Out of line: inlined, it slows construction and update() on GIL builds
NOINLINE static int
_md_resize_for_add(MultiDictObject* md)
{
    return _md_rebuild(md, calculate_log2_keysize(GROWTH_RATE(md)));
}

static inline int
md_reserve(MultiDictObject* md, Py_ssize_t extra_size)
{
    if (extra_size > (PY_SSIZE_T_MAX - 1) / 3 - md->used) {
        /* Only a __length_hint__ can claim this much; ignore it, as
           list.extend() does, rather than overflow the estimate. */
        return 0;
    }
    if (md->keys->usable >= extra_size) {
        return 0;
    }
    /* Sized by live entries, so a table short of room only because of
       deleted ones is compacted, or even shrunk, rather than grown. */
    return _md_rebuild(md, estimate_log2_keysize(extra_size + md->used));
}

/* md_reserve() into a KIND_ANYSTR table: grows and moves a compact table
   in one rebuild, where md_reserve() and then md_to_anystr() would copy
   it twice. Holes are dropped, so no batch may be in flight. */
NOINLINE static int
_md_rebuild_to_anystr(MultiDictObject* md, uint8_t log2_newsize)
{
    htkeys_t* oldkeys = md->keys;
    assert(md->batches == 0 && kind_is_compact(oldkeys->kind));
    if (!htkeys_size_fits(log2_newsize)) {
        PyErr_NoMemory();
        return -1;
    }
    htkeys_t* newkeys =
        htkeys_new_unfilled(MD_POOLS(md), log2_newsize, KIND_ANYSTR);
    if (newkeys == NULL) {
        return -1;
    }
    entry_t* src = htkeys_entries(oldkeys);
    entry_t* newentries = htkeys_entries(newkeys);
    anystr_entry_t* dst = as_anystr(newentries);
    for (Py_ssize_t i = 0; i < oldkeys->nentries;
         i++, src = entry_next(KIND_COMPACT, src)) {
        PyObject* key = src->key;
        if (key == NULL) {
            continue;
        }
        dst->identity = Py_NewRef(compact_key_identity(md->is_ci, key));
        dst->hash = compact_key_hash(md->is_ci, key);
        dst->base.key = key;
        dst->base.value = src->value;
        dst++;
    }
    Py_ssize_t numentries = md->used;
    assert(htkeys_entry_index(newkeys, newentries, &dst->base) == numentries);
    htkeys_zero_entries(newkeys, numentries);
    htkeys_build_indices(newkeys, md->is_ci, newentries, numentries);
    newkeys->usable -= numentries;
    newkeys->nentries = numentries;
    _md_publish_rebuilt(md, oldkeys, newkeys);
    return 0;
}

/* md_reserve() for extend(), update() or merge(), before their batch
   starts. Keyword names are plain str, which never fit a CIMultiDict's
   compact table, so with any the table is moved while it grows; moved
   later, it cost update(istr_items, **kwargs) a second copy, 18%. */
static inline int
md_reserve_batch(MultiDictObject* md, Py_ssize_t extra_size, bool kwargs)
{
    if (UNLIKELY(kwargs) && md->is_ci && kind_is_compact(md->keys->kind) &&
        md->used > 0 && md->batches == 0 &&
        extra_size <= (PY_SSIZE_T_MAX - 1) / 3 - md->used) {
        return _md_rebuild_to_anystr(
            md, estimate_log2_keysize(extra_size + md->used));
    }
    return md_reserve(md, extra_size);
}

/* Publishes md's replacement table, then drops the one it replaced,
   which a fresh shell does not have. The drop comes last because its
   decrefs can run a __del__ that reads or mutates md: publishing first
   means that code finds md already holding its new contents, and what
   it adds stays in md rather than being overwritten. */
static inline void
_md_install_keys(MultiDictObject* md, htkeys_t* keys, Py_ssize_t used,
                 bool is_ci, MultiDict_WatchEvent event)
{
    htkeys_t* old_keys = md->keys;
    store_used(md, used);
    bump_version(md);
    /* Lock-free readers of a live md read is_ci, so a re-init that
       keeps it must not store it. */
    if (md->is_ci != is_ci) {
        md->is_ci = is_ci;
    }
    store_keys(md, keys);
    md_watch_record_simple(md, event);
    ASSERT_CONSISTENT(md);
    if (old_keys != NULL) {
        update_marks_moved(md);
        _md_release_keys(md, old_keys);
    }
}

static inline int
md_init(MultiDictObject* md, bool is_ci, Py_ssize_t minused, uint8_t kind)
{
    assert(md->state != NULL);
    htkeys_t* new_keys = (htkeys_t*)&empty_htkeys;

    if (UNLIKELY(md->batches != 0)) {
        new_keys = _md_new_keys_after_holes(md, minused, kind);
        if (new_keys == NULL) {
            return -1;
        }
    } else if (minused > USABLE_FRACTION(HT_MINSIZE)) {
        const uint8_t log2_max_presize = 17;
        const Py_ssize_t max_presize = ((Py_ssize_t)1) << log2_max_presize;
        uint8_t log2_newsize;
        /* There are no strict guarantee that returned dict can contain minused
         * items without resize.  So we create medium size dict instead of very
         * large dict or MemoryError.
         */
        if (minused > USABLE_FRACTION(max_presize)) {
            log2_newsize = log2_max_presize;
        } else {
            log2_newsize = estimate_log2_keysize(minused);
        }

        new_keys = htkeys_new(MD_POOLS(md), log2_newsize, kind);
        if (new_keys == NULL) return -1;
    } else if (minused > 0 && kind != md_fresh_kind(md)) {
        /* The first insert would allocate this table anyway, but of the
           fresh kind, only to move it at once. */
        new_keys = htkeys_new(MD_POOLS(md), HT_LOG_MINSIZE, kind);
        if (new_keys == NULL) return -1;
    }

    _md_install_keys(md, new_keys, 0, is_ci, MultiDict_EVENT_CLEARED);
    return 0;
}

/* The kind a CIMultiDict's table starts with when `key` (borrowed, or NULL
   if unknown) is the first key it will get: pre-sizing a compact table
   for a str key would only have it rebuilt at once. */
static inline uint8_t
md_ci_kind_for_first_key(mod_state* state, PyObject* key)
{
    return key == NULL || IStr_CheckExact(state, key) ? KIND_COMPACT
                                                      : KIND_ANYSTR;
}

/* md_clone_from_ht() while an update() or merge() on md is in flight:
   other's entries go after the holes _md_new_keys_after_holes() leaves. */
COLD static int
_md_clone_after_holes(MultiDictObject* md, MultiDictObject* other)
{
    // the holes keep md's entries' indices; other's entries may need more
    htkeys_t* keys = _md_new_keys_after_holes(md, other->used, KIND_ANYSTR);
    if (keys == NULL) {
        return -1;
    }
    entry_t* entries = htkeys_entries(keys);
    anystr_entry_t* dst = as_anystr(entries) + keys->nentries;
    entry_t* src = htkeys_entries(other->keys);
    for (Py_ssize_t i = 0; i < other->keys->nentries;
         i++, src = htkeys_entry_next(other->keys, src)) {
        if (!entry_is_hole(other->keys->kind, src)) {
            dst->identity = Py_NewRef(
                entry_identity(other->keys->kind, other->is_ci, src));
            dst->base.key = Py_NewRef(src->key);
            dst->base.value = Py_NewRef(src->value);
            dst->hash = entry_hash(other->keys->kind, other->is_ci, src);
            dst++;
        }
    }
    Py_ssize_t nentries = htkeys_entry_index(keys, entries, &dst->base);
    keys->usable -= nentries - keys->nentries;
    keys->nentries = nentries;
    htkeys_build_indices_with_holes(keys, md->is_ci, entries, nentries);
    _md_install_keys(
        md, keys, other->used, other->is_ci, MultiDict_EVENT_CLONED);
    return 0;
}

static inline int
md_clone_from_ht(MultiDictObject* md, MultiDictObject* other)
{
    ASSERT_CONSISTENT(other);
    if (UNLIKELY(md->batches != 0)) {
        return _md_clone_after_holes(md, other);
    }

    htkeys_t* keys = (htkeys_t*)&empty_htkeys;
    htkeys_t* src = other->keys;
    if (src != &empty_htkeys) {
        /* The copy overwrites every byte, so this skips both of the
           memsets htkeys_new() would do; the byte count is a function
           of log2_size alone, which is also what the pool keys on. */
        size_t size = (size_t)htkeys_sizeof(src);
        keys =
            htkeys_alloc_sized(MD_POOLS(md), src->log2_size, src->kind, size);
        if (keys == NULL) {
            return -1;
        }

#ifdef Py_GIL_DISABLED
        /* Lock-free readers of other update src->num_readers while this
           runs, so the copy skips the reader fields instead of racing
           them. */
        memcpy(keys, src, offsetof(htkeys_t, num_readers));
        keys->num_readers = 0;
        keys->retired_next = NULL;
        memcpy(
            keys->indices, src->indices, size - offsetof(htkeys_t, indices));
#else
        memcpy(keys, src, size);
#endif
        keys->resume_slots = NULL;
        entry_t* entry = htkeys_entries(keys);
        if (kind_is_compact(keys->kind)) {
            for (Py_ssize_t idx = 0; idx < keys->nentries;
                 idx++, entry = entry_next(KIND_COMPACT, entry)) {
                Py_XINCREF(entry->key);  // also owns the identity
                Py_XINCREF(entry->value);
            }
        } else {
            for (Py_ssize_t idx = 0; idx < keys->nentries;
                 idx++, entry = entry_next(KIND_ANYSTR, entry)) {
                Py_XINCREF(as_anystr(entry)->identity);
                Py_XINCREF(entry->key);
                Py_XINCREF(entry->value);
            }
        }
    }

    /* No allocation happens between here and the writes to md below, so
       this snapshot of other's remaining fields is consistent with the
       keys buffer just copied above. */
    Py_ssize_t used = other->used;
    bool is_ci = other->is_ci;

    // Bumps md's own version: never reuse other's
    _md_install_keys(md, keys, used, is_ci, MultiDict_EVENT_CLONED);
    return 0;
}

static inline Py_ssize_t
md_len(MultiDictObject* md)
{
    return load_used(md);
}

/* Fill the entry past every live one, which the caller has indexed.
   identity is published last: it's the field a lock-free reader checks
   first (before ever touching hash/key/value), treating NULL as "not
   populated yet, keep probing". See the comment above load_identity().
   The GIL build has no reader to order against, so it just follows
   along. The entry is carved out of the zeroed tail, so value is still
   NULL and publish_value() is enough; nothing here has an old reference
   to drop. */
ALWAYS_INLINE static inline void
_md_fill_anystr_entry(htkeys_t* keys, Py_hash_t hash, PyObject* identity,
                      PyObject* key, PyObject* value)
{
    entry_t* entry =
        entry_at(KIND_ANYSTR, htkeys_entries(keys), keys->nentries);
    assert(as_anystr(entry)->identity == NULL && entry->key == NULL &&
           entry->value == NULL);
    entry->key = key;
    store_hash(KIND_ANYSTR, entry, hash);
    publish_value(entry, value);
    publish_identity(KIND_ANYSTR, entry, identity);
}

/* For both compact kinds: the key's reference keeps the identity alive,
   being either the key itself or the istr's canonical form. */
ALWAYS_INLINE static inline void
_md_fill_str_entry(htkeys_t* keys, PyObject* identity, PyObject* key,
                   PyObject* value)
{
    entry_t* entry =
        entry_at(KIND_COMPACT, htkeys_entries(keys), keys->nentries);
    assert(entry->key == NULL && entry->value == NULL);
    publish_value(entry, value);
    publish_identity(KIND_COMPACT, entry, key);
    Py_DECREF(identity);
}

/* Moves md to a KIND_ANYSTR table of the same size, for a key that does
   not fit a compact one (see md_key_fits()). Every entry keeps its index,
   so iterators and an update()'s marks stay valid. */
COLD static int
md_to_anystr(MultiDictObject* md)
{
    htkeys_t* oldkeys = md->keys;
    assert(kind_is_compact(oldkeys->kind) && oldkeys != &empty_htkeys);
    htkeys_t* newkeys =
        htkeys_new_unfilled(MD_POOLS(md), oldkeys->log2_size, KIND_ANYSTR);
    if (newkeys == NULL) {
        return -1;
    }
    memcpy(newkeys->indices,
           oldkeys->indices,
           (size_t)1 << oldkeys->log2_index_bytes);
    entry_t* src = htkeys_entries(oldkeys);
    anystr_entry_t* dst = as_anystr(htkeys_entries(newkeys));
    for (Py_ssize_t i = 0; i < oldkeys->nentries;
         i++, src = entry_next(KIND_COMPACT, src), dst++) {
        PyObject* key = src->key;
        dst->base.key = key;
        dst->base.value = src->value;
        if (key == NULL) {
            dst->identity = NULL;
            dst->hash = 0;
        } else {
            dst->identity = Py_NewRef(compact_key_identity(md->is_ci, key));
            dst->hash = compact_key_hash(md->is_ci, key);
        }
    }
    htkeys_zero_entries(newkeys, oldkeys->nentries);
    newkeys->nentries = oldkeys->nentries;
    newkeys->usable = oldkeys->usable;
    _md_publish_rebuilt(md, oldkeys, newkeys);
    return 0;
}

/* Stores a new entry at the end of md's table, after moving the table
   to KIND_ANYSTR if the key does not fit a compact one; the move keeps
   every index, so the slot found first stays valid. One test on the kind
   covers both the fit and the layout. Returns the table, NULL on error. */
ALWAYS_INLINE static inline htkeys_t*
_md_store_new_entry(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
                    PyObject* key, PyObject* value, bool fits, bool ci)
{
    assert(ci == md->is_ci);
    assert(fits == md_key_fits(md, key, identity, ci));
    htkeys_t* keys = md->keys;
    Py_ssize_t hashpos = htkeys_find_empty_slot(keys, hash);
    if (kind_is_compact(keys->kind)) {
        if (UNLIKELY(!fits)) {
            if (md_to_anystr(md) < 0) {
                return NULL;
            }
            keys = md->keys;
        } else {
            htkeys_set_index(keys, hashpos, keys->nentries);
            _md_fill_str_entry(keys, identity, key, value);
            return keys;
        }
    }
    htkeys_set_index(keys, hashpos, keys->nentries);
    _md_fill_anystr_entry(keys, hash, identity, key, value);
    return keys;
}

ALWAYS_INLINE static inline int
md_add_with_hash_steal_refs(MultiDictObject* md, Py_hash_t hash,
                            PyObject* identity, PyObject* key, PyObject* value,
                            bool fits, bool ci)
{
    assert(ci == md->is_ci);
    htkeys_t* keys = md->keys;
    if (keys->usable <= 0 || keys == &empty_htkeys) {
        /* Need to resize. */
        if (_md_resize_for_add(md) < 0) {
            return -1;
        }
        keys = md->keys;  // updated by resizing
    }

    keys = _md_store_new_entry(md, hash, identity, key, value, fits, ci);
    if (keys == NULL) {
        return -1;
    }

    bump_version(md);
    add_used(md, 1);
    keys->usable -= 1;
    keys->nentries += 1;
    md_watch_record(
        md, MultiDict_EVENT_ADDED, identity, hash, key, value, NULL);
    return 0;
}

ALWAYS_INLINE static inline int
md_add_with_hash(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
                 PyObject* key, PyObject* value, bool fits, bool ci)
{
    assert(ci == md->is_ci);
    Py_INCREF(identity);
    Py_INCREF(key);
    Py_INCREF(value);
    if (md_add_with_hash_steal_refs(md, hash, identity, key, value, fits, ci) <
        0) {
        Py_DECREF(identity);
        Py_DECREF(key);
        Py_DECREF(value);
        return -1;
    }
    return 0;
}

ALWAYS_INLINE static inline int
_md_add_for_upd_steal_refs(MultiDictObject* md, Py_hash_t hash,
                           PyObject* identity, PyObject* key, PyObject* value,
                           update_marks_t* marks, bool fits, bool ci)
{
    assert(ci == md->is_ci);
    htkeys_t* keys = md->keys;
    if (keys->usable <= 0 || keys == &empty_htkeys) {
        /* Need to resize. */
        if (_md_resize_for_add(md) < 0) {
            return -1;
        }
        keys = md->keys;  // updated by resizing
        if (update_marks_sync(marks, md) < 0) {
            return -1;
        }
    }
    if (bitmap_set(&marks->updated, keys->nentries) < 0) {
        return -1;
    }
    keys = _md_store_new_entry(md, hash, identity, key, value, fits, ci);
    if (keys == NULL) {
        return -1;
    }

    bump_version(md);
    add_used(md, 1);
    keys->usable -= 1;
    keys->nentries += 1;
    md_watch_record(
        md, MultiDict_EVENT_ADDED, identity, hash, key, value, NULL);
    return 0;
}

ALWAYS_INLINE static inline int
md_add_for_upd(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
               PyObject* key, PyObject* value, update_marks_t* marks,
               bool fits, bool ci)
{
    assert(ci == md->is_ci);
    Py_INCREF(identity);
    Py_INCREF(key);
    Py_INCREF(value);
    if (_md_add_for_upd_steal_refs(
            md, hash, identity, key, value, marks, fits, ci) < 0) {
        /* Not deferred: the caller still holds its own references, so
           none of these can drop to zero and run __del__. */
        Py_DECREF(identity);
        Py_DECREF(key);
        Py_DECREF(value);
        return -1;
    }
    return 0;
}

// Caller holds md's critical section
ALWAYS_INLINE static inline int
_md_add_locked(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
               PyObject* key, PyObject* value, bool fits, bool ci)
{
    assert(ci == md->is_ci);
    int ret = md_add_with_hash(md, hash, identity, key, value, fits, ci);
    ASSERT_CONSISTENT(md);
    return ret;
}

ALWAYS_INLINE static inline int
md_add(MultiDictObject* md, PyObject* key, PyObject* value, bool ci)
{
    assert(ci == md->is_ci);
    PyObject* identity;
    Py_hash_t hash;
    bool fits;
    if (md_calc_identity_hash_fits(md, key, &identity, &hash, &fits, ci) < 0) {
        return -1;
    }
    int ret;
    bool flush;
    Py_BEGIN_CRITICAL_SECTION(md);
    ret = _md_add_locked(md, identity, hash, key, value, fits, ci);
    flush = md_watch_pending(md);
    Py_END_CRITICAL_SECTION();
    Py_DECREF(identity);
    md_watch_flush_if(md, flush);
    return ret;
}

/* Removes entry, whose index is at `slot`, and hands its references to the
   caller; see _md_del_at() on the order. A compact entry has no identity
   reference of its own, so *pidentity is NULL for one. `kind` is
   md->keys->kind, read once by the caller. */
ALWAYS_INLINE static inline void
_md_unlink_at(MultiDictObject* md, uint8_t kind, size_t slot, entry_t* entry,
              PyObject** pidentity, PyObject** pkey, PyObject** pvalue)
{
    if (kind_is_compact(kind)) {
        *pkey = load_identity(KIND_COMPACT, entry);
        *pidentity = NULL;
        *pvalue = load_value(entry);
        reset_identity(KIND_COMPACT, entry);
    } else {
        *pidentity = load_identity(KIND_ANYSTR, entry);
        *pkey = entry->key;
        *pvalue = load_value(entry);
        reset_identity(KIND_ANYSTR, entry);
        entry->key = NULL;
    }
    reset_value(entry);
    htkeys_set_index(md->keys, (Py_ssize_t)slot, DKIX_DUMMY);
    add_used(md, -1);
}

ALWAYS_INLINE static inline void
_md_del_at(MultiDictObject* md, uint8_t kind, size_t slot, entry_t* entry)
{
    assert(md->keys != &empty_htkeys);
    /* Null out every field and finish md's bookkeeping (index, used)
       before dropping any reference. Freeing an object below can run
       a finalizer or weakref callback, which can hit a safepoint and
       transiently suspend this thread's critical section (PyMem_Malloc
       itself never does, see #1469), letting a concurrent resize run
       in between. If that resize caught this entry with some fields
       already NULL and others (or md->used, or the index) not yet
       updated, it would see md in a state that is neither "entry
       still there" nor "entry gone" and corrupt itself. Saving the
       objects locally and decref'ing them only once md is already
       fully self-consistent means a concurrent resize -- however far
       into this function it catches us -- always sees a coherent
       view. The GIL build needs the same order for its own reason: a
       __del__ there can release the GIL (Py_BEGIN_CRITICAL_SECTION is
       a no-op on that build), which would otherwise expose a
       half-deleted entry -- see #1489. entry->key is read/written as
       a plain pointer: unlike identity/value, no lock-free reader
       ever touches it (see the comment above load_identity()). */
    PyObject* identity;
    PyObject* key;
    PyObject* value;
    _md_unlink_at(md, kind, slot, entry, &identity, &key, &value);

    Py_XDECREF(identity);
    Py_XDECREF(key);
    Py_XDECREF(value);
}

/* _md_del_at() variant that defers the decref (see reflist_t);
 * used by _md_replace_locked()'s duplicate-cleanup path on both builds. */
static inline int
_md_del_at_deferred(MultiDictObject* md, size_t slot, entry_t* entry,
                    reflist_t* defer)
{
    htkeys_t* keys = md->keys;
    assert(keys != &empty_htkeys);
    int ret = 0;
    PyObject* key;
    if (kind_is_compact(keys->kind)) {
        key = load_identity(KIND_COMPACT, entry);
        reset_identity(KIND_COMPACT, entry);
    } else {
        PyObject* identity = load_identity(KIND_ANYSTR, entry);
        key = entry->key;
        reset_identity(KIND_ANYSTR, entry);
        entry->key = NULL;
        ret = reflist_push(defer, identity);
    }
    PyObject* value = load_value(entry);
    reset_value(entry);
    htkeys_set_index(keys, (Py_ssize_t)slot, DKIX_DUMMY);
    add_used(md, -1);

    if (reflist_push(defer, key) < 0) {
        ret = -1;
    }
    if (reflist_push(defer, value) < 0) {
        ret = -1;
    }
    return ret;
}

/* The keys and values of the pairs one call removes, held until it is
   done with md: releasing them can run a finalizer that mutates md. The
   first pair is held in place, which covers the usual call; any more go
   to `more`, set up on first use. */
typedef struct _removed_pairs {
    PyObject* key;
    PyObject* value;
    bool spilled;
    reflist_t more;
} removed_pairs_t;

static inline void
removed_pairs_init(removed_pairs_t* removed)
{
    removed->key = NULL;
    removed->value = NULL;
    removed->spilled = false;
}

// Out of line: inlined twice, it pushes md_next() out of the FT items iterator
NOINLINE static void
removed_pairs_release(removed_pairs_t* removed)
{
    Py_XDECREF(removed->key);
    Py_XDECREF(removed->value);
    if (UNLIKELY(removed->spilled)) {
        reflist_clear(&removed->more);
    }
}

COLD static int
_removed_pairs_spill(removed_pairs_t* removed, PyObject* key, PyObject* value)
{
    if (!removed->spilled) {
        reflist_init(&removed->more);
        removed->spilled = true;
    }
    int ret = reflist_push(&removed->more, key);
    if (reflist_push(&removed->more, value) < 0) {
        ret = -1;
    }
    return ret;
}

/* _md_del_at() variant that hands the key and value to `removed`. */
ALWAYS_INLINE static inline int
_md_del_at_held(MultiDictObject* md, htkeys_t* keys, uint8_t kind, size_t slot,
                entry_t* entry, removed_pairs_t* removed, bool ci)
{
    assert(ci == md->is_ci);
    assert(keys == md->keys && kind == keys->kind);
    assert(keys != &empty_htkeys);
    PyObject* key;
    PyObject* identity = NULL;
    if (kind_is_compact(kind)) {
        key = load_identity(KIND_COMPACT, entry);
        reset_identity(KIND_COMPACT, entry);
    } else {
        identity = load_identity(KIND_ANYSTR, entry);
        key = entry->key;
        reset_identity(KIND_ANYSTR, entry);
        entry->key = NULL;
    }
    PyObject* value = load_value(entry);
    reset_value(entry);
    htkeys_set_index(keys, (Py_ssize_t)slot, DKIX_DUMMY);
    add_used(md, -1);

    // an exact str: freeing it runs no code
    Py_XDECREF(identity);
    if (removed->key == NULL) {
        removed->key = key;
        removed->value = value;
        return 0;
    }
    return _removed_pairs_spill(removed, key, value);
}

/* Caller holds md's critical section. Returns 1 if anything was removed,
 * 0 if not, -1 on error; md_del() raises the KeyError outside the section.
 * The removed pairs go to `removed`, so their finalizers run only once
 * every match is gone and cannot add one this call then removes.
 * `watched` is a constant at both call sites, so the unwatched copy
 * carries no watch code at all: testing md->watch inside the loop costs a
 * reload per record, since every decref and store may alias it. */
ALWAYS_INLINE static inline int
_md_del_locked(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
               removed_pairs_t* removed, bool watched, bool ci)
{
    assert(ci == md->is_ci);
    bool found = false;
    int ret = 0;

    htkeys_t* keys = md->keys;
    htkeysiter_t iter;
    htkeysiter_init(&iter, keys, hash);

    entry_t* entries = htkeys_entries(keys);
    uint8_t kind = keys->kind;

    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t* entry = entry_at(kind, entries, iter.index);
        if (hash != entry_hash(kind, ci, entry)) {
            continue;
        }
        if (!str_cmp(entry_identity(kind, ci, entry), identity)) {
            continue;
        }

        if (watched) {
            if (!found) {
                md_watch_record_simple(md, MultiDict_EVENT_BATCH_BEGIN);
            }
            md_watch_record(md,
                            MultiDict_EVENT_DELETED,
                            entry_identity(kind, ci, entry),
                            hash,
                            entry->key,
                            entry->value,
                            NULL);
        }
        found = true;
        if (_md_del_at_held(md, keys, kind, iter.slot, entry, removed, ci) <
            0) {
            ret = -1;
            break;
        }
    }

    if (found) {
        bump_version(md);
        if (watched) {
            md_watch_record_simple(md, MultiDict_EVENT_BATCH_END);
        }
        if (ret == 0) {
            ret = 1;
        }
    }
    ASSERT_CONSISTENT(md);
    return ret;
}

COLD static int
_md_del_locked_watched(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                       removed_pairs_t* removed)
{
    return _md_del_locked(md, identity, hash, removed, true, md->is_ci);
}

ALWAYS_INLINE static inline int
_md_del(MultiDictObject* md, PyObject* key, bool ci)
{
    assert(ci == md->is_ci);
    PyObject* identity;
    Py_hash_t hash;
    if (md_calc_identity_hash(md, key, &identity, &hash, ci) < 0) {
        return -1;
    }
    int found;
    removed_pairs_t removed;
    removed_pairs_init(&removed);
    /* Unwatched on entry means nothing gets recorded, even if a __del__
       run by the delete attaches a watcher, so there is nothing to
       flush; a mutation that __del__ makes flushes its own records. */
    bool flush = false;
    Py_BEGIN_CRITICAL_SECTION(md);
    if (UNLIKELY(md->watch != NULL)) {
        found = _md_del_locked_watched(md, identity, hash, &removed);
        flush = md_watch_pending(md);
    } else {
        found = _md_del_locked(md, identity, hash, &removed, false, ci);
    }
    Py_END_CRITICAL_SECTION();
    removed_pairs_release(&removed);
    Py_DECREF(identity);
    md_watch_flush_if(md, flush);
    if (found == 0) {
        PyErr_SetObject(PyExc_KeyError, key);
        return -1;
    }
    return found < 0 ? -1 : 0;
}

// One copy per class, each inlining the path for its own only.
NOINLINE static int
md_del_ci(MultiDictObject* md, PyObject* key)
{
    assert(md->is_ci);
    return _md_del(md, key, true);
}

NOINLINE static int
md_del_cs(MultiDictObject* md, PyObject* key)
{
    assert(!md->is_ci);
    return _md_del(md, key, false);
}

/* For a caller that does not know the class; MultiDict entry points
   expect it to be one. */
ALWAYS_INLINE static inline int
md_del(MultiDictObject* md, PyObject* key)
{
    if (UNLIKELY(md->is_ci)) {
        return md_del_ci(md, key);
    }
    return md_del_cs(md, key);
}

static inline void
md_init_pos(MultiDictObject* md, md_pos_t* pos)
{
    pos->pos = 0;
    pos->version = md->version;
}

/* Forced: the FT items iterator needs it inline (#1601), and this unit
   sits so close to GCC's budget that unrelated changes push it out. */
ALWAYS_INLINE static inline int
md_next(MultiDictObject* md, md_pos_t* pos, PyObject** pidentity,
        PyObject** pkey, PyObject** pvalue)
{
    int ret = 0;

    if (pos->version != md->version) {
        PyErr_SetString(PyExc_RuntimeError,
                        "MultiDict is changed during iteration");
        ret = -1;
        goto cleanup;
    }

    if (pos->pos >= md->keys->nentries) {
        goto cleanup;
    }

    entry_t* entries = htkeys_entries(md->keys);
    uint8_t kind = md->keys->kind;
    entry_t* entry = entry_at(kind, entries, pos->pos);

    while (entry_is_hole(kind, entry)) {
        pos->pos += 1;
        if (pos->pos >= md->keys->nentries) {
            goto cleanup;
        }
        entry = entry_next(kind, entry);
    }

    if (pidentity) {
        *pidentity = Py_NewRef(entry_identity(kind, md->is_ci, entry));
    }

    if (pvalue) {
        *pvalue = Py_NewRef(entry->value);
    }
    if (pkey) {
        assert(entry->key != NULL);
        *pkey = md_ensure_key(md, entry);  // last entry access
        if (*pkey == NULL) {
            assert(PyErr_Occurred());
            if (pidentity) {
                Py_CLEAR(*pidentity);
            }
            if (pvalue) {
                Py_CLEAR(*pvalue);
            }
            ret = -1;
            goto cleanup;
        }
    }

    ++pos->pos;
    return 1;
cleanup:
    if (pidentity) {
        *pidentity = NULL;
    }
    if (pkey) {
        *pkey = NULL;
    }
    if (pvalue) {
        *pvalue = NULL;
    }
    return ret;
}

static inline void
md_init_pos_reverse(MultiDictObject* md, md_pos_t* pos)
{
    pos->pos = md->keys->nentries - 1;
    pos->version = md->version;
}

static inline int
md_prev(MultiDictObject* md, md_pos_t* pos, PyObject** pidentity,
        PyObject** pkey, PyObject** pvalue)
{
    int ret = 0;

    if (pos->version != md->version) {
        PyErr_SetString(PyExc_RuntimeError,
                        "MultiDict is changed during iteration");
        ret = -1;
        goto cleanup;
    }

    if (pos->pos < 0) {
        goto cleanup;
    }

    entry_t* entries = htkeys_entries(md->keys);
    uint8_t kind = md->keys->kind;
    entry_t* entry = entry_at(kind, entries, pos->pos);

    while (entry_is_hole(kind, entry)) {
        pos->pos -= 1;
        if (pos->pos < 0) {
            goto cleanup;
        }
        entry = entry_prev(kind, entry);
    }

    if (pidentity) {
        *pidentity = Py_NewRef(entry_identity(kind, md->is_ci, entry));
    }

    if (pvalue) {
        *pvalue = Py_NewRef(entry->value);
    }
    if (pkey) {
        assert(entry->key != NULL);
        *pkey = md_ensure_key(md, entry);  // last entry access
        if (*pkey == NULL) {
            assert(PyErr_Occurred());
            if (pidentity) {
                Py_CLEAR(*pidentity);
            }
            if (pvalue) {
                Py_CLEAR(*pvalue);
            }
            ret = -1;
            goto cleanup;
        }
    }

    --pos->pos;
    return 1;
cleanup:
    if (pidentity) {
        *pidentity = NULL;
    }
    if (pkey) {
        *pkey = NULL;
    }
    if (pvalue) {
        *pvalue = NULL;
    }
    return ret;
}

ALWAYS_INLINE static inline int
_md_contains_locked(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                    PyObject** pret, bool ci)
{
    assert(ci == md->is_ci);
    htkeysiter_t iter;
    htkeysiter_init(&iter, md->keys, hash);
    entry_t* entries = htkeys_entries(md->keys);
    uint8_t kind = md->keys->kind;

    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (UNLIKELY(iter.index < 0)) {
            continue;
        }
        entry_t* entry = entry_at(kind, entries, iter.index);
        if (hash != entry_hash(kind, ci, entry)) {
            continue;
        }
        if (str_cmp(identity, entry_identity(kind, ci, entry))) {
            if (pret != NULL) {
                *pret = md_ensure_key(md, entry);
                if (*pret == NULL) {
                    return -1;
                }
            }
            return 1;
        }
    }
    if (pret != NULL) {
        *pret = NULL;
    }
    return 0;
}

#ifdef Py_GIL_DISABLED

/* A compact entry keeps no hash of its own: it is read from the key, and
   a concurrent delete can free the key, so nothing is read through it
   until a reference is held, as in CPython's dict lookup on its
   unicode-only tables. A key that is the probe's own needs none: the
   caller's reference keeps that address from being reused. 1 is a match,
   0 means keep probing, -1 racing a writer. */
ALWAYS_INLINE static inline int
_compact_entry_matches(entry_t* entry, PyObject* probe, PyObject* identity,
                       Py_hash_t hash, bool ci)
{
    PyObject* key = load_identity(KIND_COMPACT, entry);
    if (key == NULL) {
        return 0;  // not populated (or deleted)
    }
    if (key == probe) {
        return 1;
    }
    key = try_get_ref(entry_identity_slot(KIND_COMPACT, entry));
    if (key == NULL) {
        return load_identity(KIND_COMPACT, entry) == NULL ? 0 : -1;
    }
    bool matched = compact_key_hash(ci, key) == hash &&
                   str_cmp(identity, compact_key_identity(ci, key));
    Py_DECREF(key);
    return matched;
}

/* The full layout keeps the hash in the entry, so it goes first and a
   mismatch costs no reference traffic; then a pointer-equal identity is a
   match outright, since the probe's own reference keeps that object alive
   and its address cannot be reused. Only a different object needs the
   reference for the string compare. */
ALWAYS_INLINE static inline int
_full_entry_matches(entry_t* entry, PyObject* identity, Py_hash_t hash)
{
    if (load_hash(as_anystr(entry)) != hash) {
        return 0;
    }
    PyObject* held = load_identity(KIND_ANYSTR, entry);
    if (held == NULL) {
        return 0;  // not populated (or deleted)
    }
    if (held == identity) {
        return 1;
    }
    held = try_get_ref(entry_identity_slot(KIND_ANYSTR, entry));
    if (held == NULL) {
        return load_identity(KIND_ANYSTR, entry) == NULL ? 0 : -1;
    }
    bool matched = str_cmp(identity, held);
    Py_DECREF(held);
    return matched;
}

ALWAYS_INLINE static inline int
_md_contains_lockfree(MultiDictObject* md, PyObject* probe, PyObject* identity,
                      Py_hash_t hash, bool ci)
{
    assert(ci == md->is_ci);
    htkeys_t* keys = _md_reader_enter(md);
    htkeysiter_t iter;
    htkeysiter_init(&iter, keys, hash);
    entry_t* entries = htkeys_entries(keys);
    uint8_t kind = keys->kind;

    int result = 0;
    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (UNLIKELY(iter.index < 0)) {
            continue;
        }
        entry_t* entry = entry_at(kind, entries, iter.index);
        int matched =
            kind_is_compact(kind)
                ? _compact_entry_matches(entry, probe, identity, hash, ci)
                : _full_entry_matches(entry, identity, hash);
        if (matched == 0) {
            continue;
        }
        result = matched < 0 ? 2 : 1;  // 2 is _MD_NEED_LOCK
        break;
    }

    _md_reader_exit(md, keys);
    return result;
}

/* Only a reader racing a writer gets here, so it stays out of line and
   leaves the inlining budget to the lock-free loops. */
NOINLINE static int
_md_contains_retry_locked(MultiDictObject* md, PyObject* identity,
                          Py_hash_t hash)
{
    int result;
    Py_BEGIN_CRITICAL_SECTION(md);
    result = _md_contains_locked(md, identity, hash, NULL, md->is_ci);
    Py_END_CRITICAL_SECTION();
    return result;
}

#endif /* Py_GIL_DISABLED */

ALWAYS_INLINE static inline int
_md_contains_identity(MultiDictObject* md, PyObject* probe, PyObject* identity,
                      Py_hash_t hash, bool ci)
{
    int result;
#ifdef Py_GIL_DISABLED
    result = _md_contains_lockfree(md, probe, identity, hash, ci);
    if (result == 2 /* _MD_NEED_LOCK */) {
        result = _md_contains_retry_locked(md, identity, hash);
    }
#else
    result = _md_contains_locked(md, identity, hash, NULL, ci);
#endif
    return result;
}

/* A key whose identity has to be computed: off md_contains()'s straight
   line, so its decref is not on every lookup's exit. */
NOINLINE static int
_md_contains_owned(MultiDictObject* md, PyObject* key)
{
    PyObject* identity;
    Py_hash_t hash;
    if (md_calc_identity_hash(md, key, &identity, &hash, md->is_ci) < 0) {
        return -1;
    }
    int result = _md_contains_identity(md, key, identity, hash, md->is_ci);
    Py_DECREF(identity);
    return result;
}

/* ci is md->is_ci; see md_calc_identity(). */
ALWAYS_INLINE static inline int
md_contains(MultiDictObject* md, PyObject* key, bool ci)
{
    assert(ci == md->is_ci);
    if (!PyUnicode_Check(key)) {
        return 0;
    }
    PyObject* identity = md_borrow_identity(md, key, ci);
    if (identity == NULL) {
        return _md_contains_owned(md, key);
    }
    Py_hash_t hash = unicode_hash(identity);
    if (hash == -1) {
        return -1;
    }
    return _md_contains_identity(md, key, identity, hash, ci);
}

/* md_contains() that also returns the stored key in *pret.  Only the view
   set operations need it, so it stays out of line and off md_contains()'s
   inlining budget. */
NOINLINE static int
md_find_key(MultiDictObject* md, PyObject* key, PyObject** pret)
{
    *pret = NULL;
    if (!PyUnicode_Check(key)) {
        return 0;
    }

    PyObject* identity;
    Py_hash_t hash;
    if (md_calc_identity_hash(md, key, &identity, &hash, md->is_ci) < 0) {
        return -1;
    }

    int result;
    Py_BEGIN_CRITICAL_SECTION(md);
    result = _md_contains_locked(md, identity, hash, pret, md->is_ci);
    Py_END_CRITICAL_SECTION();
    Py_DECREF(identity);
    return result;
}

ALWAYS_INLINE static inline int
_md_get_one_locked(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                   PyObject** ret, bool ci)
{
    assert(ci == md->is_ci);
    htkeysiter_t iter;
    htkeysiter_init(&iter, md->keys, hash);
    entry_t* entries = htkeys_entries(md->keys);
    uint8_t kind = md->keys->kind;

    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (UNLIKELY(iter.index < 0)) {
            continue;
        }
        entry_t* entry = entry_at(kind, entries, iter.index);
        if (hash != entry_hash(kind, ci, entry)) {
            continue;
        }
        if (str_cmp(identity, entry_identity(kind, ci, entry))) {
            *ret = Py_NewRef(entry->value);
            return 1;
        }
    }
    return 0;
}

#ifdef Py_GIL_DISABLED

/* Sentinel meaning "could not complete lock-free"; never returned to
   md_get_one()'s own caller, only used between the two functions
   below. Distinct from 1 (found) / 0 (not found) / -1 (error). */
#define _MD_NEED_LOCK 2

ALWAYS_INLINE static inline int
_md_get_one_lockfree(MultiDictObject* md, PyObject* probe, PyObject* identity,
                     Py_hash_t hash, PyObject** ret, bool ci)
{
    assert(ci == md->is_ci);
    htkeys_t* keys = _md_reader_enter(md);
    htkeysiter_t iter;
    htkeysiter_init(&iter, keys, hash);
    entry_t* entries = htkeys_entries(keys);
    uint8_t kind = keys->kind;

    int result = 0;
    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (UNLIKELY(iter.index < 0)) {
            continue;
        }
        entry_t* entry = entry_at(kind, entries, iter.index);
        int matched =
            kind_is_compact(kind)
                ? _compact_entry_matches(entry, probe, identity, hash, ci)
                : _full_entry_matches(entry, identity, hash);
        if (matched == 0) {
            continue;
        }
        if (matched < 0) {
            result = _MD_NEED_LOCK;  // racing a concurrent change
            break;
        }

        PyObject* value = try_get_ref(&entry->value);
        if (value == NULL) {
            result = _MD_NEED_LOCK;
            break;
        }
        *ret = value;
        result = 1;
        break;
    }

    _md_reader_exit(md, keys);
    return result;
}

/* See _md_contains_retry_locked(). */
NOINLINE static int
_md_get_one_retry_locked(MultiDictObject* md, PyObject* identity,
                         Py_hash_t hash, PyObject** ret)
{
    int result;
    Py_BEGIN_CRITICAL_SECTION(md);
    result = _md_get_one_locked(md, identity, hash, ret, md->is_ci);
    Py_END_CRITICAL_SECTION();
    return result;
}

ALWAYS_INLINE static inline int
_md_get_one_identity(MultiDictObject* md, PyObject* probe, PyObject* identity,
                     Py_hash_t hash, PyObject** ret, bool ci)
{
    int result = _md_get_one_lockfree(md, probe, identity, hash, ret, ci);
    if (result == _MD_NEED_LOCK) {
        result = _md_get_one_retry_locked(md, identity, hash, ret);
    }
    return result;
}

#undef _MD_NEED_LOCK

#else /* !Py_GIL_DISABLED */

ALWAYS_INLINE static inline int
_md_get_one_identity(MultiDictObject* md, PyObject* probe, PyObject* identity,
                     Py_hash_t hash, PyObject** ret, bool ci)
{
    return _md_get_one_locked(md, identity, hash, ret, ci);
}

#endif /* Py_GIL_DISABLED */

/* See _md_contains_owned().  Returns the value, or NULL, with an exception
   set on error: an out-pointer into the caller's frame would make GCC put a
   stack protector on every lookup slot. */
NOINLINE static PyObject*
_md_get_one_owned(MultiDictObject* md, PyObject* key)
{
    PyObject* identity;
    Py_hash_t hash;
    if (md_calc_identity_hash(md, key, &identity, &hash, md->is_ci) < 0) {
        return NULL;
    }
    PyObject* ret = NULL;
    _md_get_one_identity(md, key, identity, hash, &ret, md->is_ci);
    Py_DECREF(identity);
    return ret;
}

ALWAYS_INLINE static inline int
md_get_one(MultiDictObject* md, PyObject* key, PyObject** ret, bool ci)
{
    assert(ci == md->is_ci);
    PyObject* identity = md_borrow_identity(md, key, ci);
    if (identity == NULL) {
        *ret = _md_get_one_owned(md, key);
        if (*ret != NULL) {
            return 1;
        }
        return PyErr_Occurred() ? -1 : 0;
    }
    Py_hash_t hash = unicode_hash(identity);
    if (hash == -1) {
        return -1;
    }
    return _md_get_one_identity(md, key, identity, hash, ret, ci);
}

static inline int
md_to_dict(MultiDictObject* md, PyObject** ret)
{
    PyObject* key = NULL;
    PyObject* lst = NULL;
    uint64_t version = md->version;
    bitmap_t collected;
    collected.summary = NULL;

    *ret = PyDict_New();
    if (*ret == NULL) {
        return -1;
    }
    bitmap_init(&collected, md->keys, md->keys->nentries);

    /* Walk the entries in insertion order, so every key is collected at its
       first spelling; a hash chain walk is not insertion-ordered. */
    for (Py_ssize_t pos = 0; pos < md->keys->nentries; pos++) {
        entry_t* entries = htkeys_entries(md->keys);
        uint8_t kind = md->keys->kind;
        entry_t* entry = entry_at(kind, entries, pos);
        if (entry_is_hole(kind, entry)) {
            continue;  // deleted
        }
        if (bitmap_test(&collected, pos)) {
            continue;  // collected already under its first key
        }

        /* Equal keys sit on one hash chain in insertion order. Only the
           list allocation below can run Python in this walk. */
        Py_hash_t hash = entry_hash(kind, md->is_ci, entry);
        htkeysiter_t iter;
        htkeysiter_init(&iter, md->keys, hash);
        for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
            if (iter.index < 0) {
                continue;
            }
            entry_t* e = entry_at(kind, entries, iter.index);
            if (entry_hash(kind, md->is_ci, e) != hash ||
                !str_cmp(entry_identity(kind, md->is_ci, entry),
                         entry_identity(kind, md->is_ci, e))) {
                continue;
            }
            int seen = bitmap_test_and_set(&collected, iter.index);
            if (seen < 0) {
                goto fail;
            }
            if (seen) {
                continue;
            }
            if (lst == NULL) {
                lst = PyList_New(1);
                if (lst == NULL) {
                    goto fail;
                }
                /* On 3.10 and 3.11, allocating a tracked object can run a
                   collection whose finalizers mutate md: refused as below,
                   before `e` is read again. */
                if (md->version != version) {
                    PyErr_SetString(PyExc_RuntimeError,
                                    "MultiDict is changed during iteration");
                    goto fail;
                }
                PyList_SET_ITEM(lst, 0, Py_NewRef(e->value));
            } else if (PyList_Append(lst, e->value) < 0) {
                goto fail;
            }
        }
        if (lst == NULL) {
            continue;  // not reachable from its own hash chain
        }

        /* Both calls below can run a str subclass's own __hash__, __eq__
           or __del__, which may mutate this multidict. That is refused the
           way md_next() refuses one, before `entry` or `collected` is
           trusted again. */
        key = md_ensure_key(md, entry);
        if (key == NULL) {
            goto fail;
        }
        if (PyDict_SetItem(*ret, key, lst) < 0) {
            goto fail;
        }
        Py_CLEAR(key);
        Py_CLEAR(lst);
        if (md->version != version) {
            PyErr_SetString(PyExc_RuntimeError,
                            "MultiDict is changed during iteration");
            goto fail;
        }
    }

    bitmap_release(&collected);
    return 0;
fail:
    bitmap_release(&collected);
    Py_XDECREF(key);
    Py_XDECREF(lst);
    Py_CLEAR(*ret);
    return -1;
}

// Caller holds md's critical section
ALWAYS_INLINE static inline int
_md_set_default_locked(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                       PyObject* key, PyObject* value, PyObject** result,
                       bool ci)
{
    assert(ci == md->is_ci);
    ASSERT_CONSISTENT(md);

    htkeysiter_t iter;
    htkeysiter_init(&iter, md->keys, hash);
    entry_t* entries = htkeys_entries(md->keys);
    uint8_t kind = md->keys->kind;

    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t* entry = entry_at(kind, entries, iter.index);

        if (hash != entry_hash(kind, ci, entry)) {
            continue;
        }
        if (str_cmp(identity, entry_identity(kind, ci, entry))) {
            ASSERT_CONSISTENT(md);
            *result = Py_NewRef(entry->value);
            return 1;
        }
    }

    if (md_add_with_hash(md,
                         hash,
                         identity,
                         key,
                         value,
                         md_key_fits(md, key, identity, ci),
                         ci) < 0) {
        return -1;
    }

    ASSERT_CONSISTENT(md);
    *result = Py_NewRef(value);
    return 0;
}

ALWAYS_INLINE static inline int
md_set_default(MultiDictObject* md, PyObject* key, PyObject* value,
               PyObject** result, bool ci)
{
    assert(ci == md->is_ci);
    *result = NULL;
    if (value == NULL) {
        /* The caller wants the implicit None default.  Substituting it
           here keeps it out of the critical section below, where the
           pythoncapi_compat shim for Py_GetConstant() used on 3.10 to
           3.12 could allocate on its first call. */
        value = md->state->none;
    }
    PyObject* identity;
    Py_hash_t hash;
    if (md_calc_identity_hash(md, key, &identity, &hash, ci) < 0) {
        return -1;
    }
    int ret;
    bool flush;
    Py_BEGIN_CRITICAL_SECTION(md);
    ret = _md_set_default_locked(md, identity, hash, key, value, result, ci);
    flush = md_watch_pending(md);
    Py_END_CRITICAL_SECTION();
    Py_DECREF(identity);
    md_watch_flush_if(md, flush);
    return ret;
}

/* Caller holds md's critical section. `watched` is a constant at both
 * call sites; see _md_del_locked(). */
ALWAYS_INLINE static inline int
_md_pop_one_locked(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                   PyObject** ret, bool watched, bool ci)
{
    assert(ci == md->is_ci);
    htkeysiter_t iter;
    htkeysiter_init(&iter, md->keys, hash);
    entry_t* entries = htkeys_entries(md->keys);
    uint8_t kind = md->keys->kind;

    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t* entry = entry_at(kind, entries, iter.index);

        if (hash != entry_hash(kind, ci, entry)) {
            continue;
        }
        if (str_cmp(identity, entry_identity(kind, ci, entry))) {
            PyObject* value = Py_NewRef(entry->value);
            if (watched) {
                md_watch_record(md,
                                MultiDict_EVENT_DELETED,
                                identity,
                                hash,
                                entry->key,
                                value,
                                NULL);
            }
            _md_del_at(md, kind, iter.slot, entry);
            *ret = value;
            bump_version(md);
            ASSERT_CONSISTENT(md);
            return 1;
        }
    }
    ASSERT_CONSISTENT(md);
    return 0;
}

COLD static int
_md_pop_one_locked_watched(MultiDictObject* md, PyObject* identity,
                           Py_hash_t hash, PyObject** ret)
{
    return _md_pop_one_locked(md, identity, hash, ret, true, md->is_ci);
}

ALWAYS_INLINE static inline int
_md_pop_one(MultiDictObject* md, PyObject* key, PyObject** ret, bool ci)
{
    assert(ci == md->is_ci);
    PyObject* identity;
    Py_hash_t hash;
    if (md_calc_identity_hash(md, key, &identity, &hash, ci) < 0) {
        return -1;
    }
    int result;
    // see md_del()
    bool flush = false;
    Py_BEGIN_CRITICAL_SECTION(md);
    if (UNLIKELY(md->watch != NULL)) {
        result = _md_pop_one_locked_watched(md, identity, hash, ret);
        flush = md_watch_pending(md);
    } else {
        result = _md_pop_one_locked(md, identity, hash, ret, false, ci);
    }
    Py_END_CRITICAL_SECTION();
    Py_DECREF(identity);
    md_watch_flush_if(md, flush);
    return result;
}

// One copy per class, each inlining the path for its own only.
NOINLINE static int
md_pop_one_ci(MultiDictObject* md, PyObject* key, PyObject** ret)
{
    assert(md->is_ci);
    return _md_pop_one(md, key, ret, true);
}

NOINLINE static int
md_pop_one_cs(MultiDictObject* md, PyObject* key, PyObject** ret)
{
    assert(!md->is_ci);
    return _md_pop_one(md, key, ret, false);
}

/* For a caller that does not know the class; MultiDict entry points
   expect it to be one. */
ALWAYS_INLINE static inline int
md_pop_one(MultiDictObject* md, PyObject* key, PyObject** ret)
{
    if (UNLIKELY(md->is_ci)) {
        return md_pop_one_ci(md, key, ret);
    }
    return md_pop_one_cs(md, key, ret);
}

static int
_md_getall_visit(void* user_data, PyObject* identity, Py_hash_t hash,
                 PyObject* key, PyObject* value)
{
    (void)identity;
    (void)hash;
    (void)key;  // value-only walk
    if (reflist_push((reflist_t*)user_data, Py_NewRef(value)) < 0) {
        return -1;
    }
    return 1;
}

// Caller holds md's critical section
ALWAYS_INLINE static inline int
_md_get_all_locked(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                   reflist_t* values, bool ci)
{
    Py_ssize_t count = md_walk_with_hash(
        md, identity, hash, false, _md_getall_visit, values, ci);
    return count < 0 ? -1 : 0;
}

ALWAYS_INLINE static inline int
md_get_all(MultiDictObject* md, PyObject* key, PyObject** ret, bool ci)
{
    assert(ci == md->is_ci);
    *ret = NULL;
    PyObject* identity;
    Py_hash_t hash;
    if (md_calc_identity_hash(md, key, &identity, &hash, ci) < 0) {
        return -1;
    }
    reflist_t values;
    reflist_init(&values);
    int tmp;
    Py_BEGIN_CRITICAL_SECTION(md);
    tmp = _md_get_all_locked(md, identity, hash, &values, ci);
    Py_END_CRITICAL_SECTION();
    Py_DECREF(identity);
    if (tmp < 0) {
        reflist_clear(&values);
        return -1;
    }
    if (reflist_empty(&values)) {
        return 0;
    }
    *ret = reflist_to_list(&values);
    return *ret != NULL ? 1 : -1;
}

/* Caller holds md's critical section. The removed pairs go to `removed`,
 * as in _md_del_locked(); `values` collects the result. */
static inline int
_md_pop_all_locked(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                   reflist_t* values, removed_pairs_t* removed)
{
    if (md_len(md) == 0) {
        return 0;
    }

    bool batched = false;
    int ret = 0;

    htkeysiter_t iter;
    htkeysiter_init(&iter, md->keys, hash);
    entry_t* entries = htkeys_entries(md->keys);
    uint8_t kind = md->keys->kind;

    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t* entry = entry_at(kind, entries, iter.index);

        if (hash != entry_hash(kind, md->is_ci, entry)) {
            continue;
        }
        if (str_cmp(identity, entry_identity(kind, md->is_ci, entry))) {
            if (reflist_push(values, Py_NewRef(entry->value)) < 0) {
                ret = -1;
                break;
            }
            bump_version(md);
            if (!batched) {
                batched = true;
                md_watch_record_simple(md, MultiDict_EVENT_BATCH_BEGIN);
            }
            md_watch_record(md,
                            MultiDict_EVENT_DELETED,
                            identity,
                            hash,
                            entry->key,
                            entry->value,
                            NULL);
            if (_md_del_at_held(
                    md, md->keys, kind, iter.slot, entry, removed, md->is_ci) <
                0) {
                ret = -1;
                break;
            }
        }
    }

    if (batched) {
        md_watch_record_simple(md, MultiDict_EVENT_BATCH_END);
    }
    ASSERT_CONSISTENT(md);
    return ret;
}

static inline int
md_pop_all(MultiDictObject* md, PyObject* key, PyObject** ret)
{
    PyObject* identity;
    Py_hash_t hash;
    if (md_calc_identity_hash(md, key, &identity, &hash, md->is_ci) < 0) {
        return -1;
    }
    reflist_t values;
    reflist_init(&values);
    removed_pairs_t removed;
    removed_pairs_init(&removed);
    int tmp;
    bool flush;
    Py_BEGIN_CRITICAL_SECTION(md);
    tmp = _md_pop_all_locked(md, identity, hash, &values, &removed);
    flush = md_watch_pending(md);
    Py_END_CRITICAL_SECTION();
    removed_pairs_release(&removed);
    Py_DECREF(identity);
    md_watch_flush_if(md, flush);
    if (tmp < 0) {
        reflist_clear(&values);
        return -1;
    }
    if (reflist_empty(&values)) {
        return 0;
    }
    *ret = reflist_to_list(&values);
    return *ret != NULL ? 1 : -1;
}

ALWAYS_INLINE static inline PyObject*
_md_pop_item(MultiDictObject* md, bool ci)
{
    assert(ci == md->is_ci);
    if (md->used == 0) {
        PyErr_SetString(PyExc_KeyError, "empty multidict");
        return NULL;
    }

    htkeys_t* keys = md->keys;
    entry_t* entries = htkeys_entries(keys);
    uint8_t kind = keys->kind;

    Py_ssize_t pos = keys->nentries - 1;
    entry_t* entry = entry_at(kind, entries, pos);
    while (pos >= 0 && entry_is_hole(kind, entry)) {
        pos--;
        entry = entry_prev(kind, entry);
    }
    assert(pos >= 0);

    htkeysiter_t iter;
    htkeysiter_init(&iter, keys, entry_hash(kind, ci, entry));

    for (; iter.index != pos; htkeysiter_next(&iter)) {
    }
    md_watch_record(md,
                    MultiDict_EVENT_DELETED,
                    entry_identity(kind, ci, entry),
                    entry_hash(kind, ci, entry),
                    entry->key,
                    entry->value,
                    NULL);
    /* The entry is the last live one, so everything from it on is
       tombstones: drop them, or the next popitem() scans them again and
       popping n items costs O(n^2). The index slots stay DKIX_DUMMY, as
       after any delete, the same trim CPython's dict does. Trimmed before
       the delete's decrefs, which can run a __del__ that suspends the
       critical section; an add() slipping in then appends at pos. Not
       while an update() is in flight, whose marks would then name the
       entries that reuse the trimmed indices; see update_marks.h. */
    if (md->batches == 0) {
        keys->nentries = pos;
    }
    /* The entry's refs, taken over: building the result below can run
       Python code that mutates md (an istr key's __str__, or on 3.10 and
       3.11 a collection the tuple triggers), so it runs once the pair is
       gone. */
    PyObject* identity;
    PyObject* key;
    PyObject* value;
    _md_unlink_at(md, kind, iter.slot, entry, &identity, &key, &value);
    bump_version(md);
    ASSERT_CONSISTENT(md);

    // a compact table's key is already what md_calc_key() would return
    if (identity != NULL) {
        Py_SETREF(key, md_calc_key(md, key, identity, ci));
        Py_DECREF(identity);
        if (key == NULL) {
            Py_DECREF(value);
            return NULL;
        }
    }
    PyObject* ret = PyTuple_New(2);
    if (ret == NULL) {
        Py_DECREF(key);
        Py_DECREF(value);
        return NULL;
    }
    PyTuple_SET_ITEM(ret, 0, key);
    PyTuple_SET_ITEM(ret, 1, value);
    return ret;
}

static inline PyObject*
md_pop_item(MultiDictObject* md)
{
    return _md_pop_item(md, md->is_ci);
}

/* Later matches of a replaced key are rare, so their reflist lives on
   the heap: in the caller's frame it cost every d[key] = v. The watcher
   hears of the delete only once the allocation can no longer stop it. */
COLD static int
_md_replace_del_dup(MultiDictObject* md, size_t slot, entry_t* entry,
                    uint8_t kind, bool ci, Py_hash_t hash, bool watched,
                    reflist_t** dups)
{
    if (*dups == NULL) {
        *dups = PyMem_Malloc(sizeof(reflist_t));
        if (*dups == NULL) {
            PyErr_NoMemory();
            return -1;
        }
        reflist_init(*dups);
    }
    if (watched) {
        md_watch_record(md,
                        MultiDict_EVENT_DELETED,
                        entry_identity(kind, ci, entry),
                        hash,
                        entry->key,
                        entry->value,
                        NULL);
    }
    return _md_del_at_deferred(md, slot, entry, *dups);
}

COLD static void
_md_replace_free_dups(reflist_t* dups)
{
    reflist_clear(dups);
    PyMem_Free(dups);
}

/* Returns 1 when `key` isn't there, leaving the add to the caller.
 * `watched` is a constant at both call sites; see _md_del_locked(). */
ALWAYS_INLINE static inline int
_md_replace_locked(MultiDictObject* md, PyObject* key, PyObject* value,
                   PyObject* identity, Py_hash_t hash, PyObject** old_key_out,
                   PyObject** old_value_out, reflist_t** dups, bool watched,
                   bool ci)
{
    assert(ci == md->is_ci);
    bool found = false;

    /* Retries on a concurrent resize (Py_GIL_DISABLED only); deferred
     * decrefs mean nothing here can trigger one, so this shouldn't loop. */
    for (;;) {
        htkeysiter_t iter;
        htkeysiter_init(&iter, md->keys, hash);
        /* The one entry to keep. Later matches are deleted, which turns
           their slots into DKIX_DUMMY, so only this one can show up again
           when htkeysiter_next() repeats a slot. */
        Py_ssize_t replaced = -1;
        /* Equal keys sit on their hash chain in insertion order, before
           and after a resize alike, so on a retry the first match is the
           entry an earlier attempt already replaced. */
        bool skip_first = found;
        bool stale = false;

        for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
            if (iter.index < 0 || iter.index == replaced) {
                continue;
            }
#ifdef Py_GIL_DISABLED
            htkeys_t* keys_before = md->keys;
            uint64_t version_before = md->version;
#endif
            entry_t* entries = htkeys_entries(md->keys);
            uint8_t kind = md->keys->kind;
            entry_t* entry = entry_at(kind, entries, iter.index);
            if (entry_hash(kind, ci, entry) != hash ||
                !str_cmp(identity, entry_identity(kind, ci, entry))) {
                continue;
            }
            if (skip_first) {
                skip_first = false;
                replaced = iter.index;
                continue;
            }
            if (!found) {
                /* Checked only here, where the key gets stored: a miss
                   leaves it to md_add_with_hash(), and the same key
                   object fits by definition. */
                PyObject* old_key = entry->key;
                if (kind_is_compact(kind) && key != old_key &&
                    UNLIKELY(!md_key_fits(md, key, identity, ci))) {
                    if (md_to_anystr(md) < 0) {
                        return -1;
                    }
                    stale = true;  // entry is in the old table
                    break;
                }
                found = true;
                replaced = iter.index;
                /* old_key/old_value decref deferred to the caller, past
                   the critical section -- see reflist_t */
                PyObject* old_value = load_value(entry);
                /* The same key object needs no store, and in a compact
                   table no new publication either. */
                if (key == old_key) {
                    old_key = NULL;
                } else {
                    replace_key(kind, entry, Py_NewRef(key));
                }
                publish_value(entry, Py_NewRef(value));
                if (watched) {
                    md_watch_record(md,
                                    MultiDict_EVENT_REPLACED,
                                    identity,
                                    hash,
                                    key,
                                    value,
                                    old_value);
                }
                *old_key_out = old_key;
                *old_value_out = old_value;
            } else if (_md_replace_del_dup(md,
                                           iter.slot,
                                           entry,
                                           kind,
                                           ci,
                                           hash,
                                           watched,
                                           dups) < 0) {
                return -1;
            }
#ifdef Py_GIL_DISABLED
            /* Checking the pointer alone isn't enough: a freed table
               can get reallocated at the very same address by a later
               resize (same size class, common in practice), which
               would make a pointer-only check miss the change. Every
               mutation bumps md->version, including ones that don't
               otherwise touch md->keys, so compare both. */
            if (md->keys != keys_before || md->version != version_before) {
                stale = true;
                break;
            }
#endif
        }
        if (stale) {
            continue;
        }

        if (!found) {
            return 1;
        }
        bump_version(md);
        return 0;
    }
}

/* The fit is checked only here, where a key is stored, not carried from
   the identity: kept live across the replace loop, it cost d[key] = v up
   to 4%. */
ALWAYS_INLINE static inline int
_md_add_after_replace(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
                      PyObject* key, PyObject* value, bool ci)
{
    return md_add_with_hash(md,
                            hash,
                            identity,
                            key,
                            value,
                            md_key_fits(md, key, identity, ci),
                            ci);
}

COLD static int
_md_replace_watched(MultiDictObject* md, PyObject* key, PyObject* value,
                    PyObject* identity, Py_hash_t hash, PyObject** old_key,
                    PyObject** old_value, reflist_t** dups)
{
    md_watch_record_simple(md, MultiDict_EVENT_BATCH_BEGIN);
    int ret = _md_replace_locked(md,
                                 key,
                                 value,
                                 identity,
                                 hash,
                                 old_key,
                                 old_value,
                                 dups,
                                 true,
                                 md->is_ci);
    if (ret > 0) {
        ret = _md_add_after_replace(md, hash, identity, key, value, md->is_ci);
    }
    md_watch_record_simple(md, MultiDict_EVENT_BATCH_END);
    return ret;
}

ALWAYS_INLINE static inline int
_md_replace(MultiDictObject* md, PyObject* key, PyObject* value, bool ci)
{
    assert(ci == md->is_ci);
    PyObject* identity;
    Py_hash_t hash;
    if (md_calc_identity_hash(md, key, &identity, &hash, ci) < 0) {
        return -1;
    }
    // The one replaced entry's refs; later matches go to dups
    PyObject* old_key = NULL;
    PyObject* old_value = NULL;
    reflist_t* dups = NULL;
    int ret;
    // see md_del()
    bool flush = false;
    Py_BEGIN_CRITICAL_SECTION(md);
    if (UNLIKELY(md->watch != NULL)) {
        ret = _md_replace_watched(
            md, key, value, identity, hash, &old_key, &old_value, &dups);
        flush = md_watch_pending(md);
    } else {
        ret = _md_replace_locked(md,
                                 key,
                                 value,
                                 identity,
                                 hash,
                                 &old_key,
                                 &old_value,
                                 &dups,
                                 false,
                                 ci);
        if (ret > 0) {
            ret = _md_add_after_replace(md, hash, identity, key, value, ci);
        }
    }
    ASSERT_CONSISTENT(md);
    Py_END_CRITICAL_SECTION();
    Py_XDECREF(old_key);
    Py_XDECREF(old_value);
    if (UNLIKELY(dups != NULL)) {
        _md_replace_free_dups(dups);
    }
    Py_DECREF(identity);
    md_watch_flush_if(md, flush);
    return ret;
}

// One copy per class, each inlining the path for its own only.
NOINLINE static int
md_replace_ci(MultiDictObject* md, PyObject* key, PyObject* value)
{
    assert(md->is_ci);
    return _md_replace(md, key, value, true);
}

NOINLINE static int
md_replace_cs(MultiDictObject* md, PyObject* key, PyObject* value)
{
    assert(!md->is_ci);
    return _md_replace(md, key, value, false);
}

/* For a caller that does not know the class; MultiDict entry points
   expect it to be one. */
ALWAYS_INLINE static inline int
md_replace(MultiDictObject* md, PyObject* key, PyObject* value)
{
    if (UNLIKELY(md->is_ci)) {
        return md_replace_ci(md, key, value);
    }
    return md_replace_cs(md, key, value);
}

static inline int
md_eq(MultiDictObject* md, MultiDictObject* other)
{
    if (md == other) {
        return 1;
    }

    if (md_len(md) != md_len(other)) {
        return 0;
    }

    Py_ssize_t pos1 = 0;
    Py_ssize_t pos2 = 0;

    entry_t* lft_entries = htkeys_entries(md->keys);
    entry_t* rht_entries = htkeys_entries(other->keys);
    for (;;) {
        if (pos1 >= md->keys->nentries || pos2 >= other->keys->nentries) {
            return 1;
        }
        entry_t* entry1 = htkeys_entry_at(md->keys, lft_entries, pos1);
        if (entry_is_hole(md->keys->kind, entry1)) {
            pos1++;
            continue;
        }
        entry_t* entry2 = htkeys_entry_at(other->keys, rht_entries, pos2);
        if (entry_is_hole(other->keys->kind, entry2)) {
            pos2++;
            continue;
        }

        if (entry_hash(md->keys->kind, md->is_ci, entry1) !=
            entry_hash(other->keys->kind, other->is_ci, entry2)) {
            return 0;
        }

        if (!str_cmp(
                entry_identity(md->keys->kind, md->is_ci, entry1),
                entry_identity(other->keys->kind, other->is_ci, entry2))) {
            return 0;
        }

        PyObject* value1 = entry1->value;
        PyObject* value2 = entry2->value;
        int cmp;
        if (value1 == value2) {
            cmp = 1;
        } else if (PyUnicode_CheckExact(value1) &&
                   PyUnicode_CheckExact(value2)) {
            cmp = str_cmp(value1, value2);
        } else {
            /* A value's __eq__ can mutate either dict and free its keys. */
            Py_INCREF(value1);
            Py_INCREF(value2);
            cmp = PyObject_RichCompareBool(value1, value2, Py_EQ);
            Py_DECREF(value1);
            Py_DECREF(value2);
            lft_entries = htkeys_entries(md->keys);
            rht_entries = htkeys_entries(other->keys);
        }
        if (cmp < 0) {
            return -1;
        };
        if (cmp == 0) {
            return 0;
        }
        pos1++;
        pos2++;
    }
    return 1;
}

static inline int
md_eq_to_mapping(MultiDictObject* md, PyObject* other)
{
    PyObject* key = NULL;
    PyObject* avalue = NULL;
    PyObject* bvalue;

    Py_ssize_t other_len;

    if (!PyMapping_Check(other)) {
        PyErr_Format(PyExc_TypeError,
                     "other argument must be a mapping, not %s",
                     Py_TYPE(other)->tp_name);
        return -1;
    }

    other_len = PyMapping_Size(other);
    if (other_len < 0) {
        return -1;
    }
    if (md_len(md) != other_len) {
        return 0;
    }

    md_pos_t pos;
    md_init_pos(md, &pos);

    for (;;) {
        int ret = md_next(md, &pos, NULL, &key, &avalue);
        if (ret < 0) {
            return -1;
        }
        if (ret == 0) {
            break;
        }
        ret = PyMapping_GetOptionalItem(other, key, &bvalue);
        Py_DECREF(key);
        if (ret < 0) {
            Py_CLEAR(avalue);
            return -1;
        }

        if (bvalue == NULL) {
            Py_CLEAR(avalue);
            return 0;
        }

        int eq = PyObject_RichCompareBool(avalue, bvalue, Py_EQ);
        Py_DECREF(bvalue);
        Py_DECREF(avalue);

        if (eq <= 0) {
            return eq;
        }
    }

    return 1;
}

NOINLINE static PyObject*
md_repr(MultiDictObject* md, PyObject* obj, bool show_keys, bool show_values)
{
    int reprenter = Py_ReprEnter(obj);
    if (reprenter != 0) {
        return reprenter > 0 ? PyUnicode_FromString("...") : NULL;
    }

    PyObject* name =
        PyObject_GetAttr((PyObject*)Py_TYPE(obj), md->state->str_name);
    if (name == NULL) {
        Py_ReprLeave(obj);
        return NULL;
    }

    PyObject* key = NULL;
    PyObject* value = NULL;

    bool comma = false;
    uint64_t version = md->version;

    PyUnicodeWriter* writer = PyUnicodeWriter_Create(1024);
    if (writer == NULL) {
        Py_CLEAR(name);
        Py_ReprLeave(obj);
        return NULL;
    }

    if (PyUnicodeWriter_WriteChar(writer, '<') < 0) {
        goto fail;
    }
    if (PyUnicodeWriter_WriteStr(writer, name) < 0) {
        goto fail;
    }
    if (PyUnicodeWriter_WriteChar(writer, '(') < 0) {
        goto fail;
    }

    entry_t* entries = htkeys_entries(md->keys);
    uint8_t kind = md->keys->kind;

    for (Py_ssize_t pos = 0; pos < md->keys->nentries; ++pos) {
        if (version != md->version) {
            PyErr_SetString(PyExc_RuntimeError,
                            "MultiDict changed during iteration");
            goto fail;  // discard the writer instead of leaking it
        }
        entry_t* entry = entry_at(kind, entries, pos);
        if (entry_is_hole(kind, entry)) {
            continue;
        }
        key = Py_NewRef(entry->key);
        value = Py_NewRef(entry->value);

        if (comma) {
            if (PyUnicodeWriter_WriteChar(writer, ',') < 0) {
                goto fail;
            }
            if (PyUnicodeWriter_WriteChar(writer, ' ') < 0) {
                goto fail;
            }
        }
        if (show_keys) {
            /* Fast path: ASCII keys without characters that would be escaped
             * by repr() can be wrapped in single quotes directly. Falls back
             * to PyUnicodeWriter_WriteRepr for keys containing quotes,
             * backslashes, or non-printable characters so the output stays
             * a valid Python string literal. */
            int fast = 0;
            if (PyUnicode_IS_ASCII(key)) {
                Py_ssize_t klen = PyUnicode_GET_LENGTH(key);
                const unsigned char* kdata =
                    (const unsigned char*)PyUnicode_DATA(key);
                fast = 1;
                for (Py_ssize_t ki = 0; ki < klen; ++ki) {
                    unsigned char c = kdata[ki];
                    if (c < 0x20 || c == 0x7f || c == '\'' || c == '\\') {
                        fast = 0;
                        break;
                    }
                }
            }
            if (fast) {
                if (PyUnicodeWriter_WriteChar(writer, '\'') < 0) {
                    goto fail;
                }
                if (PyUnicodeWriter_WriteStr(writer, key) < 0) {
                    goto fail;
                }
                if (PyUnicodeWriter_WriteChar(writer, '\'') < 0) {
                    goto fail;
                }
            } else {
                if (PyUnicodeWriter_WriteRepr(writer, key) < 0) {
                    goto fail;
                }
            }
        }
        if (show_keys && show_values) {
            if (PyUnicodeWriter_WriteChar(writer, ':') < 0) {
                goto fail;
            }
            if (PyUnicodeWriter_WriteChar(writer, ' ') < 0) {
                goto fail;
            }
        }
        if (show_values) {
            if (PyUnicodeWriter_WriteRepr(writer, value) < 0) {
                goto fail;
            }
        }

        comma = true;
        Py_CLEAR(key);
        Py_CLEAR(value);
    }

    if (PyUnicodeWriter_WriteChar(writer, ')') < 0) {
        goto fail;
    }
    if (PyUnicodeWriter_WriteChar(writer, '>') < 0) {
        goto fail;
    }
    Py_CLEAR(name);
    Py_ReprLeave(obj);
    return PyUnicodeWriter_Finish(writer);
fail:
    Py_CLEAR(key);
    Py_CLEAR(value);
    Py_CLEAR(name);
    PyUnicodeWriter_Discard(writer);
    Py_ReprLeave(obj);
    return NULL;
}

/***********************************************************************/

static inline int
multidict_tp_traverse(MultiDictObject* md, visitproc visit, void* arg)
{
    Py_VISIT(Py_TYPE(md));
    Py_VISIT(md->state->mod);

#ifdef Py_GIL_DISABLED
    /* A table waiting on md->retired still owns its entries' references, so
       a cycle running through them is invisible to the collector unless they
       are reported here too. Only md_clear() and _md_install_keys() retire
       a table with entries left, since _md_rebuild() zeroes nentries once
       it has handed ownership to the new table, so nothing is reported
       twice. Reading the list without the lock is what the walk below
       already relies on: the collector stops the world, and nothing may
       block here, since a stopped thread can hold any lock this would
       take. */
    for (htkeys_t* t = (htkeys_t*)atomic_load_ptr((void* const*)&md->retired);
         t != NULL;
         t = t->retired_next) {
        entry_t* retired_entries = htkeys_entries(t);
        for (Py_ssize_t pos = 0; pos < t->nentries; pos++) {
            entry_t* entry = htkeys_entry_at(t, retired_entries, pos);
            if (!entry_is_hole(t->kind, entry)) {
                Py_VISIT(entry->key);
                Py_VISIT(entry->value);
            }
        }
    }
#endif

    if (md->used == 0) {
        return 0;
    }

    entry_t* entries = htkeys_entries(md->keys);
    for (Py_ssize_t pos = 0; pos < md->keys->nentries; pos++) {
        entry_t* entry = htkeys_entry_at(md->keys, entries, pos);
        if (!entry_is_hole(md->keys->kind, entry)) {
            Py_VISIT(entry->key);
            Py_VISIT(entry->value);
        }
    }

    return 0;
}

static inline int
md_clear(MultiDictObject* md)
{
    if (md->keys == NULL || md->keys == &empty_htkeys) {
#ifdef Py_GIL_DISABLED
        /* There is nothing to retire, but an earlier drain may have left a
           table on md->retired for the next one to free, and this clear can
           be the object's teardown, after which there is no next one. The
           count is zero by then, since a lock-free reader reaches md through
           a live reference, so this drain does free it. */
        _md_drain_retired(md);
#endif
        return 0;
    }
    bump_version(md);

    // Publish the empty table before releasing any entry's reference: a
    // decref below may run arbitrary Python code (a __del__), which can
    // suspend this critical section. If md->keys still pointed at the old
    // table while that happens, a concurrent, correctly-locked reader
    // could observe entries mid-clear (identity already NULL, key/value
    // not yet). Swapping first means a suspended thread only ever sees
    // either the fully-populated old table or the fully-empty one.
    htkeys_t* old_keys = md->keys;
    htkeys_t* new_keys = (htkeys_t*)&empty_htkeys;
    if (UNLIKELY(md->batches != 0)) {
        new_keys = _md_new_keys_after_holes(md, 0, md_fresh_kind(md));
        if (new_keys == NULL) {
            return -1;
        }
    }
    store_used(md, 0);
    store_keys(md, new_keys);
    update_marks_moved(md);
    _md_release_keys(md, old_keys);
    ASSERT_CONSISTENT(md);
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif
