#include "pythoncapi_compat.h"

#ifndef _MULTIDICT_UPDATE_MARKS_H
#define _MULTIDICT_UPDATE_MARKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <Python.h>
#include <stdbool.h>
#include <stdint.h>

#include "bitmap.h"
#include "compiler.h"
#include "dict.h"
#include "htkeys.h"

/* Per-batch bookkeeping for update() and merge(), keyed by entry index.

   `updated` holds the entries this batch has written (replaced or added):
   a later item for the same key must not find them again. `deleted` holds
   the entries this batch has doomed, for md_post_update() to remove; they
   stay whole until then, so whatever runs between items (Python code, or
   another thread while this one's critical section is suspended) finds
   ordinary live pairs.

   Unlike marks stored in the table, these are invisible to every other
   reader and writer. They need every index to keep naming the same entry
   while the batch runs, so md counts the batches in flight, and while
   there are any, nothing reuses an index: a resize keeps each entry where
   it is instead of compacting, popitem() leaves the holes at the end of
   the table, and a clear or re-init starts the new table with a hole for
   every old entry. Any new table bumps md->layout_gen, which tells a batch
   to widen its marks to the new room. */
typedef struct _update_marks {
    bitmap_t updated;
    bitmap_t deleted;
    uint32_t layout_gen;
} update_marks_t;

static inline Py_ssize_t
md_entries_capacity(htkeys_t* keys)
{
    return keys->nentries + keys->usable;
}

/* Starts a batch; the caller holds md's critical section until the
   matching update_marks_end(). */
static inline void
update_marks_init(update_marks_t* marks, MultiDictObject* md)
{
    Py_ssize_t capacity = md_entries_capacity(md->keys);
    bitmap_init(&marks->updated, md->keys, capacity);
    bitmap_init(&marks->deleted, md->keys, capacity);
    marks->layout_gen = md->layout_gen;
    assert(md->batches < UINT16_MAX);
    md->batches++;
}

static inline void
update_marks_end(MultiDictObject* md)
{
    assert(md->batches > 0);
    md->batches--;
}

static inline void
update_marks_release(update_marks_t* marks)
{
    bitmap_release(&marks->updated);
    bitmap_release(&marks->deleted);
}

COLD static int
_update_marks_widen(update_marks_t* marks, MultiDictObject* md)
{
    marks->layout_gen = md->layout_gen;
    Py_ssize_t capacity = md_entries_capacity(md->keys);
    if (bitmap_nbits(&marks->updated) >= capacity) {
        return 0;
    }
    if (bitmap_grow(&marks->updated, capacity) < 0) {
        return -1;
    }
    return bitmap_grow(&marks->deleted, capacity);
}

/* Makes room in the marks for every index md's table has, after md got a
   new one. */
static inline int
update_marks_sync(update_marks_t* marks, MultiDictObject* md)
{
    if (UNLIKELY(marks->layout_gen != md->layout_gen)) {
        return _update_marks_widen(marks, md);
    }
    return 0;
}

// md got a new table
static inline void
update_marks_moved(MultiDictObject* md)
{
    if (md->batches != 0) {
        md->layout_gen++;
    }
}

#ifdef __cplusplus
}
#endif

#endif
