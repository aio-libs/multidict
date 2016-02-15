=========
multidict
=========

Multidicts are useful for working with HTTP headers, URL
query args etc.

The code was extracted from aiohttp library.

Introduction
------------

*HTTP Headers* and *URL query string* require specific data structure:
*multidict*. It behaves mostly like a :class:`dict` but it can have
several *values* for the same *key*.

:mod:`multidict` has four multidict classes:
:class:`MultiDict`, :class:`MultiDictProxy`, :class:`CIMultiDict`
and :class:`CIMultiDictProxy`.

Immutable proxies (:class:`MultiDictProxy` and
:class:`CIMultiDictProxy`) provide a dynamic view on the
proxied multidict, the view reflects underlying collection changes. They
implement the :class:`~collections.abc.Mapping` interface.

Regular mutable (:class:`MultiDict` and :class:`CIMultiDict`) classes
implement :class:`~collections.abc.MutableMapping` and allows to change
their own content.


*Case insensitive* (:class:`CIMultiDict` and
:class:`CIMultiDictProxy`) ones assumes the *keys* are case
insensitive, e.g.::

   >>> dct = CIMultiDict(a='val')
   >>> 'A' in dct
   True
   >>> dct['A']
   'val'

*Keys* should be a :class:`str`.

The library has optional Cython optimization for sake of speed.


License
-------

Apache 2
