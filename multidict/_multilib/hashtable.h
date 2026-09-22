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
#include "deferred_decref.h"
#include "dict.h"
#include "finder.h"
#include "htkeys.h"
#include "identity.h"
#include "istr.h"
#include "md_debug.h"
#include "reflist.h"
#include "state.h"
#include "update_marks.h"

typedef struct _md_pos {
    Py_ssize_t pos;
    uint64_t version;
} md_pos_t;

typedef enum _UpdateOp {
    Extend,
    Update,
    Merge,
} UpdateOp;

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
_md_drain_retired() treats it that way: a table whose own num_readers
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
_md_drain_retired(), pairs with that release: observing the
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
(_md_retire()) and pop-alls (_md_drain_retired()) can run concurrently
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

/* Every write to md->keys that a lock-free reader could observe must
   use this, matching _md_reader_enter()'s atomic_load_ptr(): mixing a
   plain store here with an atomic load there is a data race regardless
   of what the surrounding critical section or num_active_readers protocol
   otherwise guarantees, and on architectures weaker than x86 a plain
   store carries no ordering guarantee at all relative to the
   num_active_readers check the retiring code depends on. */
static inline void
_md_store_keys(MultiDictObject* md, htkeys_t* keys)
{
    atomic_store_ptr((void**)&md->keys, keys);
}

/* md_len() reads md->used lock-free via atomic_load_ssize_relaxed(); every
   write to it needs the matching relaxed atomic op for the same reason
   _md_store_keys() exists above. */
static inline void
_md_store_used(MultiDictObject* md, Py_ssize_t used)
{
    atomic_store_ssize_relaxed(&md->used, used);
}

static inline void
_md_add_used(MultiDictObject* md, Py_ssize_t delta)
{
    atomic_fetch_add_ssize_relaxed(&md->used, delta);
}

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
_md_free_retired(htkeys_t* keys)
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
    htkeys_free(keys);
}

static inline void
_md_drain_retired(MultiDictObject* md)
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
            _md_free_retired(t);
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

#if PY_VERSION_HEX >= 0x030e0000
#define _MD_HAVE_TRYINCREF 1
#else
#define _MD_HAVE_TRYINCREF 0
#endif

/*
Lock-free-safe access to individual entry fields.

The table-retirement scheme above only protects an htkeys_t blob's own
memory. It says nothing about a single entry's identity/value fields
*within* a table that is still md->keys, still published, still being
mutated in the ordinary way by add()/__setitem__/pop()/update() -- all
of which run under md's critical section, which does not exclude a
lock-free reader at all.

entry->identity doubles as the "is this slot populated" signal a
lock-free walk checks first (mirroring CPython's own me_key in
compare_unicode_unicode_threadsafe): insertion publishes every other
field an entry needs (hash, key, value) before publishing identity,
last, and deletion clears identity. entry->identity is never replaced
with a *different* non-NULL identity while an entry stays populated
(only key/value change on replace/update), so the only transitions a
reader can race are NULL -> real (insertion) and real -> NULL
(deletion) -- never real -> different-real.

That ordering alone is not enough to safely dereference the identity
or value object's *contents* (_str_cmp, PyUnstable_TryIncRef's own
caller), though: a concurrent delete's Py_CLEAR() is an ordinary
decref with no deferred reclamation, so it can free the object
immediately. Every lock-free read of entry->identity or entry->value
therefore needs PyUnstable_TryIncRef() (safe even if the object is
mid-teardown on another thread; fails cleanly instead of racing it)
followed by re-reading the field to confirm it still holds what was
just incref'd -- if either step fails, the field changed or is
changing under us and the caller must fall back to the critical
section, exactly like CPython's DKIX_KEY_CHANGED retry.
*/

static inline PyObject*
_md_entry_load_identity(entry_t* entry)
{
    return (PyObject*)atomic_load_ptr((void* const*)&entry->identity);
}

static inline void
_md_entry_publish_identity(entry_t* entry, PyObject* identity)
{
#if _MD_HAVE_TRYINCREF
    PyUnstable_EnableTryIncRef(identity);
#endif
    atomic_store_ptr((void**)&entry->identity, identity);
}

static inline void
_md_entry_clear_identity(entry_t* entry)
{
    PyObject* old = _md_entry_load_identity(entry);
    atomic_store_ptr((void**)&entry->identity, NULL);
    Py_XDECREF(old);
}

static inline PyObject*
_md_entry_load_value(entry_t* entry)
{
    return (PyObject*)atomic_load_ptr((void* const*)&entry->value);
}

static inline void
_md_entry_publish_value(entry_t* entry, PyObject* value)
{
#if _MD_HAVE_TRYINCREF
    PyUnstable_EnableTryIncRef(value);
#endif
    atomic_store_ptr((void**)&entry->value, value);
}

static inline void
_md_entry_store_value(entry_t* entry, PyObject* value)
{
    PyObject* old = _md_entry_load_value(entry);
    _md_entry_publish_value(entry, value);
    Py_XDECREF(old);
}

static inline void
_md_entry_clear_value(entry_t* entry)
{
    PyObject* old = _md_entry_load_value(entry);
    atomic_store_ptr((void**)&entry->value, NULL);
    Py_XDECREF(old);
}

/* entry->hash also needs an atomic accessor once a lock-free reader
   compares against it: _md_replace()/_md_update() overwrite it in
   place on an already-populated entry (a plain write racing the
   reader's plain read is still a data race even though Py_hash_t
   isn't a pointer and can't crash on a torn value). Relaxed is enough
   -- it's read only after the identity check above already
   established happens-before for everything else in the entry;
   nothing else depends on this specific field's ordering. */
static inline Py_hash_t
_md_entry_load_hash(entry_t* entry)
{
    return (Py_hash_t)atomic_load_ssize_relaxed((Py_ssize_t*)&entry->hash);
}

static inline void
_md_entry_store_hash(entry_t* entry, Py_hash_t hash)
{
    atomic_store_ssize_relaxed((Py_ssize_t*)&entry->hash, (Py_ssize_t)hash);
}

#if _MD_HAVE_TRYINCREF
/* Tries to safely grab a strong reference to *addr's current value for
   a lock-free reader: PyUnstable_TryIncRef() (fails cleanly if the
   object is concurrently being torn down) followed by re-reading
   *addr to confirm it is still what was just incref'd. Returns NULL
   (with no reference held) if either step fails, meaning the caller
   must fall back to the critical section; the field may be NULL
   legitimately (not populated / deleted), which is reported the same
   way, since either way the caller cannot proceed lock-free. Only
   defined where PyUnstable_TryIncRef() exists at all (see the
   _MD_HAVE_TRYINCREF comment above); callers must be equally
   guarded. */
static inline PyObject*
_md_entry_try_get_ref(PyObject** addr)
{
    PyObject* value = (PyObject*)atomic_load_ptr((void* const*)addr);
    if (value == NULL) {
        return NULL;
    }
    if (!PyUnstable_TryIncRef(value)) {
        return NULL;
    }
    if ((PyObject*)atomic_load_ptr((void* const*)addr) != value) {
        Py_DECREF(value);
        return NULL;
    }
    return value;
}
#endif /* _MD_HAVE_TRYINCREF */

#endif /* Py_GIL_DISABLED */

static inline int
_md_resize(MultiDictObject* md, uint8_t log2_newsize, update_marks_t* marks)
{
    if (log2_newsize >= SIZEOF_SIZE_T * 8) {
        PyErr_NoMemory();
        return -1;
    }
    assert(log2_newsize >= HT_LOG_MINSIZE);

    htkeys_t* newkeys = htkeys_new(log2_newsize);
    if (newkeys == NULL) {
        return -1;
    }

    htkeys_t* oldkeys = md->keys;
    if (_update_marks_remap(marks, oldkeys, newkeys, newkeys->usable) < 0) {
        htkeys_free(newkeys);
        return -1;
    }
    Py_ssize_t numentries = md->used;
    entry_t* oldentries = htkeys_entries(oldkeys);
    entry_t* newentries = htkeys_entries(newkeys);
    if (oldkeys->nentries == numentries) {
        memcpy(newentries, oldentries, numentries * sizeof(entry_t));
    } else {
        entry_t* new_ep = newentries;
        entry_t* old_ep = oldentries;
        Py_ssize_t oldnumentries = oldkeys->nentries;
        for (Py_ssize_t i = 0; i < oldnumentries; ++i, ++old_ep) {
            if (old_ep->identity != NULL) {
                *new_ep++ = *old_ep;
            }
        }
    }

    if (htkeys_build_indices(newkeys, newentries, numentries) < 0) {
        return -1;
    }

    newkeys->usable = newkeys->usable - numentries;
    newkeys->nentries = numentries;

#ifdef Py_GIL_DISABLED
    _md_store_keys(md, newkeys);
#else
    md->keys = newkeys;
#endif

#ifdef Py_GIL_DISABLED
    /* Bump the version on every resize, not just when a caller's
       own insert/delete/replace would bump it anyway: a freed
       htkeys_t can get reallocated at the very same address by a
       later resize (same size class, common in practice), so code
       elsewhere that detects "did md->keys change under me" by
       comparing the raw pointer alone (see _md_replace()'s and
       _md_update()'s comments) needs a companion signal that can't
       coincidentally repeat. */
    atomic_store_uint64_relaxed(&md->version, NEXT_VERSION(md->state));

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
        htkeys_free(oldkeys);
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
    if (htkeys_build_indices(keys, entries, newnentries) < 0) {
        return -1;
    }
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

        new_keys = htkeys_new(log2_newsize);
        if (new_keys == NULL) return -1;
    }

    md_clear(md);
    md->is_ci = is_ci;
#ifdef Py_GIL_DISABLED
    _md_store_used(md, 0);
#else
    md->used = 0;
#endif
    atomic_store_uint64_relaxed(&md->version, NEXT_VERSION(md->state));
#ifdef Py_GIL_DISABLED
    _md_store_keys(md, new_keys);
#else
    md->keys = new_keys;
#endif
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
        size_t size = htkeys_sizeof(src);
        keys = PyMem_Malloc(size);
        if (keys == NULL) {
            PyErr_NoMemory();
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
#ifdef Py_GIL_DISABLED
    _md_store_used(md, used);
#else
    md->used = used;
#endif
    atomic_store_uint64_relaxed(
        &md->version, NEXT_VERSION(md->state));  // never reuse other's version
    md->is_ci = is_ci;
#ifdef Py_GIL_DISABLED
    _md_store_keys(md, keys);
#else
    md->keys = keys;
#endif
    ASSERT_CONSISTENT(md, false);
    return 0;
}

static inline Py_ssize_t
md_len(MultiDictObject* md)
{
    return atomic_load_ssize_relaxed(&md->used);
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

#ifdef Py_GIL_DISABLED
    /* identity is published last: it's the field a lock-free reader
       checks first (before ever touching hash/key/value), treating
       NULL as "not populated yet, keep probing". See the comment
       above _md_entry_load_identity(). */
    entry->key = key;
    _md_entry_store_hash(entry, hash);
    _md_entry_store_value(entry, value);
    _md_entry_publish_identity(entry, identity);
#else
    entry->identity = identity;
    entry->key = key;
    entry->value = value;
    entry->hash = hash;
#endif

    atomic_store_uint64_relaxed(&md->version, NEXT_VERSION(md->state));
#ifdef Py_GIL_DISABLED
    _md_add_used(md, 1);
#else
    md->used += 1;
#endif
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

#ifdef Py_GIL_DISABLED
    entry->key = key;
    _md_entry_store_hash(entry, hash);
    _md_entry_store_value(entry, value);
    _md_entry_publish_identity(entry, identity);
#else
    entry->identity = identity;
    entry->key = key;
    entry->value = value;
    entry->hash = hash;
#endif

    atomic_store_uint64_relaxed(&md->version, NEXT_VERSION(md->state));
#ifdef Py_GIL_DISABLED
    _md_add_used(md, 1);
#else
    md->used += 1;
#endif
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

static inline int
md_add(MultiDictObject* md, PyObject* key, PyObject* value)
{
    PyObject* identity = md_calc_identity(md, key);
    if (identity == NULL) {
        goto fail;
    }
    Py_hash_t hash = _unicode_hash(identity);
    if (hash == -1) {
        goto fail;
    }
    int ret = _md_add_with_hash(md, hash, identity, key, value);
    ASSERT_CONSISTENT(md, false);
    Py_DECREF(identity);
    return ret;
fail:
    Py_XDECREF(identity);
    return -1;
}

static inline void
_md_del_at(MultiDictObject* md, size_t slot, entry_t* entry)
{
    htkeys_t* keys = md->keys;
    assert(keys != &empty_htkeys);
#ifdef Py_GIL_DISABLED
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
       view. entry->key is read/written as a plain pointer: unlike
       identity/value, no lock-free reader ever touches it (see the
       comment above _md_entry_load_identity()). */
    PyObject* identity = _md_entry_load_identity(entry);
    PyObject* key = entry->key;
    PyObject* value = _md_entry_load_value(entry);

    atomic_store_ptr((void**)&entry->identity, NULL);
    entry->key = NULL;
    atomic_store_ptr((void**)&entry->value, NULL);
    htkeys_set_index(keys, slot, DKIX_DUMMY);
    _md_add_used(md, -1);

    Py_XDECREF(identity);
    Py_XDECREF(key);
    Py_XDECREF(value);
#else
    /* GIL-build mirror of the branch above: decref after bookkeeping so a
     * __del__-triggered GIL release (Py_BEGIN_CRITICAL_SECTION is a no-op
     * here) never exposes a half-deleted entry -- see #1489. */
    PyObject* identity = entry->identity;
    PyObject* key = entry->key;
    PyObject* value = entry->value;

    entry->identity = NULL;
    entry->key = NULL;
    entry->value = NULL;
    htkeys_set_index(keys, slot, DKIX_DUMMY);
    md->used -= 1;

    Py_XDECREF(identity);
    Py_XDECREF(key);
    Py_XDECREF(value);
#endif
}

/* _md_del_at() variant that defers the decref (see deferred_decref_t);
 * used by _md_replace()'s duplicate-cleanup path on both builds. */
static inline int
_md_del_at_deferred(MultiDictObject* md, size_t slot, entry_t* entry,
                    deferred_decref_t* defer)
{
    htkeys_t* keys = md->keys;
    assert(keys != &empty_htkeys);
#ifdef Py_GIL_DISABLED
    PyObject* identity = _md_entry_load_identity(entry);
    PyObject* key = entry->key;
    PyObject* value = _md_entry_load_value(entry);

    atomic_store_ptr((void**)&entry->identity, NULL);
    entry->key = NULL;
    atomic_store_ptr((void**)&entry->value, NULL);
    htkeys_set_index(keys, slot, DKIX_DUMMY);
    _md_add_used(md, -1);
#else
    PyObject* identity = entry->identity;
    PyObject* key = entry->key;
    PyObject* value = entry->value;

    entry->identity = NULL;
    entry->key = NULL;
    entry->value = NULL;
    htkeys_set_index(keys, slot, DKIX_DUMMY);
    md->used -= 1;
#endif

    int ret = deferred_decref_push(defer, identity);
    if (deferred_decref_push(defer, key) < 0) {
        ret = -1;
    }
    if (deferred_decref_push(defer, value) < 0) {
        ret = -1;
    }
    return ret;
}

/* Deferred half-deletion: entry may be replaced later or finished off by
 * md_post_update() (identity=NULL, used -= 1, slot -> DKIX_DUMMY). Unlike
 * _md_del_at_deferred(), this leaves identity/hash/index live -- a reader's
 * hash-chain scan can still reach this slot -- so each field is reserved
 * and pushed before it's nulled, one at a time: deferred_decref_push()'s
 * OOM fallback would otherwise decref a field's old value immediately
 * while the entry sits in that half-deleted, still-reachable state. */
static inline int
_md_del_at_for_upd_deferred(MultiDictObject* md, size_t slot, entry_t* entry,
                            deferred_decref_t* defer)
{
    (void)md;
    (void)slot;
    assert(md->keys != &empty_htkeys);
    if (_deferred_decref_reserve_one(defer) < 0) {
        return -1;
    }
    PyObject* old_key = entry->key;
    entry->key = NULL;
    deferred_decref_push_reserved(defer, old_key);

    if (_deferred_decref_reserve_one(defer) < 0) {
        return -1;
    }
#ifdef Py_GIL_DISABLED
    PyObject* old_value = _md_entry_load_value(entry);
    atomic_store_ptr((void**)&entry->value, NULL);
#else
    PyObject* old_value = entry->value;
    entry->value = NULL;
#endif
    deferred_decref_push_reserved(defer, old_value);
    return 0;
}

static inline int
md_del(MultiDictObject* md, PyObject* key)
{
    PyObject* identity = md_calc_identity(md, key);
    if (identity == NULL) {
        goto fail;
    }

    Py_hash_t hash = _unicode_hash(identity);
    if (hash == -1) {
        goto fail;
    }

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

    if (!found) {
        PyErr_SetObject(PyExc_KeyError, key);
        goto fail;
    } else {
        atomic_store_uint64_relaxed(&md->version, NEXT_VERSION(md->state));
    }
    Py_DECREF(identity);
    ASSERT_CONSISTENT(md, false);
    return 0;
fail:
    Py_XDECREF(identity);
    return -1;
}

static inline uint64_t
md_version(MultiDictObject* md)
{
    return atomic_load_uint64_relaxed(&md->version);
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
        if (iter.index < 0) {
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

#if defined(Py_GIL_DISABLED) && _MD_HAVE_TRYINCREF

static inline int
_md_contains_lockfree(MultiDictObject* md, PyObject* identity, Py_hash_t hash)
{
    htkeys_t* keys = _md_reader_enter(md);
    htkeysiter_t iter;
    htkeysiter_init(&iter, keys, hash);
    entry_t* entries = htkeys_entries(keys);

    int result = 0;
    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t* entry = entries + iter.index;

        PyObject* entry_identity = _md_entry_try_get_ref(&entry->identity);
        if (entry_identity == NULL) {
            if (_md_entry_load_identity(entry) == NULL) {
                continue;  // not populated (or deleted); keep probing
            }
            result = 2;  // _MD_NEED_LOCK
            break;
        }

        if (_md_entry_load_hash(entry) != hash) {
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

#endif /* Py_GIL_DISABLED && _MD_HAVE_TRYINCREF */

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
#if defined(Py_GIL_DISABLED) && _MD_HAVE_TRYINCREF
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
#elif defined(Py_GIL_DISABLED)
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
        if (iter.index < 0) {
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

#if defined(Py_GIL_DISABLED) && _MD_HAVE_TRYINCREF

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
        if (iter.index < 0) {
            continue;
        }
        entry_t* entry = entries + iter.index;

        PyObject* entry_identity = _md_entry_try_get_ref(&entry->identity);
        if (entry_identity == NULL) {
            if (_md_entry_load_identity(entry) == NULL) {
                continue;  // not populated (or deleted); keep probing
            }
            result = _MD_NEED_LOCK;  // racing a concurrent change
            break;
        }

        if (_md_entry_load_hash(entry) != hash) {
            Py_DECREF(entry_identity);
            continue;
        }

        bool matched = _str_cmp(identity, entry_identity);
        Py_DECREF(entry_identity);
        if (!matched) {
            continue;
        }

        PyObject* value = _md_entry_try_get_ref(&entry->value);
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

#elif defined(Py_GIL_DISABLED)

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
    int result;
    Py_BEGIN_CRITICAL_SECTION(md);
    result = _md_get_one_locked(md, identity, hash, ret);
    Py_END_CRITICAL_SECTION();
    Py_DECREF(identity);
    return result;
}

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

static inline int
md_set_default(MultiDictObject* md, PyObject* key, PyObject* value,
               PyObject** result)
{
    *result = NULL;
    PyObject* identity = md_calc_identity(md, key);
    if (identity == NULL) {
        goto fail;
    }

    Py_hash_t hash = _unicode_hash(identity);
    if (hash == -1) {
        goto fail;
    }

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
            Py_DECREF(identity);
            ASSERT_CONSISTENT(md, false);
            *result = Py_NewRef(entry->value);
            return 1;
        }
    }

    if (_md_add_with_hash(md, hash, identity, key, value) < 0) {
        goto fail;
    }

    Py_DECREF(identity);
    ASSERT_CONSISTENT(md, false);
    *result = Py_NewRef(value);
    return 0;
fail:
    Py_XDECREF(identity);
    return -1;
}

static inline int
md_pop_one(MultiDictObject* md, PyObject* key, PyObject** ret)
{
    PyObject* value = NULL;

    PyObject* identity = md_calc_identity(md, key);
    if (identity == NULL) {
        goto fail;
    }

    Py_hash_t hash = _unicode_hash(identity);
    if (hash == -1) {
        goto fail;
    }

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
            value = Py_NewRef(entry->value);
            _md_del_at(md, iter.slot, entry);
            Py_DECREF(identity);
            *ret = value;
            atomic_store_uint64_relaxed(&md->version, NEXT_VERSION(md->state));
            ASSERT_CONSISTENT(md, false);
            return 1;
        }
    }
    Py_DECREF(identity);
    ASSERT_CONSISTENT(md, false);
    return 0;
fail:
    Py_XDECREF(value);
    Py_XDECREF(identity);
    return -1;
}

// Caller holds md's critical section
static inline int
_md_pop_all_locked(MultiDictObject* md, PyObject* key, reflist_t* values)
{
    PyObject* identity = md_calc_identity(md, key);
    if (identity == NULL) {
        return -1;
    }

    Py_hash_t hash = _unicode_hash(identity);
    if (hash == -1) {
        goto fail;
    }

    if (md_len(md) == 0) {
        Py_DECREF(identity);
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
                goto fail;
            }
            uint64_t version = NEXT_VERSION(md->state);
            atomic_store_uint64_relaxed(&md->version, version);
            _md_del_at(md, iter.slot, entry);
            // the decref can run a __del__ that lets another thread resize
            if (UNLIKELY(md->keys != keys || md->version != version)) {
                goto restart;
            }
        }
    }

    Py_DECREF(identity);
    ASSERT_CONSISTENT(md, false);
    return 0;
fail:
    Py_DECREF(identity);
    return -1;
}

static inline int
md_pop_all(MultiDictObject* md, PyObject* key, PyObject** ret)
{
    reflist_t values;
    reflist_init(&values);
    int tmp;
    Py_BEGIN_CRITICAL_SECTION(md);
    tmp = _md_pop_all_locked(md, key, &values);
    Py_END_CRITICAL_SECTION();
    if (tmp < 0) {
        reflist_clear(&values);
        return -1;
    }
    if (values.size == 0) {
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
    atomic_store_uint64_relaxed(&md->version, NEXT_VERSION(md->state));
    ASSERT_CONSISTENT(md, false);
    return ret;
}

static inline int
_md_replace(MultiDictObject* md, PyObject* key, PyObject* value,
            PyObject* identity, Py_hash_t hash, deferred_decref_t* defer)
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
                /* old_key/old_value decref deferred -- see
                 * deferred_decref_t */
                PyObject* old_key = entry->key;
#ifdef Py_GIL_DISABLED
                PyObject* old_value = _md_entry_load_value(entry);
                entry->key = Py_NewRef(key);
                _md_entry_publish_value(entry, Py_NewRef(value));
#else
                PyObject* old_value = entry->value;
                entry->key = Py_NewRef(key);
                entry->value = Py_NewRef(value);
#endif
                /* Push both unconditionally, not with `||`: a failed
                   first push already decref'd old_key itself (see
                   deferred_decref_push()'s doc comment), but
                   short-circuiting past the second push would leak
                   old_value -- neither deferred nor decref'd. */
                int push_ret = deferred_decref_push(defer, old_key);
                if (deferred_decref_push(defer, old_value) < 0) {
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
        atomic_store_uint64_relaxed(&md->version, NEXT_VERSION(md->state));
        return 0;
    }
}

static inline int
md_replace(MultiDictObject* md, PyObject* key, PyObject* value,
           deferred_decref_t* defer)
{
    PyObject* identity = md_calc_identity(md, key);
    if (identity == NULL) {
        goto fail;
    }

    Py_hash_t hash = _unicode_hash(identity);
    if (hash == -1) {
        goto fail;
    }

    int ret = _md_replace(md, key, value, identity, hash, defer);
    /* identity decref deferred too; `defer` owned/released by
     * multidict_mp_as_subscript() */
    if (deferred_decref_push(defer, identity) < 0) {
        ret = -1;
    }
    ASSERT_CONSISTENT(md, false);
    return ret;
fail:
    Py_XDECREF(identity);
    return -1;
}

static inline int
_md_update(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
           PyObject* key, PyObject* value, deferred_decref_t* defer,
           update_marks_t* marks)
{
    bool found = false;
    _update_marks_sync(marks, md);

    // See _md_replace() on the retry/deferred-decref shape used here.
    for (;;) {
        htkeysiter_t iter;
        htkeysiter_init(&iter, md->keys, hash);
        /* A retry may have lost the mark on the entry this call already
           wrote; equal keys sit on their chain in insertion order, so it
           is the first unmarked match. */
        bool skip_first = found;
        bool stale = false;

        for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
            if (iter.index < 0) {
                continue;
            }
#ifdef Py_GIL_DISABLED
            htkeys_t* keys_before = md->keys;
            uint64_t version_before = md->version;
#endif
            entry_t* entries = htkeys_entries(md->keys);
            entry_t* entry = entries + iter.index;
            if (hash != entry->hash ||
                bitmap_test(&marks->updated, iter.index) ||
                !_str_cmp(identity, entry->identity)) {
                continue;
            }
            if (skip_first) {
                skip_first = false;
                if (bitmap_set(&marks->updated, iter.index) < 0) {
                    goto fail;
                }
                continue;
            }
            if (!found) {
                found = true;
                /* Marked first: nothing below can fail half-way after
                   the entry has changed. */
                if (bitmap_set(&marks->updated, iter.index) < 0) {
                    goto fail;
                }
                if (entry->key == NULL) {
                    /* Half-deleted by an earlier item of this batch; reusing
                       it keeps the key at its original position. */
                    assert(entry->value == NULL);
                    bitmap_clear(&marks->deleted, iter.index);
                    entry->key = Py_NewRef(key);
#ifdef Py_GIL_DISABLED
                    _md_entry_publish_value(entry, Py_NewRef(value));
#else
                    entry->value = Py_NewRef(value);
#endif
                } else {
                    /* old_key/old_value decref deferred -- see
                     * deferred_decref_t */
                    PyObject* old_key = entry->key;
#ifdef Py_GIL_DISABLED
                    PyObject* old_value = _md_entry_load_value(entry);
                    entry->key = Py_NewRef(key);
                    _md_entry_publish_value(entry, Py_NewRef(value));
#else
                    PyObject* old_value = entry->value;
                    entry->key = Py_NewRef(key);
                    entry->value = Py_NewRef(value);
#endif
                    /* Push both unconditionally, not with `||`: a
                       failed first push already decref'd old_key
                       itself (see deferred_decref_push()'s doc
                       comment), but short-circuiting past the
                       second push would leak old_value -- neither
                       deferred nor decref'd. */
                    int push_ret = deferred_decref_push(defer, old_key);
                    if (deferred_decref_push(defer, old_value) < 0) {
                        push_ret = -1;
                    }
                    if (push_ret < 0) {
                        goto fail;
                    }
                }
            } else {
                if (bitmap_test(&marks->deleted, iter.index)) {
                    continue;
                }
                if (bitmap_set(&marks->deleted, iter.index) < 0) {
                    goto fail;
                }
                if (_md_del_at_for_upd_deferred(md, iter.slot, entry, defer) <
                    0) {
                    goto fail;
                }
            }
#ifdef Py_GIL_DISABLED
            /* See _md_replace()'s comment on why both the pointer and
               the version are checked. */
            if (md->keys != keys_before || md->version != version_before) {
                stale = true;
                break;
            }
#endif
        }
        if (stale) {
            /* Whatever moved the table also invalidated the marks. */
            _update_marks_sync(marks, md);
            continue;
        }
        break;
    }

    if (!found) {
        if (_md_add_for_upd(md, hash, identity, key, value, marks) < 0) {
            goto fail;
        }
    }
    marks->version = md->version;
    return 0;
fail:
    marks->version = md->version;
    return -1;
}

static inline int
_md_merge(MultiDictObject* md, Py_hash_t hash, PyObject* identity,
          PyObject* key, PyObject* value, update_marks_t* marks)
{
    _update_marks_sync(marks, md);
    htkeysiter_t iter;
    htkeysiter_init(&iter, md->keys, hash);
    entry_t* entries = htkeys_entries(md->keys);

    for (; iter.index != DKIX_EMPTY; htkeysiter_next(&iter)) {
        if (iter.index < 0) {
            continue;
        }
        entry_t* entry = entries + iter.index;
        /* An entry this batch added doesn't count as already present. */
        if (hash != entry->hash || bitmap_test(&marks->updated, iter.index)) {
            continue;
        }
        if (_str_cmp(identity, entry->identity)) {
            return 0;
        }
    }

    int ret = _md_add_for_upd(md, hash, identity, key, value, marks);
    marks->version = md->version;
    return ret;
}

/* Finishes off one half-deleted entry: `slot` must index it. */
static inline int
_md_post_update_del(MultiDictObject* md, htkeys_t* keys, size_t slot,
                    entry_t* entry, deferred_decref_t* defer)
{
    assert(entry->key == NULL);
#ifdef Py_GIL_DISABLED
    PyObject* old_identity = _md_entry_load_identity(entry);
    atomic_store_ptr((void**)&entry->identity, NULL);
    htkeys_set_index(keys, slot, DKIX_DUMMY);
    _md_add_used(md, -1);
#else
    PyObject* old_identity = entry->identity;
    entry->identity = NULL;
    htkeys_set_index(keys, slot, DKIX_DUMMY);
    md->used -= 1;
#endif
    return deferred_decref_push(defer, old_identity);
}

/* The fallback when the `deleted` marks can't be trusted: every half-deleted
   entry still has a NULL key, so a full sweep finds them all. */
COLD static int
_md_post_update_sweep(MultiDictObject* md, deferred_decref_t* defer)
{
    int ret = 0;
    for (;;) {
        htkeys_t* keys = md->keys;
#ifdef Py_GIL_DISABLED
        uint64_t version_before = md->version;
#endif
        size_t num_slots = htkeys_nslots(keys);
        entry_t* entries = htkeys_entries(keys);
        bool stale = false;
        for (size_t slot = 0; slot < num_slots; slot++) {
            Py_ssize_t index = htkeys_get_index(keys, slot);
            if (index >= 0 && entries[index].key == NULL) {
                if (_md_post_update_del(
                        md, keys, slot, entries + index, defer) < 0) {
                    ret = -1;
                }
#ifdef Py_GIL_DISABLED
                if (md->keys != keys || md->version != version_before) {
                    stale = true;
                    break;
                }
#endif
            }
        }
        if (!stale) {
            return ret;
        }
    }
}

static inline int
_md_post_update_deleted(MultiDictObject* md, deferred_decref_t* defer,
                        update_marks_t* marks)
{
    _update_marks_sync(marks, md);
    if (marks->lost) {
        return _md_post_update_sweep(md, defer);
    }
    int ret = 0;
    htkeys_t* keys = md->keys;
#ifdef Py_GIL_DISABLED
    uint64_t version_before = md->version;
#endif
    entry_t* entries = htkeys_entries(keys);
    for (Py_ssize_t pos = bitmap_next(&marks->deleted, 0); pos >= 0;
         pos = bitmap_next(&marks->deleted, pos + 1)) {
        entry_t* entry = entries + pos;
        /* Another thread's update(), run while this one's critical section
           was suspended, can revive or finish off one of these in place
           without moving anything. */
        if (entry->identity == NULL || entry->key != NULL) {
            continue;
        }
        htkeysiter_t iter;
        htkeysiter_init(&iter, keys, entry->hash);
        while (iter.index != pos && iter.index != DKIX_EMPTY) {
            htkeysiter_next(&iter);
        }
        assert(iter.index == pos);
        if (iter.index != pos) {
            continue;
        }
        if (_md_post_update_del(md, keys, iter.slot, entry, defer) < 0) {
            ret = -1;
        }
#ifdef Py_GIL_DISABLED
        /* Only an out-of-memory fallback decref above can run Python. */
        if (md->keys != keys || md->version != version_before) {
            if (_md_post_update_sweep(md, defer) < 0) {
                ret = -1;
            }
            break;
        }
#endif
    }
    return ret;
}

static inline int
md_post_update(MultiDictObject* md, deferred_decref_t* defer,
               update_marks_t* marks)
{
    int ret = 0;
    /* `defer` is NULL only for merge(), which never half-deletes. */
    if (defer != NULL) {
        ret = _md_post_update_deleted(md, defer, marks);
    }
    atomic_store_uint64_relaxed(&md->version, NEXT_VERSION(md->state));
    ASSERT_CONSISTENT(md, false);
    return ret;
}

static inline int
md_update_from_ht(MultiDictObject* md, MultiDictObject* other, UpdateOp op,
                  deferred_decref_t* defer, update_marks_t* marks)
{
    Py_ssize_t pos;
    Py_hash_t hash;
    PyObject* identity = NULL;
    PyObject* key = NULL;
    bool recalc_identity = md->is_ci != other->is_ci;

    if (other->used == 0) {
        return 0;
    }

    if (md == other && op != Extend) {
        /* update(self) and merge(self) leave the dict unchanged: every key
           already maps to its own values.  Short-circuit -- doing the work in
           place would soft-delete and reinsert the very entries we iterate. */
        return 0;
    }

    /* Pre-allocate room for other's items so the inserts below cannot trigger
       a resize of md->keys.  This is what makes extend(self) (md IS other,
       e.g. ``d.extend(d)``) safe: a resize would free the very entries array
       we iterate here, a use-after-free.  Reserving up front also lets us
       snapshot the entry count so self-extension does not reprocess the
       entries it just appended. */
    if (_md_reserve(md, other->used, marks) < 0) {
        return -1;
    }

    entry_t* entries = htkeys_entries(other->keys);
    Py_ssize_t nentries = other->keys->nentries;

    for (pos = 0; pos < nentries; pos++) {
        entry_t* entry = entries + pos;
        if (entry->identity == NULL) {
            continue;
        }
        if (recalc_identity) {
            identity = md_calc_identity(md, entry->key);
            if (identity == NULL) {
                goto fail;
            }
            hash = _unicode_hash(identity);
            if (hash == -1) {
                goto fail;
            }
            /* materialize key */
            key = _md_calc_key(other, entry->key, identity);
            if (key == NULL) {
                goto fail;
            }
        } else {
            identity = entry->identity;
            hash = entry->hash;
            key = entry->key;
        }
        switch (op) {
            case Update:
                if (_md_update(
                        md, hash, identity, key, entry->value, defer, marks) <
                    0) {
                    goto fail;
                }
                break;
            case Extend:
                if (_md_add_with_hash(md, hash, identity, key, entry->value) <
                    0) {
                    goto fail;
                }
                break;
            case Merge:
                if (_md_merge(md, hash, identity, key, entry->value, marks) <
                    0) {
                    goto fail;
                }
                break;
        }
        if (recalc_identity) {
            Py_CLEAR(identity);
            Py_CLEAR(key);
        }
    }
    return 0;
fail:
    if (recalc_identity) {
        Py_CLEAR(identity);
        Py_CLEAR(key);
    }
    return -1;
}

static inline int
md_extend_self(MultiDictObject* md)
{
    if (md_reserve(md, md->keys->nentries) < 0) {
        return -1;
    }

    Py_ssize_t nentries = md->keys->nentries;
    entry_t* entries = htkeys_entries(md->keys);
    for (Py_ssize_t pos = 0; pos < nentries; pos++) {
        entry_t* entry = entries + pos;
        if (entry->identity != NULL) {
            if (_md_add_with_hash(md,
                                  entry->hash,
                                  entry->identity,
                                  entry->key,
                                  entry->value) < 0) {
                return -1;
            }
        }
    }
    return 0;
}

static inline int
md_update_from_dict(MultiDictObject* md, PyObject* kwds, UpdateOp op,
                    deferred_decref_t* defer, update_marks_t* marks)
{
    Py_ssize_t pos = 0;
    PyObject* identity = NULL;
    PyObject* key = NULL;
    PyObject* value = NULL;

    assert(PyDict_CheckExact(kwds));

    // PyDict_Next returns borrowed refs
    while (PyDict_Next(kwds, &pos, &key, &value)) {
        Py_INCREF(key);
        identity = md_calc_identity(md, key);
        if (identity == NULL) {
            goto fail;
        }
        Py_hash_t hash = _unicode_hash(identity);
        if (hash == -1) {
            goto fail;
        }
        switch (op) {
            case Update: {
                if (_md_update(md, hash, identity, key, value, defer, marks) <
                    0) {
                    goto fail;
                }
                Py_CLEAR(identity);
                Py_CLEAR(key);
                break;
            }
            case Extend: {
                int tmp = _md_add_with_hash_steal_refs(
                    md, hash, identity, key, Py_NewRef(value));
                if (tmp < 0) {
                    Py_DECREF(value);
                    goto fail;
                }

                identity = NULL;
                key = NULL;
                value = NULL;
                break;
            }
            case Merge: {
                if (_md_merge(md, hash, identity, key, value, marks) < 0) {
                    goto fail;
                }
                Py_CLEAR(identity);
                Py_CLEAR(key);
                break;
            }
        }
    }
    return 0;
fail:
    Py_CLEAR(identity);
    Py_CLEAR(key);
    return -1;
}

static inline int
md_update_from_kwnames(MultiDictObject* md, PyObject* const* args,
                       Py_ssize_t nargs, PyObject* kwnames)
{
    Py_ssize_t nkwargs = PyTuple_GET_SIZE(kwnames);
    if (md_reserve(md, nkwargs) < 0) {
        return -1;
    }
    for (Py_ssize_t i = 0; i < nkwargs; i++) {
        PyObject* key = PyTuple_GET_ITEM(kwnames, i);  // borrowed
        assert(PyUnicode_Check(key));
        Py_INCREF(key);
        PyObject* identity = md_calc_identity(md, key);
        if (identity == NULL) {
            Py_DECREF(key);
            return -1;
        }
        Py_hash_t hash = _unicode_hash(identity);
        if (hash == -1) {
            Py_DECREF(identity);
            Py_DECREF(key);
            return -1;
        }
        PyObject* value = args[nargs + i];  // borrowed
        if (_md_add_with_hash_steal_refs(
                md, hash, identity, key, Py_NewRef(value)) < 0) {
            Py_DECREF(value);
            Py_DECREF(identity);
            Py_DECREF(key);
            return -1;
        }
    }
    return 0;
}

static inline void
_err_not_sequence(Py_ssize_t i)
{
    PyErr_Format(PyExc_TypeError,
                 "multidict cannot convert sequence element #%zd"
                 " to a sequence",
                 i);
}

static inline void
_err_bad_length(Py_ssize_t i, Py_ssize_t n)
{
    PyErr_Format(PyExc_ValueError,
                 "multidict update sequence element #%zd "
                 "has length %zd; 2 is required",
                 i,
                 n);
}

static inline void
_err_cannot_fetch(Py_ssize_t i, const char* name)
{
    PyErr_Format(PyExc_ValueError,
                 "multidict update sequence element #%zd's "
                 "%s could not be fetched",
                 i,
                 name);
}

/* list[i] as a new reference. On a free-threaded build another thread can
   drop the item between a borrow and its incref, or shrink the list after
   its length was checked, so PyList_GetItemRef takes the reference
   atomically (locking the list only if its lock-free attempt fails). An
   item that is gone by then means the list changed under the caller, and
   is reported as a RuntimeError: the caller sees NULL with the error set.
   GIL builds keep the macro and compile the check away: nothing can run
   between the length check and the borrow. */
#ifdef Py_GIL_DISABLED
static inline PyObject*
_list_getitem_ref(PyObject* list, Py_ssize_t i)
{
    PyObject* item = PyList_GetItemRef(list, i);
    if (item == NULL && PyErr_ExceptionMatches(PyExc_IndexError)) {
        PyErr_Clear();
        PyErr_SetString(PyExc_RuntimeError,
                        "list changed size during iteration");
    }
    return item;
}
#define _list_item_gone(item) ((item) == NULL)
#else
#define _list_getitem_ref(list, i) Py_NewRef(PyList_GET_ITEM((list), (i)))
#define _list_item_gone(item) (0)
#endif

static int
_md_parse_item(Py_ssize_t i, PyObject* item, PyObject** pkey,
               PyObject** pvalue)
{
    Py_ssize_t n;

    if (PyTuple_CheckExact(item)) {
        n = PyTuple_GET_SIZE(item);
        if (n != 2) {
            _err_bad_length(i, n);
            goto fail;
        }
        *pkey = Py_NewRef(PyTuple_GET_ITEM(item, 0));
        *pvalue = Py_NewRef(PyTuple_GET_ITEM(item, 1));
    } else if (PyList_CheckExact(item)) {
        n = PyList_GET_SIZE(item);
        if (n != 2) {
            _err_bad_length(i, n);
            goto fail;
        }
        *pkey = _list_getitem_ref(item, 0);
        if (_list_item_gone(*pkey)) {
            goto fail;
        }
        *pvalue = _list_getitem_ref(item, 1);
        if (_list_item_gone(*pvalue)) {
            goto fail;
        }
    } else {
        if (!PySequence_Check(item)) {
            _err_not_sequence(i);
            goto fail;
        }
        n = PySequence_Size(item);
        if (n != 2) {
            _err_bad_length(i, n);
            goto fail;
        }
        *pkey = PySequence_ITEM(item, 0);
        if (*pkey == NULL) {
            _err_cannot_fetch(i, "key");
            goto fail;
        }
        *pvalue = PySequence_ITEM(item, 1);
        if (*pvalue == NULL) {
            _err_cannot_fetch(i, "value");
            goto fail;
        }
    }
    return 0;
fail:
    Py_CLEAR(*pkey);
    Py_CLEAR(*pvalue);
    return -1;
}

static inline int
md_update_from_seq(MultiDictObject* md, PyObject* seq, UpdateOp op,
                   deferred_decref_t* defer, update_marks_t* marks)
{
    PyObject* it = NULL;
    PyObject* item = NULL;  // seq[i]

    PyObject* key = NULL;
    PyObject* value = NULL;
    PyObject* identity = NULL;
    PyObject* items = NULL;

    Py_ssize_t i;
    Py_ssize_t size = -1;

    enum { LIST, TUPLE, ITER } kind;

    if (!PyList_CheckExact(seq) && !PyTuple_CheckExact(seq)) {
        items = PyMapping_Items(seq);
        if (items != NULL) {
            seq = items;
        } else {
            if (!PyErr_ExceptionMatches(PyExc_AttributeError) &&
                !PyErr_ExceptionMatches(PyExc_TypeError)) {
                // propagate MemoryError / KeyboardInterrupt / etc.
                goto fail;
            }
            // seq is not a mapping; fall back to treating it as a sequence
            PyErr_Clear();
        }
    }

    if (PyList_CheckExact(seq)) {
        kind = LIST;
        size = PyList_GET_SIZE(seq);
        if (size == 0) {
            goto exit;
        }
    } else if (PyTuple_CheckExact(seq)) {
        kind = TUPLE;
        size = PyTuple_GET_SIZE(seq);
        if (size == 0) {
            goto exit;
        }
    } else {
        kind = ITER;
        it = PyObject_GetIter(seq);
        if (it == NULL) {
            goto fail;
        }
    }

    for (i = 0;; ++i) {  // i - index into seq of current element
        switch (kind) {
            case LIST:
                /* Re-read the length every iteration.  Building the identity
                   below can run arbitrary Python (a str-subclass key's
                   .lower(), an __eq__), which may shrink seq; a stale cached
                   size would let PyList_GET_ITEM read past the end. */
                if (i >= PyList_GET_SIZE(seq)) {
                    goto exit;
                }
                item = _list_getitem_ref(seq, i);
                if (_list_item_gone(item)) {
                    goto fail;
                }
                break;
            case TUPLE:
                if (i >= size) {
                    goto exit;
                }
                item = PyTuple_GET_ITEM(seq, i);
                if (item == NULL) {
                    goto fail;
                }
                Py_INCREF(item);
                break;
            case ITER: {
                int res = PyIter_NextItem(it, &item);
                if (res < 0) {
                    goto fail;
                }
                if (res == 0) {
                    goto exit;
                }
                break;
            }
        }

        if (_md_parse_item(i, item, &key, &value) < 0) {
            goto fail;
        }

        identity = md_calc_identity(md, key);
        if (identity == NULL) {
            goto fail;
        }

        Py_hash_t hash = _unicode_hash(identity);
        if (hash == -1) {
            goto fail;
        }

        switch (op) {
            case Update:
                if (_md_update(md, hash, identity, key, value, defer, marks) <
                    0) {
                    goto fail;
                }
                Py_CLEAR(identity);
                Py_CLEAR(key);
                Py_CLEAR(value);
                break;
            case Extend:
                if (_md_add_with_hash_steal_refs(
                        md, hash, identity, key, value) < 0) {
                    goto fail;
                }
                identity = NULL;
                key = NULL;
                value = NULL;
                break;
            case Merge:
                if (_md_merge(md, hash, identity, key, value, marks) < 0) {
                    goto fail;
                }
                Py_CLEAR(identity);
                Py_CLEAR(key);
                Py_CLEAR(value);
                break;
        }
        Py_CLEAR(item);
    }

exit:
    Py_CLEAR(it);
    Py_CLEAR(items);
    return 0;

fail:
    Py_CLEAR(identity);
    Py_CLEAR(it);
    Py_CLEAR(item);
    Py_CLEAR(key);
    Py_CLEAR(value);
    Py_CLEAR(items);
    return -1;
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
    atomic_store_uint64_relaxed(&md->version, NEXT_VERSION(md->state));

    // Publish the empty table before releasing any entry's reference: a
    // decref below may run arbitrary Python code (a __del__), which can
    // suspend this critical section. If md->keys still pointed at the old
    // table while that happens, a concurrent, correctly-locked reader
    // could observe entries mid-clear (identity already NULL, key/value
    // not yet). Swapping first means a suspended thread only ever sees
    // either the fully-populated old table or the fully-empty one.
    htkeys_t* old_keys = md->keys;
#ifdef Py_GIL_DISABLED
    _md_store_used(md, 0);
    _md_store_keys(md, (htkeys_t*)&empty_htkeys);
#else
    md->used = 0;
    md->keys = (htkeys_t*)&empty_htkeys;
#endif

#ifdef Py_GIL_DISABLED
    _md_retire(md, old_keys);
#else
    entry_t* entries = htkeys_entries(old_keys);
    Py_ssize_t nentries = old_keys->nentries;
    for (Py_ssize_t pos = 0; pos < nentries; pos++) {
        entry_t* entry = entries + pos;
        if (entry->identity != NULL) {
            Py_CLEAR(entry->identity);
            Py_CLEAR(entry->key);
            Py_CLEAR(entry->value);
        }
    }
    htkeys_free(old_keys);
#endif
    ASSERT_CONSISTENT(md, false);
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif
