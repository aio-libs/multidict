Replace internal implementation from an array of items to hash table.
algorithmic complexity for lookups is switched from O(N) to O(1).

The hash table is very similar to :class:`dict` from CPython but it allows keys duplication.

The benchmark shows 25-50% boost for single lookups, x2-x3 for bulk updates, and x20 for
some multidict view operations.  The gain is not for free:
:class:`~multidict.MultiDict.add` and :class:`~multidict.MultiDict.extend` are 25-50%
slower now. We consider it as acceptable because the lookup is much more common
operation that addition for the library domain.
