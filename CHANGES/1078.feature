
Don't create Python exception if key is not found but default value if provided.

The PR affects :meth:`MultiDict.getone`, :meth:`MultiDict.getall`,
:meth:`MultiDict.get`, :meth:`MultiDict.pop`, :meth:`MultiDict.popone` and
:meth:`MultiDict.popall` methods if the key is messed *and* default is provided.

Additionally, comparison multidicts with straight dicts becomes slightly faster on
Python 3.13+.

The speedup gain is about 60% for mentioned cases.
