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

   `updated` holds the entries this batch has written (replaced, revived
   or added): a later item for the same key must not find them again.
   `deleted` holds the entries this batch has half-deleted (key and value
   gone, identity kept), for md_post_update() to finish off.

   Unlike marks stored in the table, these are invisible to every other
   reader and writer. The price is that they describe one exact table
   layout: our own resizes carry them over (see _md_update_marks_remap()),
   but a mutation made from outside the batch -- Python code run between
   items, or another thread while this one's critical section is
   suspended -- invalidates them. _md_update_marks_sync() detects that,
   drops them, and `lost` tells md_post_update() to fall back to a
   full-table sweep. */
typedef struct _md_update_marks {
    md_bitmap_t updated;
    md_bitmap_t deleted;
    uint64_t version;
    bool lost;
} md_update_marks_t;

static inline Py_ssize_t
_md_entries_capacity(htkeys_t* keys)
{
    return keys->nentries + keys->usable;
}

static inline void
md_update_marks_init(md_update_marks_t* marks, MultiDictObject* md)
{
    Py_ssize_t capacity = _md_entries_capacity(md->keys);
    md_bitmap_init(&marks->updated, md->keys, capacity);
    md_bitmap_init(&marks->deleted, md->keys, capacity);
    marks->version = md->version;
    marks->lost = false;
}

static inline void
md_update_marks_release(md_update_marks_t* marks)
{
    md_bitmap_release(&marks->updated);
    md_bitmap_release(&marks->deleted);
}

static inline void
_md_update_marks_sync(md_update_marks_t* marks, MultiDictObject* md)
{
    if (UNLIKELY(md->keys != marks->updated.keys ||
                 md->version != marks->version)) {
        md_update_marks_release(marks);
        md_update_marks_init(marks, md);
        marks->lost = true;
    }
}

/* Rebuilds `bm` for `newkeys`, given that the resize keeps exactly the
   entries with a non-NULL identity, in order. */
COLD static int
_md_bitmap_remap(md_bitmap_t* bm, entry_t* oldentries, Py_ssize_t oldnentries,
                 htkeys_t* newkeys, Py_ssize_t newcapacity)
{
    md_bitmap_t fresh;
    md_bitmap_init(&fresh, newkeys, newcapacity);
    if (md_bitmap_next(bm, 0) >= 0) {
        if (md_bitmap_reserve(&fresh) < 0) {
            return -1;
        }
        Py_ssize_t j = 0;
        for (Py_ssize_t i = 0; i < oldnentries; i++) {
            if (oldentries[i].identity == NULL) {
                continue;
            }
            if (md_bitmap_test(bm, i)) {
                md_bitmap_set(&fresh, j);  // can't fail, reserved above
            }
            j++;
        }
    }
    md_bitmap_move(bm, &fresh);
    return 0;
}

/* Must run before the resize moves any entry: it reads the old layout,
   and failing here leaves the table untouched. */
static inline int
_md_update_marks_remap(md_update_marks_t* marks, htkeys_t* oldkeys,
                       htkeys_t* newkeys, Py_ssize_t newcapacity)
{
    if (marks == NULL) {
        return 0;
    }
    entry_t* oldentries = htkeys_entries(oldkeys);
    Py_ssize_t oldnentries = oldkeys->nentries;
    if (_md_bitmap_remap(
            &marks->updated, oldentries, oldnentries, newkeys, newcapacity) <
        0) {
        return -1;
    }
    return _md_bitmap_remap(
        &marks->deleted, oldentries, oldnentries, newkeys, newcapacity);
}

#ifdef __cplusplus
}
#endif

#endif
