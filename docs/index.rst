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

The *key* is :class:`str` (or :class:`~multidict.istr` for case-insensitive
dictionaries).

:mod:`multidict` has four multidict classes:
:class:`~multidict.MultiDict`, :class:`~multidict.MultiDictProxy`,
:class:`~multidict.CIMultiDict` and :class:`~multidict.CIMultiDictProxy`.

Immutable proxies (:class:`~multidict.MultiDictProxy` and
:class:`~multidict.CIMultiDictProxy`) provide a dynamic view for the
proxied multidict, the view reflects underlying collection changes. They
implement the :class:`~collections.abc.Mapping` interface.

Regular mutable (:class:`~multidict.MultiDict` and
:class:`~multidict.CIMultiDict`) classes implement
:class:`~collections.abc.MutableMapping` and allows to change
their own content.


*Case insensitive* (:class:`~multidict.CIMultiDict` and
:class:`~multidict.CIMultiDictProxy`) ones assume the *keys*
are case insensitive, e.g.::

   >>> dct = CIMultiDict(key='val')
   >>> 'Key' in dct
   True
   >>> dct['Key']
   'val'

*Keys* should be either :class:`str` or :class:`~multidict.istr`
instance.

The library has optional C Extensions for sake of speed.

Library Installation
--------------------

.. code-block:: bash

   $ pip install multidict

The library is Python 3 only!

PyPI contains binary wheels for Linux, Windows and MacOS.  If you want to install
``multidict`` on another operation system (or *Alpine Linux* inside a Docker) the
Tarball will be used to compile the library from sources.  It requires C compiler and
Python headers installed.

To skip the compilation please use the :envvar:`MULTIDICT_NO_EXTENSIONS`
environment variable, e.g.:

.. code-block:: bash

   $ MULTIDICT_NO_EXTENSIONS=1 pip install multidict

Please note, Pure Python (uncompiled) version is about 20-50 times slower depending on
the usage scenario!!!


Source code
-----------

The project is hosted on GitHub_

Please file an issue on the `bug tracker
<https://github.com/aio-libs/multidict/issues>`_ if you have found a bug
or have some suggestion in order to improve the library.

Authors and License
-------------------

The ``multidict`` package is written by Andrew Svetlov.

It's *Apache 2* licensed and freely available.


Contents
--------

.. toctree::

   multidict
   benchmark
   changes

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`

.. _GitHub: https://github.com/aio-libs/multidict
