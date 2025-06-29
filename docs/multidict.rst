.. _multidict-reference:

============
Reference
============

.. module:: multidict



MultiDict
=========

.. class:: MultiDict(**kwargs)
           MultiDict(mapping, **kwargs)
           MultiDict(iterable, **kwargs)

   Creates a mutable multidict instance.

   Accepted parameters are the same as for :class:`dict`.
   If the same key appears several times it will be added, e.g.::

      >>> d = MultiDict([('a', 1), ('b', 2), ('a', 3)])
      >>> d
      <MultiDict ('a': 1, 'b': 2, 'a': 3)>

   .. method:: len(d)

      Return the number of items in multidict *d*.

   .. method:: d[key]

      Return the **first** item of *d* with key *key*.

      Raises a :exc:`KeyError` if key is not in the multidict.

   .. method:: d[key] = value

      Set ``d[key]`` to *value*.

      Replace all items where key is equal to *key* with single item
      ``(key, value)``.

   .. method:: del d[key]

      Remove all items where key is equal to *key* from *d*.
      Raises a :exc:`KeyError` if *key* is not in the map.

   .. method:: key in d

      Return ``True`` if d has a key *key*, else ``False``.

   .. method:: key not in d

      Equivalent to ``not (key in d)``

   .. method:: iter(d)

      Return an iterator over the keys of the dictionary.
      This is a shortcut for ``iter(d.keys())``.

   .. method:: add(key, value)

      Append ``(key, value)`` pair to the dictionary.

   .. method:: clear()

      Remove all items from the dictionary.

   .. method:: copy()

      Return a shallow copy of the dictionary.

   .. method:: getone(key[, default])

      Return the **first** value for *key* if *key* is in the
      dictionary, else *default*.

      Raises :exc:`KeyError` if *default* is not given and *key* is not found.

      ``d[key]`` is equivalent to ``d.getone(key)``.

   .. method:: getall(key[, default])

      Return a list of all values for *key* if *key* is in the
      dictionary, else *default*.

      Raises :exc:`KeyError` if *default* is not given and *key* is not found.

   .. method:: get(key[, default])

      Return the **first** value for *key* if *key* is in the
      dictionary, else *default*.

      If *default* is not given, it defaults to ``None``, so that this
      method never raises a :exc:`KeyError`.

      ``d.get(key)`` is equivalent to ``d.getone(key, None)``.

   .. method:: keys()

      Return a new view of the dictionary's keys.

      View contains all keys, possibly with duplicates.

   .. method:: items()

      Return a new view of the dictionary's items (``(key, value)`` pairs).

      View contains all items, multiple items can have the same key.

   .. method:: values()

      Return a new view of the dictionary's values.

      View contains all values.

   .. method:: popone(key[, default])

      If *key* is in the dictionary, remove it and return its the
      **first** value, else return *default*.

      If *default* is not given and *key* is not in the dictionary, a
      :exc:`KeyError` is raised.

      .. versionadded:: 3.0

   .. method:: pop(key[, default])

      An alias to :meth:`popone`

      .. versionchanged:: 3.0

         Now only *first* occurrence is removed (was all).

   .. method:: popall(key[, default])

      If *key* is in the dictionary, remove all occurrences and return
      a :class:`list` of all values in corresponding order (as
      :meth:`getall` does).

      If *key* is not found and *default* is provided return *default*.

      If *default* is not given and *key* is not in the dictionary, a
      :exc:`KeyError` is raised.

      .. versionadded:: 3.0

   .. method:: popitem()

      Remove and return an arbitrary ``(key, value)`` pair from the dictionary.

      :meth:`popitem` is useful to destructively iterate over a
      dictionary, as often used in set algorithms.

      If the dictionary is empty, calling :meth:`popitem` raises a
      :exc:`KeyError`.

   .. method:: setdefault(key[, default])

      If *key* is in the dictionary, return its the **first** value.
      If not, insert *key* with a value of *default* and return *default*.
      *default* defaults to ``None``.

   .. method:: extend([other], **kwargs)

      Extend the dictionary with the key/value pairs from *other* and *kwargs*,
      appending the pairs to this dictionary. For existing keys,
      values are added.

      Returns ``None``.

      :meth:`extend` accepts either another dictionary object or an
      iterable of key/value pairs (as tuples or other iterables of
      length two). If keyword arguments are specified, the dictionary
      is then extended with those key/value pairs:
      ``d.extend(red=1, blue=2)``.

      Effectively the same as calling :meth:`add` for every
      ``(key, value)`` pair.

      .. seealso::

         :meth:`merge` and :meth:`update`

   .. method:: merge([other], **kwargs)

      Merge the dictionary with the key/value pairs from *other* and *kwargs*,
      appending non-existing pairs to this dictionary. For existing keys,
      the addition is skipped.

      Returns ``None``.

      :meth:`merge` accepts either another dictionary object or an
      iterable of key/value pairs (as tuples or other iterables of
      length two). If keyword arguments are specified, the dictionary
      is then merged with those key/value pairs:
      ``d.merge(red=1, blue=2)``.

      Effectively the same as calling :meth:`add` for every
      ``(key, value)`` pair where ``key not in self``.

      .. seealso::

         :meth:`extend` and :meth:`update`

      .. versionadded:: 6.6

   .. method:: update([other], **kwargs)

      Update the dictionary with the key/value pairs from *other* and *kwargs*,
      overwriting existing keys.

      Returns ``None``.

      :meth:`update` accepts either another dictionary object or an
      iterable of key/value pairs (as tuples or other iterables
      of length two). If keyword arguments are specified, the
      dictionary is then updated with those key/value pairs:
      ``d.update(red=1, blue=2)``.

      .. seealso::

         :meth:`extend` and :meth:`merge`

   .. seealso::

      :class:`MultiDictProxy` can be used to create a read-only view
      of a :class:`MultiDict`.


CIMultiDict
===========


.. class:: CIMultiDict(**kwargs)
           CIMultiDict(mapping, **kwargs)
           CIMultiDict(iterable, **kwargs)

   Create a case insensitive multidict instance.

   The behavior is the same as of :class:`MultiDict` but key
   comparisons are case insensitive, e.g.::

      >>> dct = CIMultiDict(a='val')
      >>> 'A' in dct
      True
      >>> dct['A']
      'val'
      >>> dct['a']
      'val'
      >>> dct['b'] = 'new val'
      >>> dct['B']
      'new val'

   The class is inherited from :class:`MultiDict`.

   .. seealso::

      :class:`CIMultiDictProxy` can be used to create a read-only view
      of a :class:`CIMultiDict`.


MultiDictProxy
==============

.. class:: MultiDictProxy(multidict)

   Create an immutable multidict proxy.

   It provides a dynamic view on
   the multidict’s entries, which means that when the multidict changes,
   the view reflects these changes.

   Raises :exc:`TypeError` if *multidict* is not a :class:`MultiDict` instance.

   .. method:: len(d)

      Return number of items in multidict *d*.

   .. method:: d[key]

      Return the **first** item of *d* with key *key*.

      Raises a :exc:`KeyError` if key is not in the multidict.

   .. method:: key in d

      Return ``True`` if d has a key *key*, else ``False``.

   .. method:: key not in d

      Equivalent to ``not (key in d)``

   .. method:: iter(d)

      Return an iterator over the keys of the dictionary.
      This is a shortcut for ``iter(d.keys())``.

   .. method:: copy()

      Return a shallow copy of the underlying multidict.

   .. method:: getone(key[, default])

      Return the **first** value for *key* if *key* is in the
      dictionary, else *default*.

      Raises :exc:`KeyError` if *default* is not given and *key* is not found.

      ``d[key]`` is equivalent to ``d.getone(key)``.

   .. method:: getall(key[, default])

      Return a list of all values for *key* if *key* is in the
      dictionary, else *default*.

      Raises :exc:`KeyError` if *default* is not given and *key* is not found.

   .. method:: get(key[, default])

      Return the **first** value for *key* if *key* is in the
      dictionary, else *default*.

      If *default* is not given, it defaults to ``None``, so that this
      method never raises a :exc:`KeyError`.

      ``d.get(key)`` is equivalent to ``d.getone(key, None)``.

   .. method:: keys()

      Return a new view of the dictionary's keys.

      View contains all keys, possibly with duplicates.

   .. method:: items()

      Return a new view of the dictionary's items (``(key, value)`` pairs).

      View contains all items, multiple items can have the same key.

   .. method:: values()

      Return a new view of the dictionary's values.

      View contains all values.

CIMultiDictProxy
================

.. class:: CIMultiDictProxy(multidict)

   Case insensitive version of :class:`MultiDictProxy`.

   Raises :exc:`TypeError` if *multidict* is not :class:`CIMultiDict` instance.

   The class is inherited from :class:`MultiDict`.


Version
=======

All multidicts have an internal version flag. It's changed on every
dict update, thus the flag could be used for checks like cache
expiring etc.

.. function:: getversion(mdict)

   Return a version of given *mdict* object (works for proxies also).

   The type of returned value is opaque and should be used for
   equality tests only (``==`` and ``!=``), ordering is not allowed
   while not prohibited explicitly.

  .. versionadded:: 3.0

  .. seealso:: :pep:`509`


istr
====

:class:`CIMultiDict` accepts :class:`str` as *key* argument for dict
lookups but uses case-folded (lower-cased) strings for the comparison internally.

For more effective processing it should know if the *key* is already
case-folded to skip the :meth:`~str.lower()` call.

The performant code may create
case-folded string keys explicitly hand, e.g::

   >>> key = istr('Key')
   >>> key
   'Key'
   >>> mdict = CIMultiDict(key='value')
   >>> key in mdict
   True
   >>> mdict[key]
   'value'

For performance :class:`istr` strings should be created once and
stored somewhere for the later usage, see :mod:`aiohttp:aiohttp.hdrs` for example.

.. class:: istr(object='')
           istr(bytes_or_buffer[, encoding[, errors]])

      Create a new **case-folded** string object from the given
      *object*. If *encoding* or *errors* are specified, then the
      object must expose a data buffer that will be decoded using the
      given encoding and error handler.

      Otherwise, returns the result of ``object.__str__()`` (if defined)
      or ``repr(object)``.

      *encoding* defaults to ``sys.getdefaultencoding()``.

      *errors* defaults to ``'strict'``.

      The class is inherited from :class:`str` and has all regular
      string methods.

.. versionchanged:: 2.0

   ``upstr()`` is a deprecated alias for :class:`istr`.

.. versionchanged:: 3.7

   :class:`istr` doesn't title-case its argument anymore but uses
   internal lower-cased data for fast case-insensitive comparison.


Abstract Base Classes
=====================

The module provides two ABCs: ``MultiMapping`` and
``MutableMultiMapping``.  They are similar to
:class:`collections.abc.Mapping` and
:class:`collections.abc.MutableMapping` and inherited from them.

.. versionadded:: 3.3


Typing
======

The library is shipped with embedded type annotations, mypy just picks the annotations
by default.

:class:`MultiDict`, :class:`CIMultiDict`, :class:`MultiDictProxy`, and
:class:`CIMultiDictProxy` are *generic* types; please use the corresponding notation for
multidict value types, e.g. ``md: MultiDict[str] = MultiDict()``.

The type of multidict keys is always :class:`str` or a class derived from a string.

.. versionadded:: 3.7


Environment variables
=====================

.. envvar:: MULTIDICT_NO_EXTENSIONS

   An environment variable that instructs the packaging scripts to skip
   compiling the C-extension based variant of :mod:`multidict`.
   When used in runtime, it instructs the pure-Python variant to be imported
   from the top-level :mod:`multidict` entry-point package, even when the
   C-extension implementation is available.

   .. caution::

      The pure-Python (uncompiled) version is roughly 20-50 times slower than
      its C counterpart, depending on the way it's used.

.. envvar:: MULTIDICT_DEBUG_BUILD

   An environment variable that instructs the packaging scripts to compile
   the C-extension based variant of :mod:`multidict` with debug symbols.
   This is useful for debugging the C-extension code, but it will result in
   a larger binary size and worse performance.

   .. caution::

      The debug build is not intended for production use and should only be
      used for development and debugging purposes.

C-API
=====

The library is also shipped with a C-API, the header files can be compiled using
**multidict.__path__[0]**.

.. versionadded:: 6.7

.. c:enum:: UpdateOp

   Specifies the operation kind for :c:func:`MultiDict_UpdateFromMultiDict`,
   :c:func:`MultiDict_UpdateFromDict`, and :c:func:`MultiDict_UpdateFromSequence`
   calls.

   The followings members are recognized:

   * ``Extend`` = 0, for :class:`~multidict.MultiDict.extend`.

   * ``Update`` = 1, for :class:`~multidict.MultiDict.update`.

   * ``Merge`` = 0, for :class:`~multidict.MultiDict.merge`.

.. c:type:: uint64_t

   An unsigned long long 64 bit integer that can be found in **stdint.h**

.. c:struct:: MultiDict_CAPI

   Internal structure to keep C API structures.

   .. note::

      It is similar to the way `Numpy
      <https://numpy.org/doc/stable/reference/c-api/index.html>`_ and `Datetime
      <https://docs.python.org/3/c-api/datetime.html>`_ work. multidict utilizes a
      `Python Capsule <https://docs.python.org/3/c-api/capsule.html#>`_ to expose the
      CAPI to other low level projects.

   .. c:var:: void* state;

      Carries the module state to help lookup types and other objects.


.. c:function:: MultiDict_CAPI* MultiDict_Import()

   Imports Multidict CAPI as a capsule object.

   :returns: A Capsule Containing the Multidict CAPI
   :retval NULL: if object fails to import


.. c:function:: PyTypeObject* MultiDict_GetType(MultiDict_CAPI* api)

   Obtains the :class:`MultiDict` TypeObject.

   :param api: Python Capsule Pointer
   :returns: return A CPython `PyTypeObject` is returned as a pointer
   :retval NULL: if an exception was raised


.. c:function:: int MultiDict_CheckExact(MultiDict_CAPI* api, PyObject* op)

   Checks if :class:`MultiDict` object type matches exactly.

   :param api: Python Capsule Pointer
   :param op: The Object to check
   :retval 1: if true
   :retval 0: if false
   :retval -1: if exception was raised

.. c:function:: int MultiDict_Add(MultiDict_CAPI* api, PyObject* self, PyObject* key, PyObject* value)

   Adds a new entry to the :class:`MultiDict` object.

   :param api: Python Capsule Pointer
   :param self: the Multidict object
   :param key: The key of the entry to add
   :param value: The value of the entry to add
   :retval 0: on success
   :reval -1: on failure

.. c:function:: int MultiDict_Clear(MultiDict_CAPI* api, PyObject* self)

   Clears a :class:`MultiDict` object and removes all it's entries.

   :param api: Python Capsule Pointer
   :param self: the :class:`MultiDict` object
   :retval 0: if success
   :retval -1: on failure and will raise :exc:`TypeError` if :class:`MultiDict` Type is incorrect


.. c:function:: PyObject* Multidict_SetDefault(MultiDict_CAPI* api, PyObject* self, PyObject* key, PyObject* _default)

   If key is in the dictionary its the first value.
   If not, insert key with a value of default and return default.

   :param api: Python Capsule Pointer
   :param self: the :class:`MultiDict` object
   :param key: the key to insert
   :param _default: the default value to have inserted
   :returns: the default object on success
   :retval NULL: on failure

.. c:function:: int MutliDict_Del(MultiDict_CAPI* api, PyObject* self, PyObject* key)

   Remove all items where key is equal to key from d.

   :param api: Python Capsule Pointer
   :param self: the :class:`MultiDict` object
   :param key: the key to be removed
   :retval 0: on success,
   :retval -1: on failure followed by raising either :exc:`TypeError` or :exc:`KeyError` if key is not in the map.

.. c:function:: uint64_t MultiDict_Version(MultiDict_CAPI* api, PyObject* self)

   Return a version of given :class:`MultiDict` object

   :param api: Python Capsule Pointer
   :param self: the :class:`MultiDict` object
   :returns: the version flag of the object, otherwise 0 on failure

.. c:function:: int MultiDict_Contains(MultiDict_CAPI* api, PyObject* self, PyObject* key)

   Determines if a certain key exists a multidict object

   :param api: Python Capsule Pointer
   :param self: the :class:`MultiDict` object
   :param key: the key to look for
   :retval 1: if true
   :retval 0: if false,
   :retval -1: if failure had occurred


.. c:function:: PyObject* MultiDict_GetAll(MultiDict_CAPI* api, PyObject* self, PyObject* key, PyObject** result)

   Return the all values corresponding to *key* if *key* is in the
   dictionary, else *NULL*.

   :param api: Python Capsule Pointer
   :param self: the :class:`MultiDict` object
   :param key: the key to get one item from
   :param result: the object to attached the obtained object to.
   :retval 1: on success
   :retval 0: on failure (No exceptions are raised)
   :retval -1: on :exc:`TypeError` raised

.. c:function:: PyObject* MultiDict_GetOne(MultiDict_CAPI* api, PyObject* self, PyObject* key, PyObject** result)

   Return the **first** value for *key* if *key* is in the
   dictionary, else *NULL*.

   :param api: Python Capsule Pointer
   :param self: the :class:`MultiDict` object
   :param key: the key to get one item from
   :param result: the object to attached the obtained object to.
   :retval 1: on success
   :retval 0: on failure (No exceptions are raised)
   :retval -1: on :exc:`TypeError` raised

.. c:function:: PyObject* MultiDict_PopOne(MultiDict_CAPI* api, PyObject* self, PyObject* key)

   Remove and return a value from the dictionary.

   :param api: Python Capsule Pointer
   :param self: the :class:`MultiDict` object
   :param key: the key to remove
   :returns: corresponding value on success or None
   :retval NULL: on failure and raises :exc:`TypeError`

.. c:function:: PyObject* MultiDict_PopAll(MultiDict_CAPI* api, PyObject* self, PyObject* key)

   Pops all related objects corresponding to `key`

   :param api: Python Capsule Pointer
   :param self: the :class:`MultiDict` object
   :param key: the key to pop all of
   :returns: :class:`list` object on success
   :retval NULL: on error and raises either :exc:`KeyError` or :exc:`TypeError`


.. c:function:: PyObject* MultiDict_PopItem(MultiDict_CAPI* api, PyObject* self)

   Remove and return an arbitrary ``(key, value)`` pair from the
   dictionary.

   :param api: Python Capsule Pointer
   :param self: the :class:`MultiDict` object
   :returns: an arbitrary tuple on success
   :retval NULL: on error along with :exc:`TypeError` or :exc:`KeyError` raised


.. c:function:: int MultiDict_Replace(MultiDict_CAPI* api, PyObject* self, PyObject* key, PyObject* value)

   Replaces a set object with another object

   :param api: Python Capsule Pointer
   :param self: the :class:`MultiDict` object
   :param key: the key to lookup for replacement
   :param value: the value to replace with
   :retval 0: on success
   :retval -1: on failure and raises :exc:`TypeError`


.. c:function:: int MultiDict_UpdateFromMultiDict(MultiDict_CAPI* api, PyObject* self, PyObject* other, UpdateOp op)

   Updates Multidict object using another MultiDict Object

   :param api: Python Capsule Pointer
   :param self: the :class:`MultiDict` object
   :param other: a multidict object to update corresponding object with
   :param op: :c:enum:`UpdateOp` operation for extending, updating, or merging values.
   :retval 0: on success
   :retval -1: on failure



.. c:function:: int MultiDict_UpdateFromDict(MultiDict_CAPI* api, PyObject* self, PyObject* kwds, UpdateOp op)

   Updates Multidict object using another Dictionary Object

   :param api: Python Capsule Pointer
   :param self: the :class:`MultiDict` object
   :param kwds: the keywords or Dictionary object to merge
   :param op: :c:enum:`UpdateOp` operation for extending, updating, or merging values.
   :retval 0: on success
   :retval -1: on failure


.. c:function:: int MultiDict_UpdateFromSequence(MultiDict_CAPI* api, PyObject* self, PyObject* seq, UpdateOp op)

   Updates Multidict object using a sequence object

   :param api: Python Capsule Pointer
   :param self: the :class:`MultiDict` object
   :param seq: the sequence to merge with.
   :param op: :c:enum:`UpdateOp` operation for extending, updating, or merging values.
   :retval 0: on success
   :retval -1: on failure


.. c:function:: PyObject* MultiDictProxy_New(MultiDict_CAPI* api, PyObject* md)

   Initalizes a :class:`MultiDictProxy` from any other Multidict object 
   making an immutable version of it.

   :param api: Python Capsule Pointer
   :param md: the :class:`MultiDict` object to make immutable
   :returns: a :class:`MultiDictProxy` on success
   :retval NULL: on failure and raises :exc:`TypeError`


.. c:function:: int MultiDictProxy_Contains(MultiDict_CAPI* api, PyObject* self, PyObject* key)

   Determines if a certain key exists a :class:`MultiDictProxy` object

   :param api: Python Capsule Pointer
   :param self: the :class:`MultiDictProxy` object
   :param key: the key to look for
   :retval 1: if true
   :retval 0: if false,
   :retval -1: if failure had occurred

.. c:function:: PyObject* MultiDictProxy_GetAll(MultiDict_CAPI* api, PyObject* self, PyObject* key, PyObject** result)

   Return the all values corresponding to *key* if *key* is in the
   dictionary, else *NULL*.
   
   :param api: Python Capsule Pointer
   :param self: the :class:`MultiDictProxy` object
   :param key: the key to get one item from
   :param result: the object to attached the obtained object to.
   :retval 1: on success
   :retval 0: on failure (No exceptions are raised)
   :retval -1: on :exc:`TypeError` raised

.. c:function:: PyObject* MultiDictProxy_GetOne(MultiDict_CAPI* api, PyObject* self, PyObject* key, PyObject** result)

   Return the **first** value for *key* if *key* is in the
   dictionary, else *NULL*.

   :param api: Python Capsule Pointer
   :param self: the :class:`MultiDictProxy` object
   :param key: the key to get one item from
   :param result: the object to attached the obtained object to.
   :retval 1: on success
   :retval 0: on failure (No exceptions are raised)
   :retval -1: on :exc:`TypeError` raised

.. c:function:: PyTypeObject* MultiDictProxy_GetType(MultiDict_CAPI* api)

   Obtains the :class:`MultiDictProxy` TypeObject.

   :param api: Python Capsule Pointer
   :returns: return A CPython `PyTypeObject` is returned as a pointer
   :retval NULL: if an exception was raised

