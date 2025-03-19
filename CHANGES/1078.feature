Avoid Python exception creation if *key* is not found but *default* value is provided.

The PR affects :py:meth:`MultiDict.getone() <multidict.MultiDict.getone>`, :py:meth:`MultiDict.getall() <multidict.MultiDict.getall>`,
:py:meth:`MultiDict.get() <multidict.MultiDict.get>`, :py:meth:`MultiDict.pop(), :py:meth:`MultiDict.popone() <multidict.MultiDict.popone>`, and
:py:meth:`MultiDict.popall() <multidict.MultiDict.popall>` methods if the key is messed *and* default is provided.

Additionally, comparison :py:class:`~multidict.MultiDict` with straight :py:class:`dict`\ ionaries becomes slightly faster
on Python 3.13+.

The speedup gain is about 60% for mentioned cases.
