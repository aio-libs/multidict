=========
multidict
=========

.. image:: https://travis-ci.org/aio-libs/multidict.svg?branch=master
    :target:  https://travis-ci.org/aio-libs/multidict
    :align: right
.. image:: https://codecov.io/gh/aio-libs/multidict/branch/master/graph/badge.svg
    :target: https://codecov.io/gh/aio-libs/multidict
.. image:: https://badges.gitter.im/Join%20Chat.svg
    :target: https://gitter.im/aio-libs/Lobby
    :alt: Chat on Gitter
Multidicts are useful for working with HTTP headers, URL
query args etc.

The code was extracted from aiohttp_ library.

Introduction
------------

*HTTP Headers* and *URL query string* require specific data structure:
*multidict*. It behaves mostly like a regular ``dict`` but it may have
several *values* for the same *key* and *preserves insertion ordering*.

The *key* is ``str`` (or ``istr`` for case-insensitive dictionaries).

``multidict`` has four multidict classes:
``MultiDict``, ``MultiDictProxy``, ``CIMultiDict``
and ``CIMultiDictProxy``.

Immutable proxies (``MultiDictProxy`` and
``CIMultiDictProxy``) provide a dynamic view for the
proxied multidict, the view reflects underlying collection changes. They
implement the ``collections.abc.Mapping`` interface.

Regular mutable (``MultiDict`` and ``CIMultiDict``) classes
implement ``collections.abc.MutableMapping`` and allows to change
their own content.


*Case insensitive* (``CIMultiDict`` and
``CIMultiDictProxy``) ones assume the *keys* are case
insensitive, e.g.::

   >>> dct = CIMultiDict(key='val')
   >>> 'Key' in dct
   True
   >>> dct['Key']
   'val'

*Keys* should be ``str`` or ``istr`` instances.

The library has optional Cython_ optimization for sake of speed.


License
-------

Apache 2


.. _aiohttp: https://github.com/KeepSafe/aiohttp
.. _Cython: http://cython.org/
