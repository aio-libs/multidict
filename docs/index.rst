.. aiohttp documentation master file, created by
   sphinx-quickstart on Wed Mar  5 12:35:35 2014.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

multidict
=========

Multidicts are useful for working with HTTP headers, URL
query args etc.

The code was extracted from aiohttp library.

Introduction
------------

*HTTP Headers* and *URL query string* require specific data structure:
*multidict*. It behaves mostly like a regular :class:`dict` but it may have
several *values* for the same *key* and *preserves insertion ordering*.

The *key* is :class:`str` (or :class:`istr` for case-insensitive
dictionaries).

:mod:`multidict` has four multidict classes:
:class:`MultiDict`, :class:`MultiDictProxy`, :class:`CIMultiDict`
and :class:`CIMultiDictProxy`.

Immutable proxies (:class:`MultiDictProxy` and
:class:`CIMultiDictProxy`) provide a dynamic view for the
proxied multidict, the view reflects underlying collection changes. They
implement the :class:`~collections.abc.Mapping` interface.

Regular mutable (:class:`MultiDict` and :class:`CIMultiDict`) classes
implement :class:`~collections.abc.MutableMapping` and allows to change
their own content.


*Case insensitive* (:class:`CIMultiDict` and
:class:`CIMultiDictProxy`) ones assume the *keys* are case
insensitive, e.g.::

   >>> dct = CIMultiDict(key='val')
   >>> 'Key' in dct
   True
   >>> dct['Key']
   'val'

*Keys* should be either :class:`str` or :class:`istr` instance.

The library has optional Cython_ optimization for sake of speed.

Library Installation
--------------------

.. code-block:: bash

   $ pip install multidict

The library is Python 3 only!


Source code
-----------

The project is hosted on GitHub_

Please file an issue on the `bug tracker
<https://github.com/aio-libs/multidict/issues>`_ if you have found a bug
or have some suggestion in order to improve the library.

The library uses `Travis <https://travis-ci.org/aio-libs/multidict>`_ for
Continuous Integration.

Discussion list
---------------

*aio-libs* google group: https://groups.google.com/forum/#!forum/aio-libs

Feel free to post your questions and ideas here.


Authors and License
-------------------

The ``multidict`` package is written by Andrew Svetlov.

It's *Apache 2* licensed and freely available.


Contents
--------

.. toctree::

   multidict

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`

.. _GitHub: https://github.com/aio-libs/multidict
.. _Cython: http://cython.org/
