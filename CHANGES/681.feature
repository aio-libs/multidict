Use ``METH_FASTCALL`` where it makes sense.

``MultiDict.add()`` is 2.2 times faster now, ``CIMultiDict.add()`` is 1.5 times faster.
The same boost is applied to ``get*()``, ``setdefault()``, and ``pop*()`` methods.