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
#include "dict.h"
#include "freethreading.h"
#include "htkeys.h"
#include "identity.h"
#include "istr.h"
#include "md_debug.h"
#include "reflist.h"
#include "state.h"
#include "update_marks.h"
#include "walk.h"

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
htkeys_free(), reached through _md_free_retired()) cannot race the
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

static inline void
_md_free_retired(pool_t* pools, htkeys_t* keys)
{
    entry_t* entries = htkeys_entries(keys);
    /* Only md_clear()'s retired tables have live entries to release here:
   _md_resize()'s old table has its ownership already transferred to
   the new table via memcpy, so its nentries is reset to 0 before
   retirement, making this loop a no-op for that case. */
    for (Py_ssize_t i = 0; i < keys->nentries; i++) {
        Py_CLEAR(entries[i].identity);
        Py_CLEAR(entries[i].key);
        Py_CLEAR(entries[i].value);
    }
    htkeys_free(pools, keys);
}

NOINLINE static void
_md_drain_retired_slow(MultiDictObject* md)
{
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
            _md_free_retired(MD_POOLS(md), t);
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

static inline int
_md_resize(MultiDictObject* md, uint8_t log2_newsize, update_marks_t* marks)
{
    if (log2_newsize >= SIZEOF_SIZE_T * 8) {
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
    if (_update_marks_remap(marks, oldkeys, newkeys, newkeys->usable) < 0) {
        htkeys_free(MD_POOLS(md), newkeys);
        return -1;
    }
    Py_ssize_t numentries = md->used;
    entry_t* oldentries = htkeys_entries(oldkeys);
    entry_t* newentries = htkeys_entries(newkeys);
    Py_ssize_t filled;
    if (oldkeys->nentries == numentries) {
        memcpy(newentries, oldentries, numentries * sizeof(entry_t));
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

    store_keys(md, newkeys);

#ifdef Py_GIL_DISABLED
    /* Bump the version on every resize, not just when a caller's
       own insert/delete/replace would bump it anyway: a freed
       htkeys_t can get reallocated at the very same address by a
       later resize (same size class, common in practice), so code
       elsewhere that detects "did md->keys change under me" by
       comparing the raw pointer alone (see _md_replace()'s and
       _md_update()'s comments, the latter in bulk_update.h) needs a
       companion signal that can't coincidentally repeat. */
    store_version(md, next_version(md->state));

    /* Ownership of oldkeys's entries has already moved to newkeys via
       the memcpy/copy loop above; zeroing nentries tells
       _md_retire()'s cleanup there is nothing left to decref, only
       memory to free. */
    if (oldkeys != &empty_htkeys) {
        oldkeys->nentries = 0;
    }
    _md_retire(md, oldkeys);
#else
    if (oldkeys != &empty_htkeys) {
        htkeys_free(MD_POOLS(md), oldkeys);
    }
#endif

    ASSERT_CONSISTENT(md, marks != NULL);
    return 0;
}

static inline int
_md_shrink(MultiDictObject* md, update_marks_t* marks)
{
#ifdef Py_GIL_DISABLED
    /* The in-place compaction below rewrites the currently-published
       table's entries and indices while md->keys keeps pointing at it
       the whole time -- safe when every reader holds the critical
       section (mutually exclusive with this function), not safe
       against a lock-free reader concurrently walking the very memory
       being rewritten. _md_resize() already has the build-a-new-table,
       swap, retire-the-old-one shape lock-free reads need; reusing it
       at the *current* size does exactly what shrinking means here
       (drop the dummy-slot gaps) without a second, duplicate
       implementation of that shape. */
    return _md_resize(md, md->keys->log2_size, marks);
#else
    htkeys_t* keys = md->keys;
    if (_update_marks_remap(marks, keys, keys, _md_entries_capacity(keys)) <
        0) {
        return -1;
    }
    Py_ssize_t nentries = keys->nentries;
    entry_t* entries = htkeys_entries(keys);
    entry_t* new_ep = entries;
    entry_t* old_ep = entries;
    Py_ssize_t newnentries = nentries;
    for (Py_ssize_t i = 0; i < nentries; ++i, ++old_ep) {
        if (old_ep->identity != NULL) {
            if (new_ep != old_ep) {
                *new_ep = *old_ep;
            }
            new_ep++;
        } else {
            newnentries -= 1;
        }
    }
    keys->nentries = newnentries;
    keys->usable += nentries - newnentries;
    memset(&keys->indices[0], 0xff, ((size_t)1 << keys->log2_index_bytes));
    memset(new_ep, 0, sizeof(entry_t) * (size_t)(nentries - newnentries));
    htkeys_build_indices(keys, entries, newnentries);
    ASSERT_CONSISTENT(md, marks != NULL);
    return 0;
#endif
}

static inline int
_md_resize_for_insert(MultiDictObject* md)
{
    if (md->used < md->keys->nentries) {
        return _md_shrink(md, NULL);
    } else {
        return _md_resize(md, calculate_log2_keysize(GROWTH_RATE(md)), NULL);
    }
}

static inline int
_md_resize_for_update(MultiDictObject* md, update_marks_t* marks)
{
    if (md->used < md->keys->nentries) {
        return _md_shrink(md, marks);
    } else {
        return _md_resize(md, calculate_log2_keysize(GROWTH_RATE(md)), marks);
    }
}

static inline int
_md_reserve(MultiDictObject* md, Py_ssize_t extra_size, update_marks_t* marks)
{
    uint8_t new_size = estimate_log2_keysize(extra_size + md->used);
    if (new_size > md->keys->log2_size) {
        return _md_resize(md, new_size, marks);
    }
    return 0;
}

static inline int
md_reserve(MultiDictObject* md, Py_ssize_t extra_size)
{
    return _md_reserve(md, extra_size, NULL);
}

static inline int
md_clear(MultiDictObject* md);

static inline int
md_init(MultiDictObject* md, bool is_ci, Py_ssize_t minused)
{
    assert(md->state != NULL);
    htkeys_t* new_keys = (htkeys_t*)&empty_htkeys;

    if (minused > USABLE_FRACTION(HT_MINSIZE)) {
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

    md_clear(md);
    md->is_ci = is_ci;
    store_used(md, 0);
    store_version(md, next_version(md->state));
    store_keys(md, new_keys);
    ASSERT_CONSISTENT(md, false);
    return 0;
}

static inline int
md_clone_from_ht(MultiDictObject* md, MultiDictObject* other)
{
    ASSERT_CONSISTENT(other, false);

    htkeys_t* keys = (htkeys_t*)&empty_htkeys;
    htkeys_t* src = other->keys;
    if (src != &empty_htkeys) {
        /* The copy overwrites every byte, so this skips both of the
           memsets htkeys_new() would do; the byte count is a function
           of log2_size alone, which is also what the pool keys on. */
        size_t size = (size_t)htkeys_sizeof(src);
        keys = _htkeys_alloc_sized(MD_POOLS(md), src->log2_size, size);
        if (keys == NULL) {
            return -1;
        }

        memcpy(keys, src, size);
        keys->resume_slots = NULL;
#ifdef Py_GIL_DISABLED
        keys->num_readers = 0;
        keys->retired_next = NULL;
#endif
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

    md_clear(md);
    store_used(md, used);
    store_version(md, next_version(md->state));  // never reuse other's version
    md->is_ci = is_ci;
    store_keys(md, keys);
    ASSERT_CONSISTENT(md, false);
    return 0;
}

static inline Py_ssize_t
md_len(MultiDictObject* md)
{
    return load_used(md);
}

static inline int
_md_add_with_hash_steal_refs(MultiDictObject* md, Py_hash_t hash,
                             PyObject* identity, PyObject* key,
                             PyObject* value)
{
    htkeys_t* keys = md->keys;
    if (keys->usable <= 0 || keys == &empty_htkeys) {
        /* Need to resize. */
        if (_md_resize_for_insert(md) < 0) {
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

    store_version(md, next_version(md->state));
    add_used(md, 1);
    keys->usable -= 1;
    keys->nentries += 1;
    return 0;
}

static inline int
_md_add_with_hash(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
                  PyObject* key, PyObject* value)
{
    Py_INCREF(identity);
    Py_INCREF(key);
    Py_INCREF(value);
    if (_md_add_with_hash_steal_refs(md, hash, identity, key, value) < 0) {
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
        if (_md_resize_for_update(md, marks) < 0) {
            return -1;
        }
        keys = md->keys;  // updated by resizing
    }
    if (bitmap_set(&marks->updated, keys->nentries) < 0) {
        return -1;
    }
    Py_ssize_t hashpos = htkeys_find_empty_slot(keys, hash);
    htkeys_set_index(keys, hashpos, keys->nentries);

    entry_t* entry = htkeys_entries(keys) + keys->nentries;
    assert(entry->identity == NULL && entry->key == NULL &&
           entry->value == NULL);

    /* See _md_add_with_hash_steal_refs() for the ordering. */
    entry->key = key;
    store_hash(entry, hash);
    publish_value(entry, value);
    publish_identity(entry, identity);

    store_version(md, next_version(md->state));
    add_used(md, 1);
    keys->usable -= 1;
    keys->nentries += 1;
    return 0;
}

static inline int
_md_add_for_upd(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
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
    int ret = _md_add_with_hash(md, hash, identity, key, value);
    ASSERT_CONSISTENT(md, false);
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
    Py_BEGIN_CRITICAL_SECTION(md);
    ret = _md_add_locked(md, identity, hash, key, value);
    Py_END_CRITICAL_SECTION();
    Py_DECREF(identity);
    return ret;
}

static inline void
_md_del_at(MultiDictObject* md, size_t slot, entry_t* entry)
{
    htkeys_t* keys = md->keys;
    assert(keys != &empty_htkeys);
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
    PyObject* identity = load_identity(entry);
    PyObject* key = entry->key;
    PyObject* value = load_value(entry);

    reset_identity(entry);
    entry->key = NULL;
    reset_value(entry);
    htkeys_set_index(keys, slot, DKIX_DUMMY);
    add_used(md, -1);

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
    htkeys_set_index(keys, slot, DKIX_DUMMY);
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

/* Deferred half-deletion: entry may be replaced later or finished off by
 * md_post_update() (identity=NULL, used -= 1, slot -> DKIX_DUMMY). Unlike
 * _md_del_at_deferred(), this leaves identity/hash/index live -- a reader's
 * hash-chain scan can still reach this slot -- so each field is reserved
 * and pushed before it's nulled, one at a time: reflist_push()'s OOM
 * fallback would otherwise decref a field's old value immediately while
 * the entry sits in that half-deleted, still-reachable state. */
static inline int
_md_del_at_for_upd_deferred(MultiDictObject* md, size_t slot, entry_t* entry,
                            reflist_t* defer)
{
    (void)md;
    (void)slot;
    assert(md->keys != &empty_htkeys);
    if (_reflist_reserve_one(defer) < 0) {
        return -1;
    }
    PyObject* old_key = entry->key;
    entry->key = NULL;
    reflist_push_reserved(defer, old_key);

    if (_reflist_reserve_one(defer) < 0) {
        return -1;
    }
    PyObject* old_value = load_value(entry);
    reset_value(entry);
    reflist_push_reserved(defer, old_value);
    return 0;
}

/* Caller holds md's critical section. Reports whether anything was removed;
 * md_del() raises the KeyError outside the section. */
static inline bool
_md_del_locked(MultiDictObject* md, PyObject* identity, Py_hash_t hash)
{
    bool found = false;

restart:;
    htkeys_t* keys = md->keys;
    uint64_t version = md->version;
    htkeysiter_t iter;
    htkeysiter_init(&iter, keys, hash);

    entry_t* entries = htkeys_entries(keys);

    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t* entry = entries + iter.index;
        if (hash != entry->hash) {
            continue;
        }
        if (!_str_cmp(entry->identity, identity)) {
            continue;
        }

        found = true;
        _md_del_at(md, iter.slot, entry);
        // the decref can run a __del__ that lets another thread resize
        if (UNLIKELY(md->keys != keys || md->version != version)) {
            goto restart;
        }
    }

    if (found) {
        store_version(md, next_version(md->state));
    }
    ASSERT_CONSISTENT(md, false);
    return found;
}

static inline int
md_del(MultiDictObject* md, PyObject* key)
{
    PyObject* identity;
    Py_hash_t hash;
    if (md_calc_identity_hash(md, key, &identity, &hash) < 0) {
        return -1;
    }
    bool found;
    Py_BEGIN_CRITICAL_SECTION(md);
    found = _md_del_locked(md, identity, hash);
    Py_END_CRITICAL_SECTION();
    Py_DECREF(identity);
    if (!found) {
        PyErr_SetObject(PyExc_KeyError, key);
        return -1;
    }
    return 0;
}

static inline void
md_init_pos(MultiDictObject* md, md_pos_t* pos)
{
    pos->pos = 0;
    pos->version = md->version;
}

static inline int
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
        *pkey = _md_ensure_key(md, entry);  // last entry access
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
        *pkey = _md_ensure_key(md, entry);  // last entry access
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
        if (_str_cmp(identity, entry->identity)) {
            if (pret != NULL) {
                *pret = _md_ensure_key(md, entry);
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

        PyObject* entry_identity = try_get_ref(&entry->identity);
        if (entry_identity == NULL) {
            if (load_identity(entry) == NULL) {
                continue;  // not populated (or deleted); keep probing
            }
            result = 2;  // _MD_NEED_LOCK
            break;
        }

        if (load_hash(entry) != hash) {
            Py_DECREF(entry_identity);
            continue;
        }

        bool matched = _str_cmp(identity, entry_identity);
        Py_DECREF(entry_identity);
        if (matched) {
            result = 1;
            break;
        }
    }

    _md_reader_exit(md, keys);
    return result;
}

#endif /* Py_GIL_DISABLED */

static inline int
md_contains(MultiDictObject* md, PyObject* key, PyObject** pret)
{
    if (!PyUnicode_Check(key)) {
        return 0;
    }

    PyObject* identity = md_calc_identity(md, key);
    if (identity == NULL) {
        if (pret != NULL) {
            *pret = NULL;
        }
        return -1;
    }

    Py_hash_t hash = _unicode_hash(identity);
    if (hash == -1) {
        Py_DECREF(identity);
        if (pret != NULL) {
            *pret = NULL;
        }
        return -1;
    }

    int result;
#ifdef Py_GIL_DISABLED
    if (pret == NULL) {
        result = _md_contains_lockfree(md, identity, hash);
        if (result != 2 /* _MD_NEED_LOCK */) {
            Py_DECREF(identity);
            return result;
        }
    }
    Py_BEGIN_CRITICAL_SECTION(md);
    result = _md_contains_locked(md, identity, hash, pret);
    Py_END_CRITICAL_SECTION();
#else
    result = _md_contains_locked(md, identity, hash, pret);
#endif
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
        if (_str_cmp(identity, entry->identity)) {
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

        PyObject* entry_identity = try_get_ref(&entry->identity);
        if (entry_identity == NULL) {
            if (load_identity(entry) == NULL) {
                continue;  // not populated (or deleted); keep probing
            }
            result = _MD_NEED_LOCK;  // racing a concurrent change
            break;
        }

        if (load_hash(entry) != hash) {
            Py_DECREF(entry_identity);
            continue;
        }

        bool matched = _str_cmp(identity, entry_identity);
        Py_DECREF(entry_identity);
        if (!matched) {
            continue;
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
    Py_hash_t hash = _unicode_hash(identity);
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
    Py_hash_t hash = _unicode_hash(identity);
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

        /* Equal keys sit on one hash chain in insertion order. Nothing in
           this walk runs Python. */
        Py_hash_t hash = entry->hash;
        htkeysiter_t iter;
        htkeysiter_init(&iter, md->keys, hash);
        for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
            if (iter.index < 0) {
                continue;
            }
            entry_t* e = entries + iter.index;
            if (e->hash != hash || !_str_cmp(entry->identity, e->identity)) {
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
        key = _md_ensure_key(md, entry);
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
    ASSERT_CONSISTENT(md, false);

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
        if (_str_cmp(identity, entry->identity)) {
            ASSERT_CONSISTENT(md, false);
            *result = Py_NewRef(entry->value);
            return 1;
        }
    }

    if (_md_add_with_hash(md, hash, identity, key, value) < 0) {
        return -1;
    }

    ASSERT_CONSISTENT(md, false);
    *result = Py_NewRef(value);
    return 0;
}

static inline int
md_set_default(MultiDictObject* md, PyObject* key, PyObject* value,
               PyObject** result)
{
    *result = NULL;
    PyObject* identity;
    Py_hash_t hash;
    if (md_calc_identity_hash(md, key, &identity, &hash) < 0) {
        return -1;
    }
    int ret;
    Py_BEGIN_CRITICAL_SECTION(md);
    ret = _md_set_default_locked(md, identity, hash, key, value, result);
    Py_END_CRITICAL_SECTION();
    Py_DECREF(identity);
    return ret;
}

// Caller holds md's critical section
static inline int
_md_pop_one_locked(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                   PyObject** ret)
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
        if (_str_cmp(identity, entry->identity)) {
            PyObject* value = Py_NewRef(entry->value);
            _md_del_at(md, iter.slot, entry);
            *ret = value;
            store_version(md, next_version(md->state));
            ASSERT_CONSISTENT(md, false);
            return 1;
        }
    }
    ASSERT_CONSISTENT(md, false);
    return 0;
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
    Py_BEGIN_CRITICAL_SECTION(md);
    result = _md_pop_one_locked(md, identity, hash, ret);
    Py_END_CRITICAL_SECTION();
    Py_DECREF(identity);
    return result;
}

static int
_md_getall_visit(void* user_data, PyObject* key, PyObject* value)
{
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

// Caller holds md's critical section
static inline int
_md_pop_all_locked(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                   reflist_t* values)
{
    if (md_len(md) == 0) {
        return 0;
    }

restart:;
    htkeys_t* keys = md->keys;
    htkeysiter_t iter;
    htkeysiter_init(&iter, keys, hash);
    entry_t* entries = htkeys_entries(keys);

    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t* entry = entries + iter.index;

        if (hash != entry->hash) {
            continue;
        }
        if (_str_cmp(identity, entry->identity)) {
            if (reflist_push(values, Py_NewRef(entry->value)) < 0) {
                return -1;
            }
            uint64_t version = next_version(md->state);
            store_version(md, version);
            _md_del_at(md, iter.slot, entry);
            // the decref can run a __del__ that lets another thread resize
            // and bump the version through the atomic store above, so this
            // side of the comparison must be an atomic load too
            if (UNLIKELY(md->keys != keys || load_version(md) != version)) {
                goto restart;
            }
        }
    }

    ASSERT_CONSISTENT(md, false);
    return 0;
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
    int tmp;
    Py_BEGIN_CRITICAL_SECTION(md);
    tmp = _md_pop_all_locked(md, identity, hash, &values);
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

    PyObject* key = _md_calc_key(md, entry->key, entry->identity);
    if (key == NULL) {
        return NULL;
    }
    PyObject* ret = PyTuple_Pack(2, key, entry->value);
    Py_CLEAR(key);
    if (ret == NULL) {
        return NULL;
    }

    htkeysiter_t iter;
    htkeysiter_init(&iter, md->keys, entry->hash);

    for (; iter.index != pos; htkeysiter_next(&iter)) {
    }
    _md_del_at(md, iter.slot, entry);
    store_version(md, next_version(md->state));
    ASSERT_CONSISTENT(md, false);
    return ret;
}

static inline int
_md_replace(MultiDictObject* md, PyObject* key, PyObject* value,
            PyObject* identity, Py_hash_t hash, reflist_t* defer)
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
            if (entry->hash != hash || !_str_cmp(identity, entry->identity)) {
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
            return _md_add_with_hash(md, hash, identity, key, value);
        }
        store_version(md, next_version(md->state));
        return 0;
    }
}

static inline int
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
    Py_BEGIN_CRITICAL_SECTION(md);
    ret = _md_replace(md, key, value, identity, hash, &defer);
    ASSERT_CONSISTENT(md, false);
    Py_END_CRITICAL_SECTION();
    reflist_clear(&defer);
    Py_DECREF(identity);
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

        if (!_str_cmp(entry1->identity, entry2->identity)) {
            return 0;
        }

        int cmp =
            PyObject_RichCompareBool(entry1->value, entry2->value, Py_EQ);
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

static inline PyObject*
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
md_traverse(MultiDictObject* md, visitproc visit, void* arg)
{
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
        return 0;
    }
    store_version(md, next_version(md->state));

    // Publish the empty table before releasing any entry's reference: a
    // decref below may run arbitrary Python code (a __del__), which can
    // suspend this critical section. If md->keys still pointed at the old
    // table while that happens, a concurrent, correctly-locked reader
    // could observe entries mid-clear (identity already NULL, key/value
    // not yet). Swapping first means a suspended thread only ever sees
    // either the fully-populated old table or the fully-empty one.
    htkeys_t* old_keys = md->keys;
    store_used(md, 0);
    store_keys(md, (htkeys_t*)&empty_htkeys);

#ifdef Py_GIL_DISABLED
    _md_retire(md, old_keys);
#else
    entry_t* entries = htkeys_entries(old_keys);
    Py_ssize_t nentries = old_keys->nentries;
    for (Py_ssize_t pos = 0; pos < nentries; pos++) {
        entry_t* entry = entries + pos;
        if (entry->identity != NULL) {
            /* Py_CLEAR rather than freethreading.h's reset_identity() and
               reset_value(): it skips the store when a field is already
               NULL, which they cannot express, and this arm is GIL-only,
               so there is no ordering left for an accessor to carry. */
            Py_CLEAR(entry->identity);
            Py_CLEAR(entry->key);
            Py_CLEAR(entry->value);
        }
    }
    htkeys_free(MD_POOLS(md), old_keys);
#endif
    ASSERT_CONSISTENT(md, false);
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif
