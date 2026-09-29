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
GROWTH_RATE(MultiDictObject* md)
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
    entry_t* entries = htkeys_entries(keys);
    Py_ssize_t nentries = keys->nentries;
    for (Py_ssize_t i = 0; i < nentries; i++) {
        Py_XDECREF(entries[i].identity);
        Py_XDECREF(entries[i].key);
        Py_XDECREF(entries[i].value);
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
       comparing the raw pointer alone (see _md_replace()'s
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
COLD NOINLINE static int
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

    htkeys_t* newkeys = htkeys_new_unfilled(MD_POOLS(md), log2_newsize);
    if (newkeys == NULL) {
        return -1;
    }
    entry_t* newentries = htkeys_entries(newkeys);
    memcpy(newentries,
           htkeys_entries(oldkeys),
           (size_t)nentries * sizeof(entry_t));
    htkeys_zero_entries(newkeys, nentries);
    htkeys_build_indices_with_holes(newkeys, newentries, nentries);
    newkeys->usable -= nentries;
    newkeys->nentries = nentries;

    _md_publish_rebuilt(md, oldkeys, newkeys);
    return 0;
}

/* A table for md's new contents while an update() or merge() is in
   flight: it starts with as many holes as md has entries now, so the new
   entries take indices none of the batch's marks name. Holds `extra` more.
   */
COLD NOINLINE static htkeys_t*
_md_new_keys_after_holes(MultiDictObject* md, Py_ssize_t extra)
{
    Py_ssize_t nholes = md->keys->nentries;
    uint8_t log2_size = estimate_log2_keysize(nholes + extra);
    if (!htkeys_size_fits(log2_size)) {
        PyErr_NoMemory();
        return NULL;
    }
    htkeys_t* keys = htkeys_new(MD_POOLS(md), log2_size);
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
    htkeys_t* newkeys = htkeys_new_unfilled(MD_POOLS(md), log2_newsize);
    if (newkeys == NULL) {
        return -1;
    }

    htkeys_t* oldkeys = md->keys;
    Py_ssize_t numentries = md->used;
    entry_t* oldentries = htkeys_entries(oldkeys);
    entry_t* newentries = htkeys_entries(newkeys);
    Py_ssize_t filled;
    if (oldkeys->nentries == numentries) {
        memcpy(newentries, oldentries, (size_t)numentries * sizeof(entry_t));
        filled = numentries;
    } else {
        entry_t* new_ep = newentries;
        entry_t* old_ep = oldentries;
        Py_ssize_t oldnumentries = oldkeys->nentries;
        for (Py_ssize_t i = 0; i < oldnumentries; ++i, ++old_ep) {
            if (old_ep->identity != NULL) {
                *new_ep++ = *old_ep;
            }
        }
        filled = new_ep - newentries;
    }
    /* What the copy actually wrote, rather than md->used: the two agree,
       but taking the count from the copy means a table can never be
       published over entries nothing has written. */
    assert(filled == numentries);
    htkeys_zero_entries(newkeys, filled);

    htkeys_build_indices(newkeys, newentries, numentries);

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
md_init(MultiDictObject* md, bool is_ci, Py_ssize_t minused)
{
    assert(md->state != NULL);
    htkeys_t* new_keys = (htkeys_t*)&empty_htkeys;

    if (UNLIKELY(md->batches != 0)) {
        new_keys = _md_new_keys_after_holes(md, minused);
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

        new_keys = htkeys_new(MD_POOLS(md), log2_newsize);
        if (new_keys == NULL) return -1;
    }

    _md_install_keys(md, new_keys, 0, is_ci, MultiDict_EVENT_CLEARED);
    return 0;
}

/* md_clone_from_ht() while an update() or merge() on md is in flight:
   other's entries go after the holes _md_new_keys_after_holes() leaves. */
COLD NOINLINE static int
_md_clone_after_holes(MultiDictObject* md, MultiDictObject* other)
{
    htkeys_t* keys = _md_new_keys_after_holes(md, other->used);
    if (keys == NULL) {
        return -1;
    }
    entry_t* entries = htkeys_entries(keys);
    entry_t* dst = entries + keys->nentries;
    entry_t* src = htkeys_entries(other->keys);
    for (Py_ssize_t i = 0; i < other->keys->nentries; i++, src++) {
        if (src->identity != NULL) {
            dst->identity = Py_NewRef(src->identity);
            dst->key = Py_NewRef(src->key);
            dst->value = Py_NewRef(src->value);
            dst->hash = src->hash;
            dst++;
        }
    }
    Py_ssize_t nentries = dst - entries;
    keys->usable -= nentries - keys->nentries;
    keys->nentries = nentries;
    htkeys_build_indices_with_holes(keys, entries, nentries);
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
        keys = htkeys_alloc_sized(MD_POOLS(md), src->log2_size, size);
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
        for (Py_ssize_t idx = 0; idx < keys->nentries; idx++, entry++) {
            Py_XINCREF(entry->identity);
            Py_XINCREF(entry->key);
            Py_XINCREF(entry->value);
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

static inline int
md_add_with_hash_steal_refs(MultiDictObject* md, Py_hash_t hash,
                            PyObject* identity, PyObject* key, PyObject* value)
{
    htkeys_t* keys = md->keys;
    if (keys->usable <= 0 || keys == &empty_htkeys) {
        /* Need to resize. */
        if (_md_resize_for_add(md) < 0) {
            return -1;
        }
        keys = md->keys;  // updated by resizing
    }

    Py_ssize_t hashpos = htkeys_find_empty_slot(keys, hash);
    htkeys_set_index(keys, hashpos, keys->nentries);

    entry_t* entry = htkeys_entries(keys) + keys->nentries;
    assert(entry->identity == NULL && entry->key == NULL &&
           entry->value == NULL);

    /* identity is published last: it's the field a lock-free reader
       checks first (before ever touching hash/key/value), treating
       NULL as "not populated yet, keep probing". See the comment
       above load_identity(). The GIL build has no reader to order
       against, so it just follows along. The entry is carved out of
       the zeroed tail past every live one, so value is still NULL and
       publish_value() is enough; nothing here has an old reference to
       drop. */
    entry->key = key;
    store_hash(entry, hash);
    publish_value(entry, value);
    publish_identity(entry, identity);

    bump_version(md);
    add_used(md, 1);
    keys->usable -= 1;
    keys->nentries += 1;
    md_watch_record(
        md, MultiDict_EVENT_ADDED, identity, hash, key, value, NULL);
    return 0;
}

static inline int
md_add_with_hash(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
                 PyObject* key, PyObject* value)
{
    Py_INCREF(identity);
    Py_INCREF(key);
    Py_INCREF(value);
    if (md_add_with_hash_steal_refs(md, hash, identity, key, value) < 0) {
        Py_DECREF(identity);
        Py_DECREF(key);
        Py_DECREF(value);
        return -1;
    }
    return 0;
}

static inline int
_md_add_for_upd_steal_refs(MultiDictObject* md, Py_hash_t hash,
                           PyObject* identity, PyObject* key, PyObject* value,
                           update_marks_t* marks)
{
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
    Py_ssize_t hashpos = htkeys_find_empty_slot(keys, hash);
    htkeys_set_index(keys, hashpos, keys->nentries);

    entry_t* entry = htkeys_entries(keys) + keys->nentries;
    assert(entry->identity == NULL && entry->key == NULL &&
           entry->value == NULL);

    /* See md_add_with_hash_steal_refs() for the ordering. */
    entry->key = key;
    store_hash(entry, hash);
    publish_value(entry, value);
    publish_identity(entry, identity);

    bump_version(md);
    add_used(md, 1);
    keys->usable -= 1;
    keys->nentries += 1;
    md_watch_record(
        md, MultiDict_EVENT_ADDED, identity, hash, key, value, NULL);
    return 0;
}

static inline int
md_add_for_upd(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
               PyObject* key, PyObject* value, update_marks_t* marks)
{
    Py_INCREF(identity);
    Py_INCREF(key);
    Py_INCREF(value);
    if (_md_add_for_upd_steal_refs(md, hash, identity, key, value, marks) <
        0) {
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
static inline int
_md_add_locked(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
               PyObject* key, PyObject* value)
{
    int ret = md_add_with_hash(md, hash, identity, key, value);
    ASSERT_CONSISTENT(md);
    return ret;
}

static inline int
md_add(MultiDictObject* md, PyObject* key, PyObject* value)
{
    PyObject* identity;
    Py_hash_t hash;
    if (md_calc_identity_hash(md, key, &identity, &hash) < 0) {
        return -1;
    }
    int ret;
    bool flush;
    Py_BEGIN_CRITICAL_SECTION(md);
    ret = _md_add_locked(md, identity, hash, key, value);
    flush = md_watch_pending(md);
    Py_END_CRITICAL_SECTION();
    Py_DECREF(identity);
    md_watch_flush_if(md, flush);
    return ret;
}

/* Removes entry, whose index is at `slot`, and hands its references to the
   caller; see _md_del_at() on the order. */
ALWAYS_INLINE static inline void
_md_unlink_at(MultiDictObject* md, size_t slot, entry_t* entry,
              PyObject** pidentity, PyObject** pkey, PyObject** pvalue)
{
    *pidentity = load_identity(entry);
    *pkey = entry->key;
    *pvalue = load_value(entry);

    reset_identity(entry);
    entry->key = NULL;
    reset_value(entry);
    htkeys_set_index(md->keys, (Py_ssize_t)slot, DKIX_DUMMY);
    add_used(md, -1);
}

ALWAYS_INLINE static inline void
_md_del_at(MultiDictObject* md, size_t slot, entry_t* entry)
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
    _md_unlink_at(md, slot, entry, &identity, &key, &value);

    Py_XDECREF(identity);
    Py_XDECREF(key);
    Py_XDECREF(value);
}

/* _md_del_at() variant that defers the decref (see reflist_t);
 * used by _md_replace()'s duplicate-cleanup path on both builds. */
static inline int
_md_del_at_deferred(MultiDictObject* md, size_t slot, entry_t* entry,
                    reflist_t* defer)
{
    htkeys_t* keys = md->keys;
    assert(keys != &empty_htkeys);
    PyObject* identity = load_identity(entry);
    PyObject* key = entry->key;
    PyObject* value = load_value(entry);

    reset_identity(entry);
    entry->key = NULL;
    reset_value(entry);
    htkeys_set_index(keys, (Py_ssize_t)slot, DKIX_DUMMY);
    add_used(md, -1);

    int ret = reflist_push(defer, identity);
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
_md_del_at_held(MultiDictObject* md, size_t slot, entry_t* entry,
                removed_pairs_t* removed)
{
    htkeys_t* keys = md->keys;
    assert(keys != &empty_htkeys);
    PyObject* identity = load_identity(entry);
    PyObject* key = entry->key;
    PyObject* value = load_value(entry);

    reset_identity(entry);
    entry->key = NULL;
    reset_value(entry);
    htkeys_set_index(keys, (Py_ssize_t)slot, DKIX_DUMMY);
    add_used(md, -1);

    // An exact str: freeing it runs no code
    Py_DECREF(identity);
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
               removed_pairs_t* removed, bool watched)
{
    bool found = false;
    int ret = 0;

    htkeysiter_t iter;
    htkeysiter_init(&iter, md->keys, hash);

    entry_t* entries = htkeys_entries(md->keys);

    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t* entry = entries + iter.index;
        if (hash != entry->hash) {
            continue;
        }
        if (!str_cmp(entry->identity, identity)) {
            continue;
        }

        if (watched) {
            if (!found) {
                md_watch_record_simple(md, MultiDict_EVENT_BATCH_BEGIN);
            }
            md_watch_record(md,
                            MultiDict_EVENT_DELETED,
                            entry->identity,
                            hash,
                            entry->key,
                            entry->value,
                            NULL);
        }
        found = true;
        if (_md_del_at_held(md, iter.slot, entry, removed) < 0) {
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
    return _md_del_locked(md, identity, hash, removed, true);
}

NOINLINE static int
md_del(MultiDictObject* md, PyObject* key)
{
    PyObject* identity;
    Py_hash_t hash;
    if (md_calc_identity_hash(md, key, &identity, &hash) < 0) {
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
        found = _md_del_locked(md, identity, hash, &removed, false);
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
    entry_t* entry = entries + pos->pos;

    while (entry->identity == NULL) {
        pos->pos += 1;
        if (pos->pos >= md->keys->nentries) {
            goto cleanup;
        }
        entry += 1;
    }

    if (pidentity) {
        *pidentity = Py_NewRef(entry->identity);
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
    entry_t* entry = entries + pos->pos;

    while (entry->identity == NULL) {
        pos->pos -= 1;
        if (pos->pos < 0) {
            goto cleanup;
        }
        entry -= 1;
    }

    if (pidentity) {
        *pidentity = Py_NewRef(entry->identity);
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

static inline int
_md_contains_locked(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                    PyObject** pret)
{
    htkeysiter_t iter;
    htkeysiter_init(&iter, md->keys, hash);
    entry_t* entries = htkeys_entries(md->keys);

    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (UNLIKELY(iter.index < 0)) {
            continue;
        }
        entry_t* entry = entries + iter.index;
        if (hash != entry->hash) {
            continue;
        }
        if (str_cmp(identity, entry->identity)) {
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

static inline int
_md_contains_lockfree(MultiDictObject* md, PyObject* identity, Py_hash_t hash)
{
    htkeys_t* keys = _md_reader_enter(md);
    htkeysiter_t iter;
    htkeysiter_init(&iter, keys, hash);
    entry_t* entries = htkeys_entries(keys);

    int result = 0;
    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (UNLIKELY(iter.index < 0)) {
            continue;
        }
        entry_t* entry = entries + iter.index;

        /* Hash first, so a mismatch costs no reference traffic; then a
           pointer-equal identity is a match outright, since the probe's
           own reference keeps that object alive and its address cannot
           be reused. Only a different object needs the reference for
           the string compare. */
        if (load_hash(entry) != hash) {
            continue;
        }
        PyObject* entry_identity = load_identity(entry);
        if (entry_identity == NULL) {
            continue;  // not populated (or deleted); keep probing
        }
        if (entry_identity != identity) {
            entry_identity = try_get_ref(&entry->identity);
            if (entry_identity == NULL) {
                if (load_identity(entry) == NULL) {
                    continue;
                }
                result = 2;  // _MD_NEED_LOCK
                break;
            }
            bool matched = str_cmp(identity, entry_identity);
            Py_DECREF(entry_identity);
            if (!matched) {
                continue;
            }
        }
        result = 1;
        break;
    }

    _md_reader_exit(md, keys);
    return result;
}

#endif /* Py_GIL_DISABLED */

static inline int
md_contains(MultiDictObject* md, PyObject* key)
{
    if (!PyUnicode_Check(key)) {
        return 0;
    }

    PyObject* identity;
    Py_hash_t hash;
    if (md_calc_identity_hash(md, key, &identity, &hash) < 0) {
        return -1;
    }

    int result;
#ifdef Py_GIL_DISABLED
    result = _md_contains_lockfree(md, identity, hash);
    if (result != 2 /* _MD_NEED_LOCK */) {
        Py_DECREF(identity);
        return result;
    }
    Py_BEGIN_CRITICAL_SECTION(md);
    result = _md_contains_locked(md, identity, hash, NULL);
    Py_END_CRITICAL_SECTION();
#else
    result = _md_contains_locked(md, identity, hash, NULL);
#endif
    Py_DECREF(identity);
    return result;
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
    if (md_calc_identity_hash(md, key, &identity, &hash) < 0) {
        return -1;
    }

    int result;
    Py_BEGIN_CRITICAL_SECTION(md);
    result = _md_contains_locked(md, identity, hash, pret);
    Py_END_CRITICAL_SECTION();
    Py_DECREF(identity);
    return result;
}

static inline int
_md_get_one_locked(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                   PyObject** ret)
{
    htkeysiter_t iter;
    htkeysiter_init(&iter, md->keys, hash);
    entry_t* entries = htkeys_entries(md->keys);

    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (UNLIKELY(iter.index < 0)) {
            continue;
        }
        entry_t* entry = entries + iter.index;
        if (hash != entry->hash) {
            continue;
        }
        if (str_cmp(identity, entry->identity)) {
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

static inline int
_md_get_one_lockfree(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                     PyObject** ret)
{
    htkeys_t* keys = _md_reader_enter(md);
    htkeysiter_t iter;
    htkeysiter_init(&iter, keys, hash);
    entry_t* entries = htkeys_entries(keys);

    int result = 0;
    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (UNLIKELY(iter.index < 0)) {
            continue;
        }
        entry_t* entry = entries + iter.index;

        /* See _md_contains_lockfree() on the order. */
        if (load_hash(entry) != hash) {
            continue;
        }
        PyObject* entry_identity = load_identity(entry);
        if (entry_identity == NULL) {
            continue;  // not populated (or deleted); keep probing
        }
        if (entry_identity != identity) {
            entry_identity = try_get_ref(&entry->identity);
            if (entry_identity == NULL) {
                if (load_identity(entry) == NULL) {
                    continue;
                }
                result = _MD_NEED_LOCK;  // racing a concurrent change
                break;
            }
            bool matched = str_cmp(identity, entry_identity);
            Py_DECREF(entry_identity);
            if (!matched) {
                continue;
            }
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

static inline int
md_get_one(MultiDictObject* md, PyObject* key, PyObject** ret)
{
    PyObject* identity = md_calc_identity(md, key);
    if (identity == NULL) {
        return -1;
    }
    Py_hash_t hash = unicode_hash(identity);
    if (hash == -1) {
        Py_DECREF(identity);
        return -1;
    }

    int result = _md_get_one_lockfree(md, identity, hash, ret);
    if (result != _MD_NEED_LOCK) {
        Py_DECREF(identity);
        return result;
    }

    Py_BEGIN_CRITICAL_SECTION(md);
    result = _md_get_one_locked(md, identity, hash, ret);
    Py_END_CRITICAL_SECTION();
    Py_DECREF(identity);
    return result;
}

#undef _MD_NEED_LOCK

#else /* !Py_GIL_DISABLED */

static inline int
md_get_one(MultiDictObject* md, PyObject* key, PyObject** ret)
{
    PyObject* identity = md_calc_identity(md, key);
    if (identity == NULL) {
        return -1;
    }
    Py_hash_t hash = unicode_hash(identity);
    if (hash == -1) {
        Py_DECREF(identity);
        return -1;
    }
    int result = _md_get_one_locked(md, identity, hash, ret);
    Py_DECREF(identity);
    return result;
}

#endif /* Py_GIL_DISABLED */

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
        entry_t* entry = entries + pos;
        if (entry->identity == NULL) {
            continue;  // deleted
        }
        if (bitmap_test(&collected, pos)) {
            continue;  // collected already under its first key
        }

        /* Equal keys sit on one hash chain in insertion order. Only the
           list allocation below can run Python in this walk. */
        Py_hash_t hash = entry->hash;
        htkeysiter_t iter;
        htkeysiter_init(&iter, md->keys, hash);
        for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
            if (iter.index < 0) {
                continue;
            }
            entry_t* e = entries + iter.index;
            if (e->hash != hash || !str_cmp(entry->identity, e->identity)) {
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
static inline int
_md_set_default_locked(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                       PyObject* key, PyObject* value, PyObject** result)
{
    ASSERT_CONSISTENT(md);

    htkeysiter_t iter;
    htkeysiter_init(&iter, md->keys, hash);
    entry_t* entries = htkeys_entries(md->keys);

    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t* entry = entries + iter.index;

        if (hash != entry->hash) {
            continue;
        }
        if (str_cmp(identity, entry->identity)) {
            ASSERT_CONSISTENT(md);
            *result = Py_NewRef(entry->value);
            return 1;
        }
    }

    if (md_add_with_hash(md, hash, identity, key, value) < 0) {
        return -1;
    }

    ASSERT_CONSISTENT(md);
    *result = Py_NewRef(value);
    return 0;
}

static inline int
md_set_default(MultiDictObject* md, PyObject* key, PyObject* value,
               PyObject** result)
{
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
    if (md_calc_identity_hash(md, key, &identity, &hash) < 0) {
        return -1;
    }
    int ret;
    bool flush;
    Py_BEGIN_CRITICAL_SECTION(md);
    ret = _md_set_default_locked(md, identity, hash, key, value, result);
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
                   PyObject** ret, bool watched)
{
    htkeysiter_t iter;
    htkeysiter_init(&iter, md->keys, hash);
    entry_t* entries = htkeys_entries(md->keys);

    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t* entry = entries + iter.index;

        if (hash != entry->hash) {
            continue;
        }
        if (str_cmp(identity, entry->identity)) {
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
            _md_del_at(md, iter.slot, entry);
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
    return _md_pop_one_locked(md, identity, hash, ret, true);
}

static inline int
md_pop_one(MultiDictObject* md, PyObject* key, PyObject** ret)
{
    PyObject* identity;
    Py_hash_t hash;
    if (md_calc_identity_hash(md, key, &identity, &hash) < 0) {
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
        result = _md_pop_one_locked(md, identity, hash, ret, false);
    }
    Py_END_CRITICAL_SECTION();
    Py_DECREF(identity);
    md_watch_flush_if(md, flush);
    return result;
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
static inline int
_md_get_all_locked(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                   reflist_t* values)
{
    Py_ssize_t count =
        md_walk_with_hash(md, identity, hash, false, _md_getall_visit, values);
    return count < 0 ? -1 : 0;
}

static inline int
md_get_all(MultiDictObject* md, PyObject* key, PyObject** ret)
{
    *ret = NULL;
    PyObject* identity;
    Py_hash_t hash;
    if (md_calc_identity_hash(md, key, &identity, &hash) < 0) {
        return -1;
    }
    reflist_t values;
    reflist_init(&values);
    int tmp;
    Py_BEGIN_CRITICAL_SECTION(md);
    tmp = _md_get_all_locked(md, identity, hash, &values);
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

    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t* entry = entries + iter.index;

        if (hash != entry->hash) {
            continue;
        }
        if (str_cmp(identity, entry->identity)) {
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
            if (_md_del_at_held(md, iter.slot, entry, removed) < 0) {
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
    if (md_calc_identity_hash(md, key, &identity, &hash) < 0) {
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

static inline PyObject*
md_pop_item(MultiDictObject* md)
{
    if (md->used == 0) {
        PyErr_SetString(PyExc_KeyError, "empty multidict");
        return NULL;
    }

    entry_t* entries = htkeys_entries(md->keys);

    Py_ssize_t pos = md->keys->nentries - 1;
    entry_t* entry = entries + pos;
    while (pos >= 0 && entry->identity == NULL) {
        pos--;
        entry--;
    }
    assert(pos >= 0);

    htkeysiter_t iter;
    htkeysiter_init(&iter, md->keys, entry->hash);

    for (; iter.index != pos; htkeysiter_next(&iter)) {
    }
    md_watch_record(md,
                    MultiDict_EVENT_DELETED,
                    entry->identity,
                    entry->hash,
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
        md->keys->nentries = pos;
    }
    /* The entry's refs, taken over: building the result below can run
       Python code that mutates md (an istr key's __str__, or on 3.10 and
       3.11 a collection the tuple triggers), so it runs once the pair is
       gone. */
    PyObject* identity;
    PyObject* key;
    PyObject* value;
    _md_unlink_at(md, iter.slot, entry, &identity, &key, &value);
    bump_version(md);
    ASSERT_CONSISTENT(md);

    Py_SETREF(key, md_calc_key(md, key, identity));
    Py_DECREF(identity);
    if (key == NULL) {
        Py_DECREF(value);
        return NULL;
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

/* Returns 1 when `key` isn't there, leaving the add to the caller.
 * `watched` is a constant at both call sites; see _md_del_locked(). */
ALWAYS_INLINE static inline int
_md_replace(MultiDictObject* md, PyObject* key, PyObject* value,
            PyObject* identity, Py_hash_t hash, reflist_t* defer, bool watched)
{
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
            entry_t* entry = entries + iter.index;
            if (entry->hash != hash || !str_cmp(identity, entry->identity)) {
                continue;
            }
            if (skip_first) {
                skip_first = false;
                replaced = iter.index;
                continue;
            }
            if (!found) {
                found = true;
                replaced = iter.index;
                // old_key/old_value decref deferred -- see reflist_t
                PyObject* old_key = entry->key;
                PyObject* old_value = load_value(entry);
                entry->key = Py_NewRef(key);
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
                /* Push both unconditionally, not with `||`: a failed
                   first push already decref'd old_key itself (see
                   reflist_push()'s doc comment), but short-circuiting
                   past the second push would leak old_value -- neither
                   deferred nor decref'd. */
                int push_ret = reflist_push(defer, old_key);
                if (reflist_push(defer, old_value) < 0) {
                    push_ret = -1;
                }
                if (push_ret < 0) {
                    return -1;
                }
            } else {
                if (watched) {
                    md_watch_record(md,
                                    MultiDict_EVENT_DELETED,
                                    entry->identity,
                                    hash,
                                    entry->key,
                                    entry->value,
                                    NULL);
                }
                if (_md_del_at_deferred(md, iter.slot, entry, defer) < 0) {
                    return -1;
                }
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

COLD static int
_md_replace_watched(MultiDictObject* md, PyObject* key, PyObject* value,
                    PyObject* identity, Py_hash_t hash, reflist_t* defer)
{
    md_watch_record_simple(md, MultiDict_EVENT_BATCH_BEGIN);
    int ret = _md_replace(md, key, value, identity, hash, defer, true);
    if (ret > 0) {
        ret = md_add_with_hash(md, hash, identity, key, value);
    }
    md_watch_record_simple(md, MultiDict_EVENT_BATCH_END);
    return ret;
}

NOINLINE static int
md_replace(MultiDictObject* md, PyObject* key, PyObject* value)
{
    PyObject* identity;
    Py_hash_t hash;
    if (md_calc_identity_hash(md, key, &identity, &hash) < 0) {
        return -1;
    }
    reflist_t defer;
    reflist_init(&defer);
    int ret;
    // see md_del()
    bool flush = false;
    Py_BEGIN_CRITICAL_SECTION(md);
    if (UNLIKELY(md->watch != NULL)) {
        ret = _md_replace_watched(md, key, value, identity, hash, &defer);
        flush = md_watch_pending(md);
    } else {
        ret = _md_replace(md, key, value, identity, hash, &defer, false);
        if (ret > 0) {
            ret = md_add_with_hash(md, hash, identity, key, value);
        }
    }
    ASSERT_CONSISTENT(md);
    Py_END_CRITICAL_SECTION();
    reflist_clear(&defer);
    Py_DECREF(identity);
    md_watch_flush_if(md, flush);
    return ret;
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
        entry_t* entry1 = lft_entries + pos1;
        if (entry1->identity == NULL) {
            pos1++;
            continue;
        }
        entry_t* entry2 = rht_entries + pos2;
        if (entry2->identity == NULL) {
            pos2++;
            continue;
        }

        if (entry1->hash != entry2->hash) {
            return 0;
        }

        if (!str_cmp(entry1->identity, entry2->identity)) {
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
        Py_CLEAR(key);
        if (ret < 0) {
            Py_CLEAR(avalue);
            return -1;
        }

        if (bvalue == NULL) {
            Py_CLEAR(avalue);
            return 0;
        }

        int eq = PyObject_RichCompareBool(avalue, bvalue, Py_EQ);
        Py_CLEAR(bvalue);
        Py_CLEAR(avalue);

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

    for (Py_ssize_t pos = 0; pos < md->keys->nentries; ++pos) {
        if (version != md->version) {
            PyErr_SetString(PyExc_RuntimeError,
                            "MultiDict changed during iteration");
            goto fail;  // discard the writer instead of leaking it
        }
        entry_t* entry = entries + pos;
        if (entry->identity == NULL) {
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
            entry_t* entry = retired_entries + pos;
            if (entry->identity != NULL) {
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
        entry_t* entry = entries + pos;
        if (entry->identity != NULL) {
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
        new_keys = _md_new_keys_after_holes(md, 0);
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
