Replace internal implementation from an array of items to hashtable.
algorithmic complexity for lookups is switched from O(N) to O(1).

Hashtable is very similar to :class:`dict` from CPython but it allows keys duplication.

The benchmark shows 25-50% boost for single lookups, x2-x3 for ulk updates, and x20 for
some multidis view operations.  The gain is not for free:
:class:`~multidict.MultiDict.add` and :class:`~multidict.MultiDict.extend` are 25-50%
slower now. We consider it as acceptable because the lookup is much more common
operation that addition for the library domain.
