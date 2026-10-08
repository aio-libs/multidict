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
#include "freethreading.h"
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
   to widen its marks to the new room.

   An entry stays doomed only while it holds what it held when doomed: code
   run between items may write to it since (a nested update(), say), and
   that write stands. Every write stores a new value, so each doomed entry
   is recorded with its value, referenced to keep the address from being
   reused meanwhile, and md_post_update() removes only the entries that
   still hold it. The key is no guide: reading a CIMultiDict's key swaps
   the stored str for its istr. */
typedef struct _doomed_entry {
    Py_ssize_t index;
    PyObject* value;
} doomed_entry_t;

typedef struct _update_marks {
    bitmap_t updated;
    bitmap_t deleted;
    doomed_entry_t* doomed;
    Py_ssize_t ndoomed;
    Py_ssize_t doomed_capacity;
    Py_ssize_t reserve;  // room the first add makes; see md_reserve_batch()
    uint32_t layout_gen;
} update_marks_t;

static Py_ssize_t
_md_entries_capacity(const htkeys_t* keys)
{
    return keys->nentries + keys->usable;
}

/* Starts a batch of `size` items; the caller holds md's critical section
   until the matching update_marks_end(). */
static void
update_marks_init(update_marks_t* marks, MultiDictObject* md, Py_ssize_t size)
{
    Py_ssize_t capacity = _md_entries_capacity(md->keys);
    bitmap_init(&marks->updated, md->keys, capacity);
    bitmap_init(&marks->deleted, md->keys, capacity);
    marks->doomed = NULL;
    marks->ndoomed = 0;
    marks->doomed_capacity = 0;
    marks->reserve = size;
    marks->layout_gen = md->layout_gen;
    assert(md->batches < UINT16_MAX);
    md->batches++;
}

static void
update_marks_end(MultiDictObject* md)
{
    assert(md->batches > 0);
    md->batches--;
}

/* Called once the critical section is over: the references it drops can
   run a finalizer. */
static void
update_marks_release(update_marks_t* marks)
{
    bitmap_release(&marks->updated);
    bitmap_release(&marks->deleted);
    for (Py_ssize_t i = 0; i < marks->ndoomed; i++) {
        Py_DECREF(marks->doomed[i].value);
    }
    PyMem_Free(marks->doomed);
    marks->doomed = NULL;
    marks->ndoomed = 0;
}

COLD static int
_update_marks_grow_doomed(update_marks_t* marks)
{
    Py_ssize_t capacity =
        marks->doomed_capacity == 0 ? 64 : marks->doomed_capacity * 2;
    if ((size_t)capacity > PY_SSIZE_T_MAX / sizeof(doomed_entry_t)) {
        PyErr_NoMemory();
        return -1;
    }
    doomed_entry_t* doomed = PyMem_Realloc(
        marks->doomed, (size_t)capacity * sizeof(doomed_entry_t));
    if (doomed == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    marks->doomed = doomed;
    marks->doomed_capacity = capacity;
    return 0;
}

/* Dooms `entry`, at `index`; a no-op if the batch already has. */
static int
update_marks_doom(update_marks_t* marks, Py_ssize_t index, entry_t* entry)
{
    int seen = bitmap_test_and_set(&marks->deleted, index);
    if (seen != 0) {
        return seen < 0 ? -1 : 0;
    }
    if (marks->ndoomed == marks->doomed_capacity &&
        _update_marks_grow_doomed(marks) < 0) {
        bitmap_clear(&marks->deleted, index);
        return -1;
    }
    doomed_entry_t* doomed = marks->doomed + marks->ndoomed++;
    doomed->index = index;
    doomed->value = Py_NewRef(load_value(entry));
    return 0;
}

COLD static int
_update_marks_widen(update_marks_t* marks, MultiDictObject* md)
{
    marks->layout_gen = md->layout_gen;
    Py_ssize_t capacity = _md_entries_capacity(md->keys);
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
static int
update_marks_sync(update_marks_t* marks, MultiDictObject* md)
{
    if (UNLIKELY(marks->layout_gen != md->layout_gen)) {
        return _update_marks_widen(marks, md);
    }
    return 0;
}

// md got a new table
static void
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
