=========
multidict
=========

Multidicts are useful for working with HTTP headers, URL
query args etc.

The code was extracted from aiohttp library.

Introduction
------------

*HTTP Headers* and *URL query string* require specific data structure:
*multidict*. It behaves mostly like a ``dict`` but it can have
several *values* for the same *key*.

``multidict`` has four multidict classes:
``MultiDict``, ``MultiDictProxy``, ``CIMultiDict``
and ``CIMultiDictProxy``.

Immutable proxies (``MultiDictProxy`` and
``CIMultiDictProxy``) provide a dynamic view on the
proxied multidict, the view reflects underlying collection changes. They
implement the ``collections.abc.Mapping`` interface.

Regular mutable (``MultiDict`` and ``CIMultiDict``) classes
implement ``collections.abc.MutableMapping`` and allows to change
their own content.


*Case insensitive* (``CIMultiDict`` and
``CIMultiDictProxy``) ones assumes the *keys* are case
insensitive, e.g.::

   >>> dct = CIMultiDict(a='val')
   >>> 'A' in dct
   True
   >>> dct['A']
   'val'

*Keys* should be ``str`` instances.

The library has optional Cython optimization for sake of speed.


License
-------

Apache 2
