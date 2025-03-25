:meth:`multidict.MultiDict.popitem` and :meth:`multidict.CIMultiDict.popitem`
are changed to remove the latest entry instead of the fisrt.

It gives O(1) amortized complexity.

The standard :meth:`dict.popitem` removes the last entry also.
