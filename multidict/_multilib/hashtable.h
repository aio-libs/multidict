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
#include "str_cmp.h"
#include "update_marks.h"
#include "walk.h"
#include "watch.h"

#define MD_POOLS(md) ((md)->state->htkeys_pools)

/* Whether a compact table of md can hold `key`: a MultiDict's key must be
   its own identity, a CIMultiDict's an exact istr, whose identity is then
   its canonical form. */
static inline bool
md_key_fits(const MultiDictObject* md, PyObject* key, PyObject* identity)
{
    if (md->is_ci) {
        assert(!IStr_CheckExact(md->state, key) ||
               istr_canonical(key) == identity);
        return IStr_CheckExact(md->state, key);
    }
    return key == identity;
}

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
calculation could return the already visited index before reaching the end.
Equal keys are reached first in insertion order, though, so the walk skips any
entry index not above the last match (see md_walk_with_hash() in walk.h); the
table itself is never marked. Double iteration over the indices still has O(1)
amortized time, it is ok.

`.add()`, `val = md[key]`, `md[key] = val`, `md.setdefault()` all have O(1).
`.getall()` / `.popall()` have O(N) where N is the amount of returned items.
`.update()` / `extend()` have O(N+M) where N and M are amount of items
in the left and right arguments.

`.copy()` and constuction from multidict is super fast.
*/

/* GROWTH_RATE(). Growth rate upon hitting maximum load.
 * Currently set to used*3.
 * This means that dicts double in size when growing without deletions,
 * but have more head room when the number of deletions is on a par with the
 * number of insertions.  See also bpo-17563 and bpo-33205.
 *
 * GROWTH_RATE was set to used*4 up to version 3.2.
 * GROWTH_RATE was set to used*2 in version 3.3.0
 * GROWTH_RATE was set to used*2 + capacity/2 in 3.4.0-3.6.0.
 */
#define GROWTH_RATE(md) ((md)->used * 3)

/* Frees a table no longer published, releasing the references its
   entries still hold: only md_clear()'s and _md_install_keys()'s
   tables have any, since _md_rebuild() moves its old table's entries
   and zeroes nentries. */
static void
_htkeys_dispose(pool_t* pools, htkeys_t* keys)
{
    /* Nothing can reach the table any more, so a finalizer run by a decref
       cannot see the stale pointers: no need to clear them, nor to reload
       nentries after each call. */
    Py_ssize_t nentries = keys->nentries;
    if (kind_is_compact(keys->kind)) {
        entry_t* entry = HTKEYS_COMPACT_ENTRIES(keys);
        for (entry_t* end = entry + nentries; entry < end; entry++) {
            Py_XDECREF(entry->key);  // also owns the identity
            Py_XDECREF(entry->value);
        }
    } else {
        anystr_entry_t* entry = HTKEYS_ANYSTR_ENTRIES(keys);
        for (anystr_entry_t* end = entry + nentries; entry < end; entry++) {
            Py_XDECREF(entry->identity);
            Py_XDECREF(entry->base.key);
            Py_XDECREF(entry->base.value);
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

/* Out of line: inlined, it costs FT lookups up to 10 Ir. */
static void
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

static void
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

static void
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
static void
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

static Py_hash_t
_compact_entry_hash(const entry_t* entry, bool ci)
{
    return ci ? compact_key_hash_ci(entry->key)
              : compact_key_hash_cs(entry->key);
}

/* _md_has_equal_hashes() for a KIND_COMPACT table; `ci` is a constant at
   both call sites. */
static inline bool
_md_has_equal_hashes_compact(htkeys_t* keys, bool ci)
{
    entry_t* entries = HTKEYS_COMPACT_ENTRIES(keys);
    for (Py_ssize_t i = 0; i < keys->nentries; i++) {
        if (entry_is_hole(entries + i)) {
            continue;
        }
        Py_hash_t hash = _compact_entry_hash(entries + i, ci);
        htkeysiter_t iter;
        HTKEYSITER_INIT(&iter, keys, hash);
        for (; iter.index != i; HTKEYSITER_NEXT(&iter)) {
            assert(iter.index != DKIX_EMPTY);
            if (iter.index >= 0 &&
                _compact_entry_hash(entries + iter.index, ci) == hash) {
                return true;
            }
        }
    }
    return false;
}

/* The same for a KIND_ANYSTR table. */
static bool
_md_has_equal_hashes_anystr(htkeys_t* keys)
{
    anystr_entry_t* entries = HTKEYS_ANYSTR_ENTRIES(keys);
    for (Py_ssize_t i = 0; i < keys->nentries; i++) {
        if (entry_is_hole((const entry_t*)(entries + i))) {
            continue;
        }
        Py_hash_t hash = entries[i].hash;
        htkeysiter_t iter;
        HTKEYSITER_INIT(&iter, keys, hash);
        for (; iter.index != i; HTKEYSITER_NEXT(&iter)) {
            assert(iter.index != DKIX_EMPTY);
            if (iter.index >= 0 && entries[iter.index].hash == hash) {
                return true;
            }
        }
    }
    return false;
}

/* Whether a live entry has another of the same hash before it on its
   probe chain, which is where an earlier entry of its key would sit. */
COLD static bool
_md_has_equal_hashes(htkeys_t* keys, bool ci)
{
    if (!kind_is_compact(keys->kind)) {
        return _md_has_equal_hashes_anystr(keys);
    }
    if (ci) {
        return _md_has_equal_hashes_compact(keys, true);
    }
    return _md_has_equal_hashes_compact(keys, false);
}

/* Publishes a rebuilt table: the old one's entries have all moved over.
   A set maybe_dups is checked again, since deletions may have left it
   stale. */
static inline void
_md_publish_rebuilt(MultiDictObject* md, htkeys_t* oldkeys, htkeys_t* newkeys)
{
    if (oldkeys->maybe_dups) {
        newkeys->maybe_dups = _md_has_equal_hashes(newkeys, md->is_ci);
    }
    store_keys(md, newkeys);
    update_marks_moved(md);

    /* Bump the version on every resize, not just when a caller's
       own insert/delete/replace would bump it anyway: a freed
       htkeys_t can get reallocated at the very same address by a
       later resize (same size class, common in practice), so code
       elsewhere that detects "did md->keys change under me" by
       comparing the raw pointer alone (see _md_replace_locked()'s
       comment) needs a companion signal that can't coincidentally
       repeat. It also lets md_check_version() ignore md->keys. */
    bump_version(md);

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
    if (UNLIKELY(!htkeys_size_fits(log2_newsize))) {
        PyErr_NoMemory();
        return -1;
    }
    // the room the caller asked for, on top of the holes
    Py_ssize_t want =
        USABLE_FRACTION((Py_ssize_t)1 << log2_newsize) - md->used + nentries;
    while (USABLE_FRACTION((Py_ssize_t)1 << log2_newsize) < want) {
        log2_newsize++;
        if (UNLIKELY(!htkeys_size_fits(log2_newsize))) {
            PyErr_NoMemory();
            return -1;
        }
    }

    htkeys_t* newkeys = htkeys_new_unfilled(
        MD_POOLS(md),
        log2_newsize,
        oldkeys->nentries == 0 ? KIND_COMPACT : oldkeys->kind);
    if (UNLIKELY(newkeys == NULL)) {
        return -1;
    }
    htkeys_entries_copy(newkeys, oldkeys, nentries);
    htkeys_zero_entries(newkeys, nentries);
    if (kind_is_compact(newkeys->kind)) {
        htkeys_build_compact_indices(newkeys, md->is_ci, nentries, true);
    } else {
        htkeys_build_anystr_indices(newkeys, nentries, true);
    }
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
    if (UNLIKELY(!htkeys_size_fits(log2_size))) {
        PyErr_NoMemory();
        return NULL;
    }
    htkeys_t* keys = htkeys_new(MD_POOLS(md), log2_size, kind);
    if (UNLIKELY(keys == NULL)) {
        return NULL;
    }
    keys->usable -= nholes;
    keys->nentries = nholes;
    return keys;
}

/* Copies the live entries of `src` to the front of `dst`, a table of its
   kind, returning how many. One copy per kind, so each steps by a
   constant entry size. */
static Py_ssize_t
_md_copy_live(htkeys_t* dst, const htkeys_t* src)
{
    Py_ssize_t n = src->nentries;
    if (kind_is_compact(src->kind)) {
        entry_t* first = HTKEYS_COMPACT_ENTRIES(dst);
        entry_t* out = first;
        for (entry_t *ep = HTKEYS_COMPACT_ENTRIES(src), *end = ep + n;
             ep < end;
             ep++) {
            if (!entry_is_hole(ep)) {
                *out++ = *ep;
            }
        }
        return out - first;
    }
    anystr_entry_t* first = HTKEYS_ANYSTR_ENTRIES(dst);
    anystr_entry_t* out = first;
    for (anystr_entry_t *ep = HTKEYS_ANYSTR_ENTRIES(src), *end = ep + n;
         ep < end;
         ep++) {
        if (!entry_is_hole((const entry_t*)ep)) {
            *out++ = *ep;
        }
    }
    return out - first;
}

/* Publishes newkeys, a KIND_COMPACT table whose first `filled` entries
   hold all of md's and nothing else. */
static void
_md_publish_compacted_compact(MultiDictObject* md, htkeys_t* oldkeys,
                              htkeys_t* newkeys, Py_ssize_t filled)
{
    /* What the copy actually wrote, rather than md->used: the two agree,
       but taking the count from the copy means a table can never be
       published over entries nothing has written. */
    assert(filled == md->used);
    assert(kind_is_compact(newkeys->kind));
    htkeys_zero_entries(newkeys, filled);
    htkeys_build_compact_indices(newkeys, md->is_ci, filled, false);
    newkeys->usable -= filled;
    newkeys->nentries = filled;
    _md_publish_rebuilt(md, oldkeys, newkeys);
}

/* The same for a KIND_ANYSTR table. */
static void
_md_publish_compacted_anystr(MultiDictObject* md, htkeys_t* oldkeys,
                             htkeys_t* newkeys, Py_ssize_t filled)
{
    assert(filled == md->used);
    assert(!kind_is_compact(newkeys->kind));
    htkeys_zero_entries(newkeys, filled);
    htkeys_build_anystr_indices(newkeys, filled, false);
    newkeys->usable -= filled;
    newkeys->nentries = filled;
    _md_publish_rebuilt(md, oldkeys, newkeys);
}

static int
_md_rebuild(MultiDictObject* md, uint8_t log2_newsize)
{
    // without holes, compacting keeps every index anyway
    if (UNLIKELY(md->batches != 0) && md->keys->nentries != md->used) {
        return _md_rebuild_keeping_indices(md, log2_newsize);
    }
    if (UNLIKELY(!htkeys_size_fits(log2_newsize))) {
        PyErr_NoMemory();
        return -1;
    }
    assert(log2_newsize >= HT_LOG_MINSIZE);

    /* A table starts compact and moves to KIND_ANYSTR on the first key
       that does not fit (see md_key_fits()); one with no entries yet
       starts afresh, so a clear() lets md return to the compact kind. */
    uint8_t kind = md->keys->nentries == 0 ? KIND_COMPACT : md->keys->kind;
    /* The copy below writes the front of the entries array, so only
       what it leaves over has to be zeroed. */
    htkeys_t* newkeys = htkeys_new_unfilled(MD_POOLS(md), log2_newsize, kind);
    if (UNLIKELY(newkeys == NULL)) {
        return -1;
    }

    htkeys_t* oldkeys = md->keys;
    Py_ssize_t numentries = md->used;
    Py_ssize_t filled;
    if (oldkeys->nentries == numentries) {
        htkeys_entries_copy(newkeys, oldkeys, numentries);
        filled = numentries;
    } else {
        filled = _md_copy_live(newkeys, oldkeys);
    }
    if (kind_is_compact(kind)) {
        _md_publish_compacted_compact(md, oldkeys, newkeys, filled);
    } else {
        _md_publish_compacted_anystr(md, oldkeys, newkeys, filled);
    }
    return 0;
}

static int
md_reserve(MultiDictObject* md, Py_ssize_t extra_size)
{
    if (md->keys->usable >= extra_size) {
        return 0;
    }
    /* Sized by live entries, so a table short of room only because of
       deleted ones is compacted, or even shrunk, rather than grown. */
    return _md_rebuild(md, estimate_log2_keysize(extra_size + md->used));
}

/* _md_rebuild_to_anystr()'s loop, `sfx` being cs or ci as for
   compact_key_identity_cs(). */
#define _MD_REBUILD_TO_ANYSTR_LOOP(sfx)                             \
    for (Py_ssize_t i = 0; i < oldkeys->nentries; i++) {            \
        entry_t* src = oldentries + i;                              \
        PyObject* key = src->key;                                   \
        if (key == NULL) {                                          \
            continue;                                               \
        }                                                           \
        anystr_entry_t* dst = newentries + filled++;                \
        dst->identity = Py_NewRef(compact_key_identity_##sfx(key)); \
        dst->hash = compact_key_hash_##sfx(key);                    \
        dst->base.key = key;                                        \
        dst->base.value = src->value;                               \
    }

/* md_reserve() into a KIND_ANYSTR table: grows and moves a compact table
   in one rebuild, where md_reserve() and then md_to_anystr() would copy
   it twice. Holes are dropped, so with a batch in flight there must be
   none. */
static int
_md_rebuild_to_anystr(MultiDictObject* md, uint8_t log2_newsize)
{
    htkeys_t* oldkeys = md->keys;
    assert(md->batches == 0 || oldkeys->nentries == md->used);
    assert(kind_is_compact(oldkeys->kind) || oldkeys->nentries == 0);
    if (UNLIKELY(!htkeys_size_fits(log2_newsize))) {
        PyErr_NoMemory();
        return -1;
    }
    htkeys_t* newkeys =
        htkeys_new_unfilled(MD_POOLS(md), log2_newsize, KIND_ANYSTR);
    if (UNLIKELY(newkeys == NULL)) {
        return -1;
    }
    entry_t* oldentries = HTKEYS_COMPACT_ENTRIES(oldkeys);
    anystr_entry_t* newentries = HTKEYS_ANYSTR_ENTRIES(newkeys);
    Py_ssize_t filled = 0;
    if (md->is_ci) {
        _MD_REBUILD_TO_ANYSTR_LOOP(ci);
    } else {
        _MD_REBUILD_TO_ANYSTR_LOOP(cs);
    }
    _md_publish_compacted_anystr(md, oldkeys, newkeys, filled);
    return 0;
}

#undef _MD_REBUILD_TO_ANYSTR_LOOP

/* _md_rebuild() for an add of a key that fits a compact table or not:
   one that does not moves md to KIND_ANYSTR in the same rebuild. A table
   with no entries counts as compact, since _md_rebuild() would restart
   it so. */
static int
_md_grow_for_add(MultiDictObject* md, uint8_t log2_newsize, bool fits)
{
    htkeys_t* keys = md->keys;
    if (!fits && (keys->nentries == 0 || kind_is_compact(keys->kind)) &&
        (md->batches == 0 || keys->nentries == md->used)) {
        return _md_rebuild_to_anystr(md, log2_newsize);
    }
    return _md_rebuild(md, log2_newsize);
}

/* md_reserve() for extend(), update() or merge(), before their batch
   starts. Keyword names are plain str, which never fit a CIMultiDict's
   compact table, so with any the table is moved while it grows; moved
   later, it cost update(istr_items, **kwargs) a second copy, 18%.

   update() and merge() often find their keys there already, so, like
   dict.update(), they grow only a table that could not hold `extra_size`
   entries even empty. Any other growth waits for the batch's first add,
   which then makes room for them all, unless deletions left holes: those
   the batch would have to keep. */
static int
md_reserve_batch(MultiDictObject* md, Py_ssize_t extra_size, bool kwargs,
                 bool overlap)
{
    if (UNLIKELY(kwargs) && md->is_ci && kind_is_compact(md->keys->kind) &&
        md->used > 0 && md->batches == 0) {
        return _md_rebuild_to_anystr(
            md, estimate_log2_keysize(extra_size + md->used));
    }
    if (overlap && md->keys->nentries == md->used &&
        md->keys->nentries + md->keys->usable >= extra_size) {
        return 0;
    }
    return md_reserve(md, extra_size);
}

/* Publishes md's replacement table, then drops the one it replaced,
   which a fresh shell does not have. The drop comes last because its
   decrefs can run a __del__ that reads or mutates md: publishing first
   means that code finds md already holding its new contents, and what
   it adds stays in md rather than being overwritten. */
static void
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
    MD_WATCH_RECORD_SIMPLE(md, event);
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
        if (UNLIKELY(new_keys == NULL)) {
            return -1;
        }
    } else if (minused > USABLE_FRACTION(HT_MINSIZE)) {
        uint8_t log2_newsize;
        /* There are no strict guarantee that returned dict can contain minused
         * items without resize.  So we create medium size dict instead of very
         * large dict or MemoryError.
         */
        if (minused > USABLE_FRACTION((Py_ssize_t)1 << HT_LOG_MAX_PRESIZE)) {
            log2_newsize = HT_LOG_MAX_PRESIZE;
        } else {
            log2_newsize = estimate_log2_keysize(minused);
        }

        new_keys = htkeys_new(MD_POOLS(md), log2_newsize, kind);
        if (UNLIKELY(new_keys == NULL)) return -1;
    } else if (minused > 0 && kind != KIND_COMPACT) {
        /* The first insert would allocate this table anyway, but compact,
           only to move it at once. */
        new_keys = htkeys_new(MD_POOLS(md), HT_LOG_MINSIZE, kind);
        if (UNLIKELY(new_keys == NULL)) return -1;
    }

    _md_install_keys(md, new_keys, 0, is_ci, MultiDict_EVENT_CLEARED);
    return 0;
}

/* The kind a CIMultiDict's table starts with when `key` (borrowed, or NULL
   if unknown) is the first key it will get: pre-sizing a compact table
   for a str key would only have it rebuilt at once. */
static uint8_t
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
    bool other_ci = other->is_ci;
    // the holes keep md's entries' indices; other's entries may need more
    htkeys_t* keys = _md_new_keys_after_holes(md, other->used, KIND_ANYSTR);
    if (UNLIKELY(keys == NULL)) {
        return -1;
    }
    anystr_entry_t* dst = HTKEYS_ANYSTR_ENTRIES(keys) + keys->nentries;
    htkeys_t* src_keys = other->keys;
    if (kind_is_compact(src_keys->kind) && other_ci) {
        entry_t* src = HTKEYS_COMPACT_ENTRIES(src_keys);
        for (entry_t* end = src + src_keys->nentries; src < end; src++) {
            if (!entry_is_hole(src)) {
                dst->identity = Py_NewRef(compact_key_identity_ci(src->key));
                dst->base.key = Py_NewRef(src->key);
                dst->base.value = Py_NewRef(src->value);
                dst->hash = compact_key_hash_ci(src->key);
                dst++;
            }
        }
    } else if (kind_is_compact(src_keys->kind)) {
        entry_t* src = HTKEYS_COMPACT_ENTRIES(src_keys);
        for (entry_t* end = src + src_keys->nentries; src < end; src++) {
            if (!entry_is_hole(src)) {
                dst->identity = Py_NewRef(compact_key_identity_cs(src->key));
                dst->base.key = Py_NewRef(src->key);
                dst->base.value = Py_NewRef(src->value);
                dst->hash = compact_key_hash_cs(src->key);
                dst++;
            }
        }
    } else {
        anystr_entry_t* src = HTKEYS_ANYSTR_ENTRIES(src_keys);
        for (anystr_entry_t* end = src + src_keys->nentries; src < end;
             src++) {
            if (!entry_is_hole((const entry_t*)src)) {
                dst->identity = Py_NewRef(src->identity);
                dst->base.key = Py_NewRef(src->base.key);
                dst->base.value = Py_NewRef(src->base.value);
                dst->hash = src->hash;
                dst++;
            }
        }
    }
    Py_ssize_t nentries = dst - HTKEYS_ANYSTR_ENTRIES(keys);
    keys->usable -= nentries - keys->nentries;
    keys->nentries = nentries;
    htkeys_build_anystr_indices(keys, nentries, true);
    keys->maybe_dups = src_keys->maybe_dups;
    _md_install_keys(md, keys, other->used, other_ci, MultiDict_EVENT_CLONED);
    return 0;
}

static int
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
        if (UNLIKELY(keys == NULL)) {
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
        if (kind_is_compact(keys->kind)) {
            entry_t* entry = HTKEYS_COMPACT_ENTRIES(keys);
            for (entry_t* end = entry + keys->nentries; entry < end; entry++) {
                Py_XINCREF(entry->key);  // also owns the identity
                Py_XINCREF(entry->value);
            }
        } else {
            anystr_entry_t* entry = HTKEYS_ANYSTR_ENTRIES(keys);
            for (anystr_entry_t* end = entry + keys->nentries; entry < end;
                 entry++) {
                Py_XINCREF(entry->identity);
                Py_XINCREF(entry->base.key);
                Py_XINCREF(entry->base.value);
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

static Py_ssize_t
md_len(MultiDictObject* md)
{
    return load_used(md);
}

/* md_is_first_key()'s chain walk, once the first slot did not match. */
COLD static bool
_md_is_first_key_probe(htkeys_t* keys, bool ci, Py_hash_t hash,
                       PyObject* identity, Py_ssize_t index, Py_ssize_t first)
{
    bool compact = kind_is_compact(keys->kind);
    htkeysiter_t iter = {.keys = keys,
                         .mask = (size_t)_htkeys_mask(keys),
                         .perturb = (size_t)hash,
                         .index = first};
    iter.slot = (size_t)hash & iter.mask;
    for (; iter.index != index; HTKEYSITER_NEXT(&iter)) {
        assert(iter.index != DKIX_EMPTY);
        if (iter.index < 0) {
            continue;
        }
        Py_hash_t other_hash;
        PyObject* other;
        if (compact) {
            entry_t* e = HTKEYS_COMPACT_ENTRIES(keys) + iter.index;
            other_hash = (ci ? compact_key_hash_ci(e->key)
                             : compact_key_hash_cs(e->key));
            other = (ci ? compact_key_identity_ci((e)->key)
                        : compact_key_identity_cs(e->key));
        } else {
            anystr_entry_t* e = HTKEYS_ANYSTR_ENTRIES(keys) + iter.index;
            other_hash = e->hash;
            other = e->identity;
        }
        if (other_hash == hash &&
            (other == identity || str_cmp(identity, other))) {
            return false;
        }
    }
    return true;
}

/* Inserts take only empty slots and rebuilds reinsert in index order, so
   the first entry of an identity on its probe chain is its lowest index,
   the one getone() returns.  Keys and iteration yield only that entry;
   callers skip the test while the table's maybe_dups is clear. An entry
   usually sits in its first slot. */
static inline bool
md_is_first_key(htkeys_t* keys, bool ci, Py_hash_t hash, PyObject* identity,
                Py_ssize_t index)
{
    size_t slot = (size_t)hash & (size_t)_htkeys_mask(keys);
    Py_ssize_t first =
        HTKEYS_GET_INDEX(keys, keys->log2_size, (Py_ssize_t)slot);
    if (first == index) {
        return true;
    }
    return _md_is_first_key_probe(keys, ci, hash, identity, index, first);
}

// The number of distinct keys, len(md.keys()).
static Py_ssize_t
md_keys_len(MultiDictObject* md)
{
    Py_ssize_t ret = 0;
    Py_BEGIN_CRITICAL_SECTION(md);
    htkeys_t* keys = md->keys;
    bool ci = md->is_ci;
    if (!keys->maybe_dups) {
        ret = md_len(md);
    } else if (kind_is_compact(keys->kind)) {
        entry_t* entries = HTKEYS_COMPACT_ENTRIES(keys);
        for (Py_ssize_t i = 0; i < keys->nentries; i++) {
            entry_t* e = entries + i;
            if (!entry_is_hole(e) &&
                md_is_first_key(keys,
                                ci,
                                (ci ? compact_key_hash_ci(e->key)
                                    : compact_key_hash_cs(e->key)),
                                (ci ? compact_key_identity_ci((e)->key)
                                    : compact_key_identity_cs(e->key)),
                                i)) {
                ++ret;
            }
        }
    } else {
        anystr_entry_t* entries = HTKEYS_ANYSTR_ENTRIES(keys);
        for (Py_ssize_t i = 0; i < keys->nentries; i++) {
            anystr_entry_t* e = entries + i;
            if (!entry_is_hole((const entry_t*)e) &&
                md_is_first_key(keys, ci, e->hash, e->identity, i)) {
                ++ret;
            }
        }
    }
    Py_END_CRITICAL_SECTION();
    return ret;
}

/* md_to_anystr()'s loop, `sfx` being cs or ci as for
   compact_key_identity_cs(). */
#define _MD_TO_ANYSTR_LOOP(sfx)                                         \
    for (Py_ssize_t i = 0; i < oldkeys->nentries; i++) {                \
        entry_t* src = oldentries + i;                                  \
        anystr_entry_t* dst = newentries + i;                           \
        PyObject* key = src->key;                                       \
        dst->base.key = key;                                            \
        dst->base.value = src->value;                                   \
        if (key == NULL) {                                              \
            dst->identity = NULL;                                       \
            dst->hash = 0;                                              \
        } else {                                                        \
            dst->identity = Py_NewRef(compact_key_identity_##sfx(key)); \
            dst->hash = compact_key_hash_##sfx(key);                    \
        }                                                               \
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
    if (UNLIKELY(newkeys == NULL)) {
        return -1;
    }
    memcpy(newkeys->indices,
           oldkeys->indices,
           (size_t)1 << oldkeys->log2_index_bytes);
    entry_t* oldentries = HTKEYS_COMPACT_ENTRIES(oldkeys);
    anystr_entry_t* newentries = HTKEYS_ANYSTR_ENTRIES(newkeys);
    if (md->is_ci) {
        _MD_TO_ANYSTR_LOOP(ci);
    } else {
        _MD_TO_ANYSTR_LOOP(cs);
    }
    htkeys_zero_entries(newkeys, oldkeys->nentries);
    newkeys->nentries = oldkeys->nentries;
    newkeys->usable = oldkeys->usable;
    /* Kept, not checked again: an insert can run this after noting that
       its own, not yet stored, entry repeats a key. */
    uint8_t maybe_dups = oldkeys->maybe_dups;
    _md_publish_rebuilt(md, oldkeys, newkeys);
    newkeys->maybe_dups = maybe_dups;
    return 0;
}

#undef _MD_TO_ANYSTR_LOOP

/* What an insert knows of its key, as the `slot` it passes: the key may
   already be there, or the caller knows it is new. */
#define MD_SLOT_CHECK ((Py_ssize_t) - 1)
#define MD_SLOT_FIND ((Py_ssize_t) - 2)

/* Whether md's table must change before an add: it is full, or it is
   compact and the key does not fit. */
static inline bool
_md_add_needs_new_table(MultiDictObject* md, Py_ssize_t room, bool fits)
{
    htkeys_t* keys = md->keys;
    return keys->usable <= room || keys == &empty_htkeys ||
           (UNLIKELY(!fits) && kind_is_compact(keys->kind));
}

/* Makes room in md's table for one more entry, of a key that fits a
   compact table or not, in one rebuild where both are due. */
COLD static int
_md_prepare_add(MultiDictObject* md, bool fits)
{
    htkeys_t* keys = md->keys;
    if (keys->usable <= 0 || keys == &empty_htkeys) {
        uint8_t log2_newsize = calculate_log2_keysize(GROWTH_RATE(md));
        if (UNLIKELY(_md_grow_for_add(md, log2_newsize, fits) < 0)) {
            return -1;
        }
    }
    if (!fits && kind_is_compact(md->keys->kind)) {
        return md_to_anystr(md);
    }
    return 0;
}

/* Stores a new entry at the end of md's table, which _md_prepare_add()
   has readied for it, past every live one. Until HTKEYS_PUBLISH_INDEX()
   no lock-free reader can reach the entry, so plain stores are enough;
   mark_shared() still lets a reader take a reference once it does. The
   entry is carved out of the zeroed tail, so nothing here has an old
   reference to drop. A compact entry drops the identity it was given: the
   key's reference keeps it alive, being either the key itself or the
   istr's canonical form. Returns the table. */
static inline htkeys_t*
_md_store_new_entry(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
                    PyObject* key, PyObject* value, Py_ssize_t slot)
{
    htkeys_t* keys = md->keys;
    Py_ssize_t hashpos =
        slot == MD_SLOT_CHECK
            ? htkeys_find_empty_slot_noting_dups(keys, hash, md->is_ci)
            : htkeys_find_empty_slot(keys, hash);
    if (kind_is_compact(keys->kind)) {
        assert(md_key_fits(md, key, identity));
        entry_t* entry = HTKEYS_COMPACT_ENTRIES(keys) + keys->nentries;
        assert(entry->key == NULL && entry->value == NULL);
        mark_shared(key);
        mark_shared(value);
        entry->key = key;
        entry->value = value;
        Py_DECREF(identity);
    } else {
        anystr_entry_t* entry = HTKEYS_ANYSTR_ENTRIES(keys) + keys->nentries;
        assert(entry->identity == NULL && entry->base.key == NULL &&
               entry->base.value == NULL);
        mark_shared(identity);
        mark_shared(value);
        entry->base.key = key;
        entry->base.value = value;
        entry->hash = hash;
        entry->identity = identity;
    }
    HTKEYS_PUBLISH_INDEX(keys, keys->log2_size, hashpos, keys->nentries);
    return keys;
}

/* Stores the new entry once md's table has room for it: all an add does
   after its last chance to fail. */
static inline void
_md_add_stored(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
               PyObject* key, PyObject* value, Py_ssize_t slot)
{
    htkeys_t* keys = _md_store_new_entry(md, hash, identity, key, value, slot);

    bump_version(md);
    add_used(md, 1);
    keys->usable -= 1;
    keys->nentries += 1;
    MD_WATCH_RECORD(
        md, MultiDict_EVENT_ADDED, identity, hash, key, value, NULL);
}

static int
md_add_with_hash_steal_refs(MultiDictObject* md, Py_hash_t hash,
                            PyObject* identity, PyObject* key, PyObject* value,
                            bool fits, Py_ssize_t slot)
{
    assert(fits == md_key_fits(md, key, identity));
    if (_md_add_needs_new_table(md, 0, fits) &&
        UNLIKELY(_md_prepare_add(md, fits) < 0)) {
        return -1;
    }
    _md_add_stored(md, hash, identity, key, value, slot);
    return 0;
}

static int
md_add_with_hash(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
                 PyObject* key, PyObject* value, bool fits, Py_ssize_t slot)
{
    assert(fits == md_key_fits(md, key, identity));
    if (_md_add_needs_new_table(md, 0, fits) &&
        UNLIKELY(_md_prepare_add(md, fits) < 0)) {
        return -1;
    }
    _md_add_stored(
        md, hash, Py_NewRef(identity), Py_NewRef(key), Py_NewRef(value), slot);
    return 0;
}

/* _md_prepare_add() for a batch's add: on the first add that is short of
   the room update_marks_init() asked for, all of it. */
static int
_md_prepare_add_for_upd(MultiDictObject* md, update_marks_t* marks, bool fits)
{
    Py_ssize_t extra = marks->reserve;
    marks->reserve = 0;
    if (extra > 0 && md->keys->usable < extra &&
        UNLIKELY(_md_grow_for_add(
                     md, estimate_log2_keysize(extra + md->used), fits) < 0)) {
        return -1;
    }
    if (UNLIKELY(_md_prepare_add(md, fits) < 0)) {
        return -1;
    }
    return update_marks_sync(marks, md);
}

static int
md_add_for_upd(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
               PyObject* key, PyObject* value, update_marks_t* marks,
               bool fits, Py_ssize_t slot)
{
    assert(fits == md_key_fits(md, key, identity));
    Py_INCREF(identity);
    Py_INCREF(key);
    Py_INCREF(value);
    // a reserve still pending is no more than the room left
    if (_md_add_needs_new_table(md, marks->reserve, fits) &&
        UNLIKELY(_md_prepare_add_for_upd(md, marks, fits) < 0)) {
        goto fail;
    }
    if (UNLIKELY(bitmap_set(&marks->updated, md->keys->nentries) < 0)) {
        goto fail;
    }
    htkeys_t* keys = _md_store_new_entry(md, hash, identity, key, value, slot);

    bump_version(md);
    add_used(md, 1);
    keys->usable -= 1;
    keys->nentries += 1;
    MD_WATCH_RECORD(
        md, MultiDict_EVENT_ADDED, identity, hash, key, value, NULL);
    return 0;
fail:
    /* Not deferred: the caller still holds its own references, so
       none of these can drop to zero and run __del__. */
    Py_DECREF(identity);
    Py_DECREF(key);
    Py_DECREF(value);
    return -1;
}

/* A borrowed identity is the key itself, which fits a compact table only
   in a MultiDict, or an istr's canonical, which always does. A key whose
   identity has to be computed fits none. */
static int
md_add(MultiDictObject* md, PyObject* key, PyObject* value)
{
    PyObject* identity = md_identity_or_new(md, key);
    if (UNLIKELY(identity == NULL)) {
        return -1;
    }
    int ret = -1;
    Py_hash_t hash = unicode_hash(identity);
    if (hash != -1) {
        bool fits = md_key_fits(md, key, identity);
        bool flush;
        Py_BEGIN_CRITICAL_SECTION(md);
        ret = md_add_with_hash(
            md, hash, identity, key, value, fits, MD_SLOT_CHECK);
        ASSERT_CONSISTENT(md);
        flush = MD_WATCH_PENDING(md);
        Py_END_CRITICAL_SECTION();
        MD_WATCH_FLUSH_IF(md, flush);
    }
    md_identity_release(md, key, identity);
    return ret;
}

/* Removes entry, whose index is at `slot`, and hands its references to the
   caller, for a KIND_COMPACT table. A compact entry has no identity
   reference of its own, so *pidentity is NULL.

   Every field is nulled out and md's bookkeeping (index, used) finished
   before the caller drops any reference. Freeing an object can run a
   finalizer or weakref callback, which can hit a safepoint and
   transiently suspend this thread's critical section (PyMem_Malloc
   itself never does, see #1469), letting a concurrent resize run in
   between. If that resize caught this entry with some fields already
   NULL and others (or md->used, or the index) not yet updated, it would
   see md in a state that is neither "entry still there" nor "entry gone"
   and corrupt itself. Handing the objects to the caller, which decrefs
   them only once md is already fully self-consistent, means a concurrent
   resize always sees a coherent view. The GIL build needs the same order
   for its own reason: a __del__ there can release the GIL
   (Py_BEGIN_CRITICAL_SECTION is a no-op on that build), which would
   otherwise expose a half-deleted entry -- see #1489. entry->key is
   read/written as a plain pointer: unlike identity/value, no lock-free
   reader ever touches it (see the identity slot comment in
   freethreading.h). */
static inline void
_md_unlink_at_compact(MultiDictObject* md, size_t slot, entry_t* entry,
                      PyObject** pidentity, PyObject** pkey, PyObject** pvalue)
{
    *pkey = load_compact_identity(entry);
    *pidentity = NULL;
    *pvalue = load_value(entry);
    reset_compact_identity(entry);
    reset_value(entry);
    HTKEYS_SET_INDEX(
        md->keys, md->keys->log2_size, (Py_ssize_t)slot, DKIX_DUMMY);
    add_used(md, -1);
}

/* The same for a KIND_ANYSTR table. */
static inline void
_md_unlink_at_anystr(MultiDictObject* md, size_t slot, anystr_entry_t* entry,
                     PyObject** pidentity, PyObject** pkey, PyObject** pvalue)
{
    *pidentity = load_anystr_identity(entry);
    *pkey = entry->base.key;
    *pvalue = load_value(&entry->base);
    reset_anystr_identity(entry);
    entry->base.key = NULL;
    reset_value(&entry->base);
    HTKEYS_SET_INDEX(
        md->keys, md->keys->log2_size, (Py_ssize_t)slot, DKIX_DUMMY);
    add_used(md, -1);
}

/* Deletes the entry at `slot` of a KIND_COMPACT table, deferring the
 * decref (see reflist_t); used by _md_replace_locked()'s duplicate-cleanup
 * path and by update()'s post-pass on both builds. */
static int
md_del_at_deferred_compact(MultiDictObject* md, size_t slot, entry_t* entry,
                           reflist_t* defer)
{
    assert(md->keys != &empty_htkeys);
    PyObject* identity;
    PyObject* key;
    PyObject* value;
    _md_unlink_at_compact(md, slot, entry, &identity, &key, &value);
    return reflist_push2(defer, key, value);
}

/* The same for a KIND_ANYSTR table. */
static int
md_del_at_deferred_anystr(MultiDictObject* md, size_t slot,
                          anystr_entry_t* entry, reflist_t* defer)
{
    PyObject* identity;
    PyObject* key;
    PyObject* value;
    _md_unlink_at_anystr(md, slot, entry, &identity, &key, &value);
    return reflist_push3(defer, key, value, identity);
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
_removed_pairs_init(removed_pairs_t* removed)
{
    removed->key = NULL;
    removed->value = NULL;
    removed->spilled = false;
}

static void
_removed_pairs_release(removed_pairs_t* removed)
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
    return reflist_push2(&removed->more, key, value);
}

static inline int
_removed_pairs_add(removed_pairs_t* removed, PyObject* key, PyObject* value)
{
    if (removed->key == NULL) {
        removed->key = key;
        removed->value = value;
        return 0;
    }
    return _removed_pairs_spill(removed, key, value);
}

/* Deletes the entry at `slot` of a KIND_COMPACT table, handing the key and
   value to `removed`. Not through _md_unlink_at_compact(), which costs del
   d[key] up to 3 instructions. */
static inline int
_md_del_at_held_compact(MultiDictObject* md, htkeys_t* keys, size_t slot,
                        entry_t* entry, removed_pairs_t* removed)
{
    assert(keys == md->keys && kind_is_compact(keys->kind));
    PyObject* key = load_compact_identity(entry);
    reset_compact_identity(entry);
    PyObject* value = load_value(entry);
    reset_value(entry);
    HTKEYS_SET_INDEX(keys, keys->log2_size, (Py_ssize_t)slot, DKIX_DUMMY);
    add_used(md, -1);
    return _removed_pairs_add(removed, key, value);
}

/* The same for a KIND_ANYSTR table. */
static inline int
_md_del_at_held_anystr(MultiDictObject* md, htkeys_t* keys, size_t slot,
                       anystr_entry_t* entry, removed_pairs_t* removed)
{
    assert(keys == md->keys && !kind_is_compact(keys->kind));
    assert(keys != &empty_htkeys);
    PyObject* identity = load_anystr_identity(entry);
    PyObject* key = entry->base.key;
    reset_anystr_identity(entry);
    entry->base.key = NULL;
    PyObject* value = load_value(&entry->base);
    reset_value(&entry->base);
    HTKEYS_SET_INDEX(keys, keys->log2_size, (Py_ssize_t)slot, DKIX_DUMMY);
    add_used(md, -1);

    // an exact str: freeing it runs no code
    Py_XDECREF(identity);
    return _removed_pairs_add(removed, key, value);
}

/* Tells the watcher of the match of _md_del_locked() in `entry`; `first`
   is whether it is the first one. The watcher hears of it under the
   probe's identity, equal to the entry's. */
static inline void
_md_del_record(MultiDictObject* md, entry_t* entry, PyObject* identity,
               Py_hash_t hash, bool watched, bool first)
{
    if (watched) {
        if (first) {
            MD_WATCH_RECORD_SIMPLE(md, MultiDict_EVENT_BATCH_BEGIN);
        }
        MD_WATCH_RECORD(md,
                        MultiDict_EVENT_DELETED,
                        identity,
                        hash,
                        entry->key,
                        entry->value,
                        NULL);
    }
}

/* Caller holds md's critical section. Returns 1 if anything was removed,
 * 0 if not, -1 on error; md_del() raises the KeyError outside the section.
 * The removed pairs go to `removed`, so their finalizers run only once
 * every match is gone and cannot add one this call then removes.
 * `watched` is a constant at both call sites, so the unwatched copy
 * carries no watch code at all: testing md->watch inside the loop costs a
 * reload per record, since every decref and store may alias it. */
static int
_md_del_locked(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
               removed_pairs_t* removed, bool watched)
{
    bool ci = md->is_ci;
    bool found = false;
    int ret = 0;

    htkeys_t* keys = md->keys;
    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, keys, hash);

    if (kind_is_compact(keys->kind)) {
        entry_t* entry = NULL;
        if (ci) {
            for (;;) {
                HTKEYSITER_FIND_COMPACT_CI(&iter, identity, hash, entry);
                if (entry == NULL) {
                    break;
                }
                _md_del_record(md, entry, identity, hash, watched, !found);
                ret = _md_del_at_held_compact(
                    md, keys, iter.slot, entry, removed);
                found = true;
                if (ret < 0 || !keys->maybe_dups) {
                    break;  // on error, or past the key's only entry
                }
            }
        } else {
            for (;;) {
                HTKEYSITER_FIND_COMPACT_CS(&iter, identity, hash, entry);
                if (entry == NULL) {
                    break;
                }
                _md_del_record(md, entry, identity, hash, watched, !found);
                ret = _md_del_at_held_compact(
                    md, keys, iter.slot, entry, removed);
                found = true;
                if (ret < 0 || !keys->maybe_dups) {
                    break;  // on error, or past the key's only entry
                }
            }
        }
    } else {
        entry_t* entry = NULL;
        for (;;) {
            HTKEYSITER_FIND_ANYSTR(&iter, identity, hash, entry);
            if (entry == NULL) {
                break;
            }
            _md_del_record(md, entry, identity, hash, watched, !found);
            ret = _md_del_at_held_anystr(
                md, keys, iter.slot, as_anystr(entry), removed);
            found = true;
            if (ret < 0 || !keys->maybe_dups) {
                break;  // on error, or past the key's only entry
            }
        }
    }

    if (found) {
        bump_version(md);
        if (watched) {
            MD_WATCH_RECORD_SIMPLE(md, MultiDict_EVENT_BATCH_END);
        }
        if (ret == 0) {
            ret = 1;
        }
    }
    ASSERT_CONSISTENT(md);
    return ret;
}

/* The caller's reference to key keeps a borrowed identity alive through
   the finalizers that releasing the removed pairs can run. */
static int
md_del(MultiDictObject* md, PyObject* key)
{
    PyObject* identity = md_identity_or_new(md, key);
    if (UNLIKELY(identity == NULL)) {
        return -1;
    }
    Py_hash_t hash = unicode_hash(identity);
    if (UNLIKELY(hash == -1)) {
        md_identity_release(md, key, identity);
        return -1;
    }
    int found;
    removed_pairs_t removed;
    _removed_pairs_init(&removed);
    /* Unwatched on entry means nothing gets recorded, even if a __del__
       run by the delete attaches a watcher, so there is nothing to
       flush; a mutation that __del__ makes flushes its own records. */
    bool flush = false;
    Py_BEGIN_CRITICAL_SECTION(md);
    if (UNLIKELY(md->watch != NULL)) {
        found = _md_del_locked(md, identity, hash, &removed, true);
        flush = MD_WATCH_PENDING(md);
    } else {
        found = _md_del_locked(md, identity, hash, &removed, false);
    }
    Py_END_CRITICAL_SECTION();
    _removed_pairs_release(&removed);
    MD_WATCH_FLUSH_IF(md, flush);
    int ret = 0;
    if (found == 0) {
        PyErr_SetObject(PyExc_KeyError, key);
        ret = -1;
    } else if (found < 0) {
        ret = -1;
    }
    md_identity_release(md, key, identity);
    return ret;
}

static int
_md_contains_locked(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                    PyObject** pret)
{
    bool ci = md->is_ci;
    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, md->keys, hash);

    entry_t* entry = NULL;
    HTKEYSITER_FIND_FIRST(&iter, ci, identity, hash, entry);
    if (entry != NULL) {
        if (pret != NULL) {
            *pret = md_ensure_key(md, entry);
            if (UNLIKELY(*pret == NULL)) {
                return -1;
            }
        }
        return 1;
    }
    if (pret != NULL) {
        *pret = NULL;
    }
    return 0;
}

#ifdef Py_GIL_DISABLED

/* Sentinel meaning "could not complete lock-free"; never returned to
   md_contains()'s, md_get_one()'s or md_get_all()'s own caller. Distinct
   from 1 (found) / 0 (not found) / -1 (error). */
#define _MD_NEED_LOCK 2

/* Sets `matched_` to 1 if a compact entry matches, 0 to keep probing,
   -1 racing a writer. A compact entry keeps no hash of its own: it is
   read from the key, and a concurrent delete can free the key, so nothing
   is read through it until a reference is held, as in CPython's dict
   lookup on its unicode-only tables. A key that is the probe's own needs
   none: the caller's reference keeps that address from being reused.
   `sfx` is cs or ci as for compact_key_identity_cs(). */
#define _COMPACT_ENTRY_MATCHES(                                              \
    sfx, entry_, probe_, identity_, hash_, matched_)                         \
    do {                                                                     \
        PyObject* _em_key = load_compact_identity(entry_);                   \
        if (_em_key == NULL) {                                               \
            (matched_) = 0; /* not populated (or deleted) */                 \
        } else if (_em_key == (probe_)) {                                    \
            (matched_) = 1;                                                  \
        } else {                                                             \
            _em_key = try_get_ref(&(entry_)->key);                           \
            if (_em_key == NULL) {                                           \
                (matched_) = load_compact_identity(entry_) == NULL ? 0 : -1; \
            } else {                                                         \
                (matched_) = compact_key_hash_##sfx(_em_key) == (hash_) &&   \
                             str_cmp((identity_),                            \
                                     compact_key_identity_##sfx(_em_key));   \
                Py_DECREF(_em_key);                                          \
            }                                                                \
        }                                                                    \
    } while (0)

/* The full layout keeps the hash in the entry, so it goes first and a
   mismatch costs no reference traffic; then a pointer-equal identity is a
   match outright, since the probe's own reference keeps that object alive
   and its address cannot be reused. Only a different object needs the
   reference for the string compare. */
static inline int
_full_entry_matches(entry_t* entry, PyObject* identity, Py_hash_t hash)
{
    if (load_hash(as_anystr(entry)) != hash) {
        return 0;
    }
    PyObject* held = load_anystr_identity(as_anystr(entry));
    if (held == NULL) {
        return 0;  // not populated (or deleted)
    }
    if (held == identity) {
        return 1;
    }
    held = try_get_ref(&as_anystr(entry)->identity);
    if (held == NULL) {
        return load_anystr_identity(as_anystr(entry)) == NULL ? 0 : -1;
    }
    bool matched = str_cmp(identity, held);
    Py_DECREF(held);
    return matched;
}

/* The lock-free HTKEYSITER_FIND_COMPACT_CS(): 1 with the match in *pentry,
   0 at the end of the chain, -1 racing a writer. One function per table
   kind, so each stays small enough for GCC to inline into the lookups. */
static inline int
_md_lockfree_find_cs(htkeysiter_t* iter, PyObject* probe, PyObject* identity,
                     Py_hash_t hash, entry_t** pentry)
{
    entry_t* entries = HTKEYS_COMPACT_ENTRIES(iter->keys);
    for (; iter->index != DKIX_EMPTY; HTKEYSITER_NEXT_ACQUIRE(iter)) {
        if (UNLIKELY(iter->index < 0)) {
            continue;
        }
        entry_t* entry = entries + iter->index;
        int matched;
        _COMPACT_ENTRY_MATCHES(cs, entry, probe, identity, hash, matched);
        if (matched != 0) {
            *pentry = entry;
            return matched;
        }
    }
    return 0;
}

// _md_lockfree_find_cs() for a CIMultiDict's table.
static inline int
_md_lockfree_find_ci(htkeysiter_t* iter, PyObject* probe, PyObject* identity,
                     Py_hash_t hash, entry_t** pentry)
{
    entry_t* entries = HTKEYS_COMPACT_ENTRIES(iter->keys);
    for (; iter->index != DKIX_EMPTY; HTKEYSITER_NEXT_ACQUIRE(iter)) {
        if (UNLIKELY(iter->index < 0)) {
            continue;
        }
        entry_t* entry = entries + iter->index;
        int matched;
        _COMPACT_ENTRY_MATCHES(ci, entry, probe, identity, hash, matched);
        if (matched != 0) {
            *pentry = entry;
            return matched;
        }
    }
    return 0;
}

// _md_lockfree_find_cs() for a KIND_ANYSTR table.
static inline int
_md_lockfree_find_anystr(htkeysiter_t* iter, PyObject* identity,
                         Py_hash_t hash, entry_t** pentry)
{
    anystr_entry_t* entries = HTKEYS_ANYSTR_ENTRIES(iter->keys);
    for (; iter->index != DKIX_EMPTY; HTKEYSITER_NEXT_ACQUIRE(iter)) {
        if (UNLIKELY(iter->index < 0)) {
            continue;
        }
        entry_t* entry = &entries[iter->index].base;
        int matched = _full_entry_matches(entry, identity, hash);
        if (matched != 0) {
            *pentry = entry;
            return matched;
        }
    }
    return 0;
}

static inline int
_md_lockfree_find(htkeysiter_t* iter, bool ci, PyObject* probe,
                  PyObject* identity, Py_hash_t hash, entry_t** pentry)
{
    if (!kind_is_compact(iter->keys->kind)) {
        return _md_lockfree_find_anystr(iter, identity, hash, pentry);
    }
    if (ci) {
        return _md_lockfree_find_ci(iter, probe, identity, hash, pentry);
    }
    return _md_lockfree_find_cs(iter, probe, identity, hash, pentry);
}

static inline int
_md_contains_lockfree(MultiDictObject* md, PyObject* probe, PyObject* identity,
                      Py_hash_t hash)
{
    bool ci = md->is_ci;
    htkeys_t* keys = _md_reader_enter(md);
    htkeysiter_t iter;
    HTKEYSITER_INIT_ACQUIRE(&iter, keys, hash);

    entry_t* entry;
    int matched = _md_lockfree_find(&iter, ci, probe, identity, hash, &entry);
    int result = matched < 0 ? _MD_NEED_LOCK : matched;

    _md_reader_exit(md, keys);
    return result;
}

/* COLD for the reason given at _md_get_one_retry_locked(). */
COLD static int
_md_contains_retry_locked(MultiDictObject* md, PyObject* identity,
                          Py_hash_t hash)
{
    int result;
    Py_BEGIN_CRITICAL_SECTION(md);
    result = _md_contains_locked(md, identity, hash, NULL);
    Py_END_CRITICAL_SECTION();
    return result;
}

#endif /* Py_GIL_DISABLED */

static inline int
_md_contains_identity(MultiDictObject* md, PyObject* probe, PyObject* identity,
                      Py_hash_t hash)
{
    int result;
#ifdef Py_GIL_DISABLED
    result = _md_contains_lockfree(md, probe, identity, hash);
    if (result == _MD_NEED_LOCK) {
        result = _md_contains_retry_locked(md, identity, hash);
    }
#else
    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, md->keys, hash);
    entry_t* entry = NULL;
    HTKEYSITER_FIND_FIRST(&iter, md->is_ci, identity, hash, entry);
    result = entry != NULL;
#endif
    return result;
}

/* A key whose identity has to be computed. */
static int
_md_contains_owned(MultiDictObject* md, PyObject* key)
{
    identity_hash_t ih = md_calc_identity_hash(md, key);
    PyObject* identity = ih.identity;
    Py_hash_t hash = ih.hash;
    if (UNLIKELY(identity == NULL)) {
        return -1;
    }
    int result = _md_contains_identity(md, key, identity, hash);
    Py_DECREF(identity);
    return result;
}

static int
md_contains(MultiDictObject* md, PyObject* key)
{
    if (!PyUnicode_Check(key)) {
        return 0;
    }
    PyObject* identity = md_borrow_identity(md, key);
    if (identity == NULL) {
        return _md_contains_owned(md, key);
    }
    Py_hash_t hash = unicode_hash(identity);
    if (UNLIKELY(hash == -1)) {
        return -1;
    }
    return _md_contains_identity(md, key, identity, hash);
}

/* md_contains() that also returns the stored key in *pret.  Only the view
   set operations need it. */
static int
md_find_key(MultiDictObject* md, PyObject* key, PyObject** pret)
{
    *pret = NULL;
    if (!PyUnicode_Check(key)) {
        return 0;
    }

    identity_hash_t ih = md_calc_identity_hash(md, key);
    PyObject* identity = ih.identity;
    Py_hash_t hash = ih.hash;
    if (UNLIKELY(identity == NULL)) {
        return -1;
    }

    int result;
    Py_BEGIN_CRITICAL_SECTION(md);
    result = _md_contains_locked(md, identity, hash, pret);
    Py_END_CRITICAL_SECTION();
    Py_DECREF(identity);
    return result;
}

static inline PyObject*
_md_get_one_locked(MultiDictObject* md, PyObject* identity, Py_hash_t hash)
{
    bool ci = md->is_ci;
    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, md->keys, hash);

    entry_t* entry = NULL;
    HTKEYSITER_FIND_FIRST(&iter, ci, identity, hash, entry);
    if (entry != NULL) {
        return Py_NewRef(entry->value);
    }
    return NULL;
}

/* A lookup's result: -1 on error, 0 if missing, 1 if found, or
   _MD_NEED_LOCK inside the FT helpers.  value is a new reference when
   result is 1 and NULL otherwise.  Returned by value for the reason given
   at identity_hash_t. */
typedef struct {
    int result;
    PyObject* value;
} lookup_t;

#ifdef Py_GIL_DISABLED

// The caller is a registered reader of keys.
static inline lookup_t
_md_lockfree_get(htkeys_t* keys, bool ci, PyObject* probe, PyObject* identity,
                 Py_hash_t hash)
{
    htkeysiter_t iter;
    HTKEYSITER_INIT_ACQUIRE(&iter, keys, hash);

    entry_t* entry;
    lookup_t ret = {
        _md_lockfree_find(&iter, ci, probe, identity, hash, &entry), NULL};
    if (ret.result < 0) {
        ret.result = _MD_NEED_LOCK;  // racing a concurrent change
    } else if (ret.result > 0) {
        ret.value = try_get_ref(&entry->value);
        if (ret.value == NULL) {
            ret.result = _MD_NEED_LOCK;
        }
    }
    return ret;
}

static inline lookup_t
_md_get_one_lockfree(MultiDictObject* md, PyObject* probe, PyObject* identity,
                     Py_hash_t hash)
{
    bool ci = md->is_ci;
    htkeys_t* keys = _md_reader_enter(md);
    lookup_t ret = _md_lockfree_get(keys, ci, probe, identity, hash);
    _md_reader_exit(md, keys);
    return ret;
}

/* COLD keeps the critical section's state, and so a stack canary, out of
   every lookup that never needs the lock. */
COLD static PyObject*
_md_get_one_retry_locked(MultiDictObject* md, PyObject* identity,
                         Py_hash_t hash)
{
    PyObject* ret;
    Py_BEGIN_CRITICAL_SECTION(md);
    ret = _md_get_one_locked(md, identity, hash);
    Py_END_CRITICAL_SECTION();
    return ret;
}

static inline lookup_t
_md_get_one_identity(MultiDictObject* md, PyObject* probe, PyObject* identity,
                     Py_hash_t hash)
{
    lookup_t ret = _md_get_one_lockfree(md, probe, identity, hash);
    if (ret.result == _MD_NEED_LOCK) {
        ret.value = _md_get_one_retry_locked(md, identity, hash);
        ret.result = ret.value != NULL;
    }
    return ret;
}

/* getall() on a table without duplicate keys is a get(); one with them
   takes the critical section. */
static inline lookup_t
_md_get_all_lockfree(MultiDictObject* md, PyObject* probe, PyObject* identity,
                     Py_hash_t hash)
{
    bool ci = md->is_ci;
    htkeys_t* keys = _md_reader_enter(md);
    lookup_t ret = {_MD_NEED_LOCK, NULL};
    if (!htkeys_maybe_dups(keys)) {
        ret = _md_lockfree_get(keys, ci, probe, identity, hash);
    }
    _md_reader_exit(md, keys);
    return ret;
}

#else /* !Py_GIL_DISABLED */

static inline lookup_t
_md_get_one_identity(MultiDictObject* md, PyObject* probe, PyObject* identity,
                     Py_hash_t hash)
{
    lookup_t ret = {0, _md_get_one_locked(md, identity, hash)};
    ret.result = ret.value != NULL;
    return ret;
}

#endif /* Py_GIL_DISABLED */

static lookup_t
md_get_one(MultiDictObject* md, PyObject* key)
{
    PyObject* identity = md_borrow_identity(md, key);
    if (identity == NULL) {
        /* A key whose identity has to be computed.  Open-codes
           md_calc_identity_hash(), which GCC calls out of line here, failing
           its rule in tools/check_inlining.py; keep the two in step. */
        lookup_t ret = {-1, NULL};
        identity = md_calc_identity(md, key);
        if (UNLIKELY(identity == NULL)) {
            return ret;
        }
        Py_hash_t hash = unicode_hash(identity);
        if (hash != -1) {
            ret = _md_get_one_identity(md, key, identity, hash);
        }
        Py_DECREF(identity);
        return ret;
    }
    Py_hash_t hash = unicode_hash(identity);
    if (UNLIKELY(hash == -1)) {
        lookup_t ret = {-1, NULL};
        return ret;
    }
    return _md_get_one_identity(md, key, identity, hash);
}

// Caller holds md's critical section
static int
_md_set_default_locked(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                       PyObject* key, PyObject* value, PyObject** result)
{
    bool ci = md->is_ci;
    ASSERT_CONSISTENT(md);

    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, md->keys, hash);

    entry_t* entry = NULL;
    HTKEYSITER_FIND_FIRST(&iter, ci, identity, hash, entry);
    if (entry != NULL) {
        ASSERT_CONSISTENT(md);
        *result = Py_NewRef(entry->value);
        return 1;
    }

    if (UNLIKELY(md_add_with_hash(md,
                                  hash,
                                  identity,
                                  key,
                                  value,
                                  md_key_fits(md, key, identity),
                                  MD_SLOT_FIND) < 0)) {
        return -1;
    }

    ASSERT_CONSISTENT(md);
    *result = Py_NewRef(value);
    return 0;
}

static int
md_set_default(MultiDictObject* md, PyObject* key, PyObject* value,
               PyObject** result)
{
    *result = NULL;
    if (value == NULL) {
        /* The caller wants the implicit None default.  Substituting it
           here keeps it out of the critical section below, where the
           pythoncapi_compat shim for Py_GetConstant() used on 3.11 and
           3.12 could allocate on its first call. */
        value = md->state->none;
    }
    identity_hash_t ih = md_calc_identity_hash(md, key);
    PyObject* identity = ih.identity;
    Py_hash_t hash = ih.hash;
    if (UNLIKELY(identity == NULL)) {
        return -1;
    }
    int ret;
    bool flush;
    Py_BEGIN_CRITICAL_SECTION(md);
    ret = _md_set_default_locked(md, identity, hash, key, value, result);
    flush = MD_WATCH_PENDING(md);
    Py_END_CRITICAL_SECTION();
    Py_DECREF(identity);
    MD_WATCH_FLUSH_IF(md, flush);
    return ret;
}

/* A new reference to the value of the match of _md_pop_one_locked() in
   `entry`, which the watcher hears of. */
static inline PyObject*
_md_pop_one_record(MultiDictObject* md, entry_t* entry, PyObject* identity,
                   Py_hash_t hash, bool watched)
{
    PyObject* value = Py_NewRef(entry->value);
    if (watched) {
        MD_WATCH_RECORD(md,
                        MultiDict_EVENT_DELETED,
                        identity,
                        hash,
                        entry->key,
                        value,
                        NULL);
    }
    return value;
}

/* Caller holds md's critical section. `watched` is a constant at both
 * call sites; see _md_del_locked(). */
static int
_md_pop_one_locked(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                   PyObject** ret, bool watched)
{
    bool ci = md->is_ci;
    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, md->keys, hash);

    PyObject* value;
    PyObject* old_identity;
    PyObject* old_key;
    PyObject* old_value;
    if (kind_is_compact(iter.keys->kind)) {
        entry_t* entry = NULL;
        if (ci) {
            HTKEYSITER_FIND_COMPACT_CI(&iter, identity, hash, entry);
        } else {
            HTKEYSITER_FIND_COMPACT_CS(&iter, identity, hash, entry);
        }
        if (entry == NULL) {
            ASSERT_CONSISTENT(md);
            return 0;
        }
        value = _md_pop_one_record(md, entry, identity, hash, watched);
        _md_unlink_at_compact(
            md, iter.slot, entry, &old_identity, &old_key, &old_value);
    } else {
        entry_t* entry = NULL;
        HTKEYSITER_FIND_ANYSTR(&iter, identity, hash, entry);
        if (entry == NULL) {
            ASSERT_CONSISTENT(md);
            return 0;
        }
        value = _md_pop_one_record(md, entry, identity, hash, watched);
        _md_unlink_at_anystr(md,
                             iter.slot,
                             as_anystr(entry),
                             &old_identity,
                             &old_key,
                             &old_value);
    }
    Py_XDECREF(old_identity);
    Py_XDECREF(old_key);
    Py_XDECREF(old_value);
    *ret = value;
    bump_version(md);
    ASSERT_CONSISTENT(md);
    return 1;
}

static int
md_pop_one(MultiDictObject* md, PyObject* key, PyObject** ret)
{
    identity_hash_t ih = md_calc_identity_hash(md, key);
    PyObject* identity = ih.identity;
    Py_hash_t hash = ih.hash;
    if (UNLIKELY(identity == NULL)) {
        return -1;
    }
    int result;
    // see md_del()
    bool flush = false;
    Py_BEGIN_CRITICAL_SECTION(md);
    if (UNLIKELY(md->watch != NULL)) {
        result = _md_pop_one_locked(md, identity, hash, ret, true);
        flush = MD_WATCH_PENDING(md);
    } else {
        result = _md_pop_one_locked(md, identity, hash, ret, false);
    }
    Py_END_CRITICAL_SECTION();
    Py_DECREF(identity);
    MD_WATCH_FLUSH_IF(md, flush);
    return result;
}

static int
_md_getall_visit(void* user_data, PyObject* identity, Py_hash_t hash,
                 PyObject* key, PyObject* value)
{
    (void)identity;
    (void)hash;
    (void)key;  // value-only walk
    if (UNLIKELY(reflist_push((reflist_t*)user_data, Py_NewRef(value)) < 0)) {
        return -1;
    }
    return 1;
}

/* The result of getall() or popall(): 1 with the collected values as a
   list in *ret, 0 if there are none, -1 on error or if the walk failed. */
static inline int
_md_values_to_list(reflist_t* values, bool failed, PyObject** ret)
{
    if (failed) {
        reflist_clear(values);
        return -1;
    }
    if (reflist_empty(values)) {
        return 0;
    }
    *ret = reflist_to_list(values);
    return *ret != NULL ? 1 : -1;
}

// The getall() of a single value, whose reference it steals.
static inline int
_md_single_value_list(PyObject* value, PyObject** ret)
{
    *ret = PyList_New(1);
    if (UNLIKELY(*ret == NULL)) {
        Py_DECREF(value);
        return -1;
    }
    PyList_SET_ITEM(*ret, 0, value);
    return 1;
}

static inline int
_md_get_all_locked(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                   PyObject** ret)
{
    reflist_t values;
    reflist_init(&values);
    Py_ssize_t count = 0;
    PyObject* value = NULL;
    int single = -1;
    Py_BEGIN_CRITICAL_SECTION(md);
    if (!md->keys->maybe_dups) {
        // at most one entry: a plain lookup, without the walk's seen set
        value = _md_get_one_locked(md, identity, hash);
        single = value != NULL;
    } else {
        count = md_walk_with_hash(
            md, identity, hash, false, _md_getall_visit, &values);
    }
    Py_END_CRITICAL_SECTION();
    if (single >= 0) {
        if (single == 0) {
            return 0;
        }
        return _md_single_value_list(value, ret);
    }
    return _md_values_to_list(&values, count < 0, ret);
}

static inline int
_md_get_all_identity(MultiDictObject* md, PyObject* probe, PyObject* identity,
                     Py_hash_t hash, PyObject** ret)
{
#ifdef Py_GIL_DISABLED
    lookup_t single = _md_get_all_lockfree(md, probe, identity, hash);
    if (single.result == 1) {
        return _md_single_value_list(single.value, ret);
    }
    if (single.result == 0) {
        return 0;
    }
#else
    (void)probe;
#endif
    return _md_get_all_locked(md, identity, hash, ret);
}

#undef _MD_NEED_LOCK

static int
md_get_all(MultiDictObject* md, PyObject* key, PyObject** ret)
{
    *ret = NULL;
    PyObject* identity = md_identity_or_new(md, key);
    if (UNLIKELY(identity == NULL)) {
        return -1;
    }
    int found = -1;
    Py_hash_t hash = unicode_hash(identity);
    if (hash != -1) {
        found = _md_get_all_identity(md, key, identity, hash, ret);
    }
    md_identity_release(md, key, identity);
    return found;
}

/* Collects the value of the match of _md_pop_all_locked() in `entry` and
   tells the watcher of it. The batch begins with the first match that
   gets past the push, which sets *pbegun. */
static inline int
_md_pop_all_record(MultiDictObject* md, entry_t* entry, PyObject* identity,
                   Py_hash_t hash, reflist_t* values, bool* pbegun)
{
    if (UNLIKELY(reflist_push(values, Py_NewRef(entry->value)) < 0)) {
        return -1;
    }
    bump_version(md);
    if (!*pbegun) {
        *pbegun = true;
        MD_WATCH_RECORD_SIMPLE(md, MultiDict_EVENT_BATCH_BEGIN);
    }
    MD_WATCH_RECORD(md,
                    MultiDict_EVENT_DELETED,
                    identity,
                    hash,
                    entry->key,
                    entry->value,
                    NULL);
    return 0;
}

/* Caller holds md's critical section. The removed pairs go to `removed`,
 * as in _md_del_locked(); `values` collects the result. */
COLD static int
_md_pop_all_locked(MultiDictObject* md, PyObject* identity, Py_hash_t hash,
                   reflist_t* values, removed_pairs_t* removed)
{
    bool ci = md->is_ci;
    if (md_len(md) == 0) {
        return 0;
    }

    bool begun = false;
    int ret = 0;
    htkeys_t* keys = md->keys;

    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, keys, hash);

    if (kind_is_compact(keys->kind)) {
        entry_t* entry = NULL;
        if (ci) {
            for (;;) {
                HTKEYSITER_FIND_COMPACT_CI(&iter, identity, hash, entry);
                if (entry == NULL) {
                    break;
                }
                ret = _md_pop_all_record(
                    md, entry, identity, hash, values, &begun);
                if (UNLIKELY(ret < 0)) {
                    break;
                }
                ret = _md_del_at_held_compact(
                    md, keys, iter.slot, entry, removed);
                if (ret < 0 || !keys->maybe_dups) {
                    break;  // on error, or past the key's only entry
                }
            }
        } else {
            for (;;) {
                HTKEYSITER_FIND_COMPACT_CS(&iter, identity, hash, entry);
                if (entry == NULL) {
                    break;
                }
                ret = _md_pop_all_record(
                    md, entry, identity, hash, values, &begun);
                if (UNLIKELY(ret < 0)) {
                    break;
                }
                ret = _md_del_at_held_compact(
                    md, keys, iter.slot, entry, removed);
                if (ret < 0 || !keys->maybe_dups) {
                    break;  // on error, or past the key's only entry
                }
            }
        }
    } else {
        entry_t* entry = NULL;
        for (;;) {
            HTKEYSITER_FIND_ANYSTR(&iter, identity, hash, entry);
            if (entry == NULL) {
                break;
            }
            ret =
                _md_pop_all_record(md, entry, identity, hash, values, &begun);
            if (UNLIKELY(ret < 0)) {
                break;
            }
            ret = _md_del_at_held_anystr(
                md, keys, iter.slot, as_anystr(entry), removed);
            if (ret < 0 || !keys->maybe_dups) {
                break;  // on error, or past the key's only entry
            }
        }
    }

    if (begun) {
        MD_WATCH_RECORD_SIMPLE(md, MultiDict_EVENT_BATCH_END);
    }
    ASSERT_CONSISTENT(md);
    return ret;
}

COLD static int
md_pop_all(MultiDictObject* md, PyObject* key, PyObject** ret)
{
    identity_hash_t ih = md_calc_identity_hash(md, key);
    PyObject* identity = ih.identity;
    Py_hash_t hash = ih.hash;
    if (UNLIKELY(identity == NULL)) {
        return -1;
    }
    reflist_t values;
    reflist_init(&values);
    removed_pairs_t removed;
    _removed_pairs_init(&removed);
    int tmp;
    bool flush;
    Py_BEGIN_CRITICAL_SECTION(md);
    tmp = _md_pop_all_locked(md, identity, hash, &values, &removed);
    flush = MD_WATCH_PENDING(md);
    Py_END_CRITICAL_SECTION();
    _removed_pairs_release(&removed);
    Py_DECREF(identity);
    MD_WATCH_FLUSH_IF(md, flush);
    return _md_values_to_list(&values, tmp < 0, ret);
}

/* The last live entry at or before *ppos, which it moves there; md has
   one. One loop per kind, so each steps by a constant entry size. */
static entry_t*
_md_last_live(htkeys_t* keys, Py_ssize_t* ppos)
{
    Py_ssize_t pos = *ppos;
    entry_t* entry;
    if (kind_is_compact(keys->kind)) {
        entry_t* entries = HTKEYS_COMPACT_ENTRIES(keys);
        while (entry_is_hole(entries + pos)) {
            pos--;
        }
        entry = entries + pos;
    } else {
        anystr_entry_t* entries = HTKEYS_ANYSTR_ENTRIES(keys);
        while (entry_is_hole((const entry_t*)(entries + pos))) {
            pos--;
        }
        entry = &entries[pos].base;
    }
    assert(pos >= 0);
    *ppos = pos;
    return entry;
}

static PyObject*
_md_pop_item_locked(MultiDictObject* md)
{
    bool ci = md->is_ci;
    if (md->used == 0) {
        PyErr_SetString(PyExc_KeyError, "empty multidict");
        return NULL;
    }

    htkeys_t* keys = md->keys;
    uint8_t kind = keys->kind;

    Py_ssize_t pos = keys->nentries - 1;
    entry_t* entry = _md_last_live(keys, &pos);

    Py_hash_t hash = !kind_is_compact(kind) ? as_anystr(entry)->hash
                     : ci                   ? compact_key_hash_ci(entry->key)
                                            : compact_key_hash_cs(entry->key);
    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, keys, hash);

    for (; iter.index != pos; HTKEYSITER_NEXT(&iter)) {
    }
    MD_WATCH_RECORD(md,
                    MultiDict_EVENT_DELETED,
                    !kind_is_compact(kind) ? as_anystr(entry)->identity
                    : ci ? compact_key_identity_ci(entry->key)
                         : compact_key_identity_cs(entry->key),
                    hash,
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
       Python code that mutates md (an istr key's __str__, or on 3.11 a
       collection the tuple triggers), so it runs once the pair is
       gone. */
    PyObject* identity;
    PyObject* key;
    PyObject* value;
    if (kind_is_compact(kind)) {
        _md_unlink_at_compact(md, iter.slot, entry, &identity, &key, &value);
    } else {
        _md_unlink_at_anystr(
            md, iter.slot, as_anystr(entry), &identity, &key, &value);
    }
    bump_version(md);
    ASSERT_CONSISTENT(md);

    // a compact table's key is already what md_calc_key() would return
    if (identity != NULL) {
        Py_SETREF(key, md_calc_key(md, key, identity));
        Py_DECREF(identity);
        if (UNLIKELY(key == NULL)) {
            Py_DECREF(value);
            return NULL;
        }
    }
    PyObject* ret = PyTuple_New(2);
    if (UNLIKELY(ret == NULL)) {
        Py_DECREF(key);
        Py_DECREF(value);
        return NULL;
    }
    PyTuple_SET_ITEM(ret, 0, key);
    PyTuple_SET_ITEM(ret, 1, value);
    return ret;
}

static PyObject*
md_pop_item(MultiDictObject* md)
{
    PyObject* ret;
    bool flush;
    Py_BEGIN_CRITICAL_SECTION(md);
    ret = _md_pop_item_locked(md);
    flush = MD_WATCH_PENDING(md);
    Py_END_CRITICAL_SECTION();
    MD_WATCH_FLUSH_IF(md, flush);
    return ret;
}

/* Later matches of a replaced key are rare, so their reflist lives on
   the heap: in the caller's frame it cost every d[key] = v. The watcher
   hears of the delete only once the allocation can no longer stop it. */
static inline int
_md_replace_dups_prepare(MultiDictObject* md, entry_t* entry,
                         PyObject* identity, Py_hash_t hash, bool watched,
                         reflist_t** dups)
{
    if (*dups == NULL) {
        *dups = PyMem_Malloc(sizeof(reflist_t));
        if (UNLIKELY(*dups == NULL)) {
            PyErr_NoMemory();
            return -1;
        }
        reflist_init(*dups);
    }
    if (watched) {
        MD_WATCH_RECORD(md,
                        MultiDict_EVENT_DELETED,
                        identity,
                        hash,
                        entry->key,
                        entry->value,
                        NULL);
    }
    return 0;
}

COLD static int
_md_replace_del_dup_compact(MultiDictObject* md, size_t slot, entry_t* entry,
                            PyObject* identity, Py_hash_t hash, bool watched,
                            reflist_t** dups)
{
    if (UNLIKELY(_md_replace_dups_prepare(
                     md, entry, identity, hash, watched, dups) < 0)) {
        return -1;
    }
    return md_del_at_deferred_compact(md, slot, entry, *dups);
}

COLD static int
_md_replace_del_dup_anystr(MultiDictObject* md, size_t slot, entry_t* entry,
                           PyObject* identity, Py_hash_t hash, bool watched,
                           reflist_t** dups)
{
    if (UNLIKELY(_md_replace_dups_prepare(
                     md, entry, identity, hash, watched, dups) < 0)) {
        return -1;
    }
    return md_del_at_deferred_anystr(md, slot, as_anystr(entry), *dups);
}

COLD static void
_md_replace_free_dups(reflist_t* dups)
{
    reflist_clear(dups);
    PyMem_Free(dups);
}

#define _MD_REPLACE_STALE 2

/* Stores `key` into the first match of _md_replace_pass() in a
   KIND_COMPACT table, handing the old key to *old_key_out. The same key
   object needs no store, and in a compact table no new publication
   either. */
static inline void
_md_replace_key_compact(entry_t* entry, PyObject* key, PyObject** old_key_out)
{
    if (key != entry->key) {
        *old_key_out = entry->key;
        replace_compact_key(entry, Py_NewRef(key));
    }
}

/* The same for a KIND_ANYSTR table. */
static inline void
_md_replace_key_anystr(anystr_entry_t* entry, PyObject* key,
                       PyObject** old_key_out)
{
    if (key != entry->base.key) {
        *old_key_out = entry->base.key;
        replace_anystr_key(entry, Py_NewRef(key));
    }
}

/* A match of _md_replace_pass() at `slot`, past the one to keep: the
   first gets the new value, its key already stored by the caller, the
   rest are deleted; *pfound tells which. `compact` is a constant at each
   call site. 0 on success, -1 on error, _MD_REPLACE_STALE when md's table
   moved under it. */
static inline int
_md_replace_matched(MultiDictObject* md, size_t slot, entry_t* entry,
                    PyObject* key, PyObject* value, PyObject* identity,
                    Py_hash_t hash, PyObject** old_value_out, reflist_t** dups,
                    bool watched, bool compact, bool* pfound)
{
#ifdef Py_GIL_DISABLED
    htkeys_t* keys_before = md->keys;
    uint64_t version_before = md->version;
#endif
    if (!*pfound) {
        *pfound = true;
        /* old_value decref deferred to the caller, past the critical
           section -- see reflist_t */
        PyObject* old_value = load_value(entry);
        publish_value(entry, Py_NewRef(value));
        if (watched) {
            MD_WATCH_RECORD(md,
                            MultiDict_EVENT_REPLACED,
                            identity,
                            hash,
                            key,
                            value,
                            old_value);
        }
        *old_value_out = old_value;
    } else if (compact) {
        if (UNLIKELY(_md_replace_del_dup_compact(
                         md, slot, entry, identity, hash, watched, dups) <
                     0)) {
            return -1;
        }
    } else if (UNLIKELY(_md_replace_del_dup_anystr(
                            md, slot, entry, identity, hash, watched, dups) <
                        0)) {
        return -1;
    }
#ifdef Py_GIL_DISABLED
    /* Checking the pointer alone isn't enough: a freed table can get
       reallocated at the very same address by a later resize (same size
       class, common in practice), which would make a pointer-only check
       miss the change. Every mutation bumps md->version, including ones
       that don't otherwise touch md->keys, so compare both. */
    if (md->keys != keys_before || md->version != version_before) {
        return _MD_REPLACE_STALE;
    }
#endif
    return 0;
}

/* One attempt of _md_replace_locked(), one loop per kind: 1 if `key`
   isn't there, 0 once it is replaced, -1 on error, _MD_REPLACE_STALE when
   md's table moved under it. *pfound carries over to the next attempt.

   `replaced` is the one entry to keep. Later matches are deleted, which
   turns their slots into DKIX_DUMMY, so only this one can show up again
   when HTKEYSITER_NEXT() repeats a slot. Equal keys sit on their hash
   chain in insertion order, before and after a resize alike, so on a
   retry the first match is the entry an earlier attempt already
   replaced. */
static inline int
_md_replace_pass(MultiDictObject* md, PyObject* key, PyObject* value,
                 PyObject* identity, Py_hash_t hash, PyObject** old_key_out,
                 PyObject** old_value_out, reflist_t** dups, bool watched,
                 bool* pfound)
{
    bool ci = md->is_ci;
    htkeysiter_t iter;
    HTKEYSITER_INIT(&iter, md->keys, hash);
    Py_ssize_t replaced = -1;
    bool skip_first = *pfound;
    int ret = 0;

    if (kind_is_compact(md->keys->kind)) {
        entry_t* entry = NULL;
        if (ci) {
            while (ret == 0) {
                HTKEYSITER_FIND_COMPACT_CI(&iter, identity, hash, entry);
                if (entry == NULL) {
                    break;
                }
                if (iter.index == replaced) {
                    continue;
                }
                if (skip_first || !*pfound) {
                    /* Checked only where the key gets stored: a miss leaves
                       it to md_add_with_hash(), and the same key object fits
                       by definition. */
                    if (!skip_first && key != entry->key &&
                        UNLIKELY(!md_key_fits(md, key, identity))) {
                        if (UNLIKELY(md_to_anystr(md) < 0)) {
                            return -1;
                        }
                        return _MD_REPLACE_STALE;  // entry is in the old table
                    }
                    replaced = iter.index;
                    if (skip_first) {
                        skip_first = false;
                        continue;
                    }
                }
                if (!*pfound) {
                    _md_replace_key_compact(entry, key, old_key_out);
                }
                ret = _md_replace_matched(md,
                                          iter.slot,
                                          entry,
                                          key,
                                          value,
                                          identity,
                                          hash,
                                          old_value_out,
                                          dups,
                                          watched,
                                          true,
                                          pfound);
                if (!md->keys->maybe_dups) {
                    break;  // the key's only entry
                }
            }
        } else {
            while (ret == 0) {
                HTKEYSITER_FIND_COMPACT_CS(&iter, identity, hash, entry);
                if (entry == NULL) {
                    break;
                }
                if (iter.index == replaced) {
                    continue;
                }
                if (skip_first || !*pfound) {
                    /* Checked only where the key gets stored: a miss leaves
                       it to md_add_with_hash(), and the same key object fits
                       by definition. */
                    if (!skip_first && key != entry->key &&
                        UNLIKELY(!md_key_fits(md, key, identity))) {
                        if (UNLIKELY(md_to_anystr(md) < 0)) {
                            return -1;
                        }
                        return _MD_REPLACE_STALE;  // entry is in the old table
                    }
                    replaced = iter.index;
                    if (skip_first) {
                        skip_first = false;
                        continue;
                    }
                }
                if (!*pfound) {
                    _md_replace_key_compact(entry, key, old_key_out);
                }
                ret = _md_replace_matched(md,
                                          iter.slot,
                                          entry,
                                          key,
                                          value,
                                          identity,
                                          hash,
                                          old_value_out,
                                          dups,
                                          watched,
                                          true,
                                          pfound);
                if (!md->keys->maybe_dups) {
                    break;  // the key's only entry
                }
            }
        }
    } else {
        entry_t* entry = NULL;
        while (ret == 0) {
            HTKEYSITER_FIND_ANYSTR(&iter, identity, hash, entry);
            if (entry == NULL) {
                break;
            }
            if (iter.index == replaced) {
                continue;
            }
            if (skip_first || !*pfound) {
                replaced = iter.index;
                if (skip_first) {
                    skip_first = false;
                    continue;
                }
            }
            if (!*pfound) {
                _md_replace_key_anystr(as_anystr(entry), key, old_key_out);
            }
            ret = _md_replace_matched(md,
                                      iter.slot,
                                      entry,
                                      key,
                                      value,
                                      identity,
                                      hash,
                                      old_value_out,
                                      dups,
                                      watched,
                                      false,
                                      pfound);
            if (!md->keys->maybe_dups) {
                break;  // the key's only entry
            }
        }
    }
    if (ret != 0) {
        return ret;
    }
    return *pfound ? 0 : 1;
}

/* Adds `key` when it isn't there. The fit is checked only on that add,
 * not carried from the identity: kept live across the replace loop, it
 * cost d[key] = v up to 4%. */
static int
_md_replace_locked(MultiDictObject* md, PyObject* key, PyObject* value,
                   PyObject* identity, Py_hash_t hash, PyObject** old_key_out,
                   PyObject** old_value_out, reflist_t** dups, bool watched)
{
    if (watched) {
        MD_WATCH_RECORD_SIMPLE(md, MultiDict_EVENT_BATCH_BEGIN);
    }
    bool found = false;
    int ret;
    /* Retries on a concurrent resize (Py_GIL_DISABLED only), or after
     * md_to_anystr(); deferred decrefs mean nothing here can trigger a
     * resize, so this shouldn't loop more than once. */
    do {
        ret = _md_replace_pass(md,
                               key,
                               value,
                               identity,
                               hash,
                               old_key_out,
                               old_value_out,
                               dups,
                               watched,
                               &found);
    } while (ret == _MD_REPLACE_STALE);
    if (ret == 0) {
        bump_version(md);
    } else if (ret > 0) {
        ret = md_add_with_hash(md,
                               hash,
                               identity,
                               key,
                               value,
                               md_key_fits(md, key, identity),
                               MD_SLOT_FIND);
    }
    if (watched) {
        MD_WATCH_RECORD_SIMPLE(md, MultiDict_EVENT_BATCH_END);
    }
    return ret;
}

#undef _MD_REPLACE_STALE

// See md_del() on the borrowed identity.
static int
md_replace(MultiDictObject* md, PyObject* key, PyObject* value)
{
    PyObject* identity = md_identity_or_new(md, key);
    if (UNLIKELY(identity == NULL)) {
        return -1;
    }
    Py_hash_t hash = unicode_hash(identity);
    if (UNLIKELY(hash == -1)) {
        md_identity_release(md, key, identity);
        return -1;
    }
    // The one replaced entry's refs; later matches go to dups
    PyObject* old_key = NULL;
    PyObject* old_value = NULL;
    reflist_t* dups = NULL;
    int ret;
    bool flush;
    Py_BEGIN_CRITICAL_SECTION(md);
    ret = _md_replace_locked(md,
                             key,
                             value,
                             identity,
                             hash,
                             &old_key,
                             &old_value,
                             &dups,
                             md->watch != NULL);
    flush = MD_WATCH_PENDING(md);
    ASSERT_CONSISTENT(md);
    Py_END_CRITICAL_SECTION();
    Py_XDECREF(old_key);
    Py_XDECREF(old_value);
    if (UNLIKELY(dups != NULL)) {
        _md_replace_free_dups(dups);
    }
    MD_WATCH_FLUSH_IF(md, flush);
    md_identity_release(md, key, identity);
    return ret;
}

/***********************************************************************/

static int
_md_traverse_entries(htkeys_t* keys, visitproc visit, void* arg)
{
    if (kind_is_compact(keys->kind)) {
        entry_t* entry = HTKEYS_COMPACT_ENTRIES(keys);
        for (entry_t* end = entry + keys->nentries; entry < end; entry++) {
            if (!entry_is_hole(entry)) {
                Py_VISIT(entry->key);
                Py_VISIT(entry->value);
            }
        }
    } else {
        anystr_entry_t* entry = HTKEYS_ANYSTR_ENTRIES(keys);
        for (anystr_entry_t* end = entry + keys->nentries; entry < end;
             entry++) {
            if (!entry_is_hole((const entry_t*)entry)) {
                Py_VISIT(entry->base.key);
                Py_VISIT(entry->base.value);
            }
        }
    }
    return 0;
}

static int
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
        int ret = _md_traverse_entries(t, visit, arg);
        if (ret != 0) {
            return ret;
        }
    }
#endif

    if (md->used == 0) {
        return 0;
    }

    return _md_traverse_entries(md->keys, visit, arg);
}

/* Out of line: GCC otherwise copies it into dealloc and both clear()
   entry points on GIL builds. */
NOINLINE static int
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
        new_keys = _md_new_keys_after_holes(md, 0, KIND_COMPACT);
        if (UNLIKELY(new_keys == NULL)) {
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
