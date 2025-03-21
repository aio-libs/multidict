The C-extension no longer pre-allocates a Python exception object in
lookup-related methods of :py:class:`~multidict.MultiDict` when the
passed-in *key* is not found but *default* value is provided.

Namely, this affects :py:meth:`MultiDict.getone()
<multidict.MultiDict.getone>`, :py:meth:`MultiDict.getall()
<multidict.MultiDict.getall>`, :py:meth:`MultiDict.get()
<multidict.MultiDict.get>`, :py:meth:`MultiDict.pop()
<multidict.MultiDict.pop>`, :py:meth:`MultiDict.popone()
<multidict.MultiDict.popone>`, and :py:meth:`MultiDict.popall()
<multidict.MultiDict.popall>`.

Additionally, the :py:class:`~multidict.MultiDict` comparison with
regular :py:class:`dict`\ ionaries is now about 60% faster
on Python 3.13+ in the fallback-to-default case.
