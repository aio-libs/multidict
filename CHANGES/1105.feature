:meth:`multidict.MultiDict.popitem` is changed to remove
the latest entry instead of the first.

It gives O(1) amortized complexity.

The standard :meth:`dict.popitem` removes the last entry also.
