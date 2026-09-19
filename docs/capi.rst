.. _multidict-capi:

===========
C API
===========

.. highlight:: c

``multidict`` exposes a small public C API for other C extensions to
create and manipulate :class:`~multidict.MultiDict`,
:class:`~multidict.CIMultiDict`, :class:`~multidict.MultiDictProxy`,
:class:`~multidict.CIMultiDictProxy` and :class:`~multidict.istr`
instances directly, without going through the Python-level API.

.. versionadded:: 7.0

The API is published as a `capsule
<https://docs.python.org/3/c-api/capsule.html>`_ named
``multidict._multidict.CAPI``. A client only ever includes
``multidict_capi.h``, which declares :c:type:`MultiDict_CAPI` and the
inline wrapper functions documented below.

The header ships in the ``multidict`` wheel, so a client extension only
needs its build to see the installed ``multidict`` package on the include
path. :func:`multidict.get_include` returns that directory:

.. code-block:: pycon

   >>> import multidict
   >>> multidict.get_include()
   '/.../site-packages/multidict'

.. function:: multidict.get_include() -> str

   Return the directory containing ``multidict_capi.h``, to pass as
   an ``-I`` include path when building a C extension against it.

   .. versionadded:: 7.0

Importing the capsule
======================

.. c:function:: MultiDict_CAPI *MultiDict_GetCAPI(void)

   Import the capsule and return a pointer to it, or ``NULL`` with an
   exception set on failure (for example if ``multidict._multidict``
   could not be imported, or if the installed ``multidict`` provides
   an older, incompatible capsule version than this header requires --
   raised as :exc:`RuntimeError`).

   This wraps `PyCapsule_Import()
   <https://docs.python.org/3/c-api/capsule.html#c.PyCapsule_Import>`_,
   so ``multidict`` should already be imported (e.g. via
   ``PyImport_ImportModule("multidict")`` in C) before calling this.

   Call this once, typically from the importing module's
   ``Py_mod_exec`` slot, and keep the returned pointer around (e.g. in
   the module's per-module state) for the lifetime of that module::

      #include <multidict_capi.h>

      static int
      module_exec(PyObject *mod)
      {
          my_mod_state *state = PyModule_GetState(mod);
          state->capi = MultiDict_GetCAPI();
          if (state->capi == NULL) {
              return -1;
          }
          return 0;
      }

   Every other function on this page takes the returned
   :c:type:`MultiDict_CAPI` pointer as its first argument.

.. c:type:: MultiDict_CAPI

   An opaque handle to the capsule's function table. Client code never
   accesses its fields directly; it is only ever passed to the wrapper
   functions below.

   For the sake of backward and future compatibility, the layout only
   ever grows at the end -- a client compiled against an older
   ``multidict`` library version keeps working against a newer
   ``multidict`` runtime, since it only reads the fields it knows
   about. The reverse direction (a client built against a newer
   header than the installed ``multidict`` provides) is guarded by a
   version number stored in the capsule itself: :c:func:`MultiDict_GetCAPI`
   checks it against ``MultiDict_CAPI_VERSION``, the version this header
   was shipped with, and fails instead of reading past the end of an
   older, smaller struct.

istr
====

.. c:function:: PyTypeObject *IStr_GetType(MultiDict_CAPI *capi)

   Return a new reference to the :class:`~multidict.istr` type object.

.. c:function:: int IStr_Check(MultiDict_CAPI *capi, PyObject *op)
                 int IStr_CheckExact(MultiDict_CAPI *capi, PyObject *op)

   Like :c:func:`MultiDict_Check` / :c:func:`MultiDict_CheckExact`, but
   for :class:`~multidict.istr`.

.. c:function:: PyObject *IStr_FromUnicode(MultiDict_CAPI *capi, PyObject *str)

   Return a new reference to an :class:`~multidict.istr` built from
   *str*, or ``NULL`` with an exception set on failure. Equivalent to
   the Python-level ``istr(str)``: if *str* is already an
   :class:`~multidict.istr`, the same object is returned instead of a
   copy. *str* must be a :class:`str` instance (an :class:`~multidict.istr`
   qualifies, being a subclass); anything else raises :exc:`TypeError`.

Version counter
===============

.. c:function:: uint64_t MultiDict_GetVersion(MultiDict_CAPI *capi, PyObject *self)

   Return *self*'s version counter, equivalent to
   :func:`multidict.getversion`. It changes every time *self* is
   mutated, and is unaffected by mutating a *copy* of *self*.

   *self* may be a :class:`~multidict.MultiDict`,
   :class:`~multidict.CIMultiDict`, :class:`~multidict.MultiDictProxy`
   or :class:`~multidict.CIMultiDictProxy` instance; anything else
   raises :exc:`TypeError` and returns ``0``. Since ``0`` is also a
   valid version, check for failure with ``PyErr_Occurred()`` rather
   than the return value alone.

Type objects and type checks
=============================

.. c:function:: PyTypeObject *MultiDict_GetType(MultiDict_CAPI *capi)
                 PyTypeObject *CIMultiDict_GetType(MultiDict_CAPI *capi)
                 PyTypeObject *MultiDictProxy_GetType(MultiDict_CAPI *capi)
                 PyTypeObject *CIMultiDictProxy_GetType(MultiDict_CAPI *capi)

   Return a new reference to the :class:`~multidict.MultiDict`,
   :class:`~multidict.CIMultiDict`, :class:`~multidict.MultiDictProxy`
   or :class:`~multidict.CIMultiDictProxy` type object.

.. c:function:: int MultiDict_Check(MultiDict_CAPI *capi, PyObject *op)
                 int CIMultiDict_Check(MultiDict_CAPI *capi, PyObject *op)
                 int MultiDictProxy_Check(MultiDict_CAPI *capi, PyObject *op)
                 int CIMultiDictProxy_Check(MultiDict_CAPI *capi, PyObject *op)

   Return true if *op* is an instance of the corresponding type or one
   of its subclasses. Since :class:`~multidict.CIMultiDict` is a
   :class:`~multidict.MultiDict` subclass, :c:func:`MultiDict_Check`
   also returns true for a :class:`~multidict.CIMultiDict` instance;
   :c:func:`CIMultiDict_Check` does not return true for a plain
   :class:`~multidict.MultiDict` instance. Likewise for
   :c:func:`MultiDictProxy_Check` / :c:func:`CIMultiDictProxy_Check`.

.. c:function:: int MultiDict_CheckExact(MultiDict_CAPI *capi, PyObject *op)
                 int CIMultiDict_CheckExact(MultiDict_CAPI *capi, PyObject *op)
                 int MultiDictProxy_CheckExact(MultiDict_CAPI *capi, PyObject *op)
                 int CIMultiDictProxy_CheckExact(MultiDict_CAPI *capi, PyObject *op)

   Like the ``_Check`` variants above, but return true only if *op*'s
   type is exactly the corresponding type, not a subclass.

Constructors
============

.. c:function:: PyObject *MultiDict_New(MultiDict_CAPI *capi, Py_ssize_t prealloc_size)
                 PyObject *CIMultiDict_New(MultiDict_CAPI *capi, Py_ssize_t prealloc_size)

   Return a new, empty :class:`~multidict.MultiDict` or
   :class:`~multidict.CIMultiDict` instance, or ``NULL`` with an
   exception set on failure.

   *prealloc_size* is a hint for the number of items the multidict is
   expected to hold; pass ``0`` when there is no such estimate.

.. c:function:: PyObject *MultiDictProxy_New(MultiDict_CAPI *capi, PyObject *arg)

   Return a new :class:`~multidict.MultiDictProxy` wrapping *arg*, or
   ``NULL`` with an exception set on failure. *arg* may be a
   :class:`~multidict.MultiDict`, :class:`~multidict.CIMultiDict`,
   :class:`~multidict.MultiDictProxy` or
   :class:`~multidict.CIMultiDictProxy` instance; anything else raises
   :exc:`TypeError`. This is equivalent to the Python-level
   ``MultiDictProxy(arg)``.

.. c:function:: PyObject *CIMultiDictProxy_New(MultiDict_CAPI *capi, PyObject *arg)

   Same as :c:func:`MultiDictProxy_New`, but returns a
   :class:`~multidict.CIMultiDictProxy` and only accepts a
   :class:`~multidict.CIMultiDict` or
   :class:`~multidict.CIMultiDictProxy` instance for *arg* (a plain
   :class:`~multidict.MultiDict` is rejected with :exc:`TypeError`).

Item access
===========

.. c:function:: Py_ssize_t MultiDict_Size(MultiDict_CAPI *capi, PyObject *self)

   Return the number of items in *self*, equivalent to ``len(self)``.

   *self* may be a :class:`~multidict.MultiDict`,
   :class:`~multidict.CIMultiDict`, :class:`~multidict.MultiDictProxy`
   or :class:`~multidict.CIMultiDictProxy` instance; anything else
   raises :exc:`TypeError` and returns ``-1``.

.. c:function:: int MultiDict_Contains(MultiDict_CAPI *capi, PyObject *self, PyObject *key)

   Return ``1`` if *key* is in *self*, ``0`` if not, or ``-1`` with an
   exception set on failure (including when *self* is not a
   :class:`~multidict.MultiDict`, :class:`~multidict.CIMultiDict`,
   :class:`~multidict.MultiDictProxy` or
   :class:`~multidict.CIMultiDictProxy` instance). Equivalent to
   ``key in self``.

.. c:function:: int MultiDict_GetItem(MultiDict_CAPI *capi, PyObject *self, PyObject *key, PyObject **result)

   Look up the *first* value for *key* in *self*, equivalent to
   ``self[key]``. Return ``1`` and set ``*result`` to a new reference to
   the value if *key* is present; return ``0`` and set ``*result`` to
   ``NULL`` if *key* is absent, *without* setting an exception; or
   return ``-1`` and set ``*result`` to ``NULL`` with an exception set
   on failure (including when *self* is not a
   :class:`~multidict.MultiDict`, :class:`~multidict.CIMultiDict`,
   :class:`~multidict.MultiDictProxy` or
   :class:`~multidict.CIMultiDictProxy` instance). Unlike ``self[key]``,
   a missing key is not by itself an error here.

.. c:function:: int MultiDict_Add(MultiDict_CAPI *capi, PyObject *self, PyObject *key, PyObject *value)

   Append the ``(key, value)`` pair to *self*, equivalent to
   :meth:`~multidict.MultiDict.add`. Return ``0`` on
   success, ``-1`` with an exception set on failure (including when
   *self* is not a :class:`~multidict.MultiDict` instance).

.. c:function:: int MultiDict_Clear(MultiDict_CAPI *capi, PyObject *self)

   Remove all items from *self*, equivalent to
   :meth:`~multidict.MultiDict.clear`. Return ``0``
   on success, ``-1`` with an exception set on failure (including when
   *self* is not a :class:`~multidict.MultiDict` instance).

.. c:function:: int MultiDict_DelItem(MultiDict_CAPI *capi, PyObject *self, PyObject *key)

   Remove every item in *self* whose key is *key*, equivalent to ``del
   self[key]``. Return ``0`` on success, ``-1`` with :exc:`KeyError`
   set if *key* is not in *self*, or with another exception set on
   other failures (including when *self* is not a
   :class:`~multidict.MultiDict` instance).

.. c:function:: int MultiDict_Pop(MultiDict_CAPI *capi, PyObject *self, PyObject *key, PyObject **result)

   Remove *key* from *self*, equivalent to ``self.pop(key)`` with no
   default. Return ``1`` and set ``*result`` to a
   new reference to the removed *first* value if *key* was present;
   return ``0`` and set ``*result`` to ``NULL`` if *key* was absent,
   without setting an exception; or return ``-1`` and set ``*result``
   to ``NULL`` with an exception set on failure (including when *self*
   is not a :class:`~multidict.MultiDict` instance).

.. c:function:: int MultiDict_SetDefault(MultiDict_CAPI *capi, PyObject *self, PyObject *key, PyObject *default_value, PyObject **result)

   Equivalent to ``self.setdefault(key, default_value)``, except
   *default_value* is required here (the Python method defaults it to
   ``None``). If *key* is already in *self*, set ``*result`` to a new
   reference to its *first* value and return ``1``, without touching
   *self*. Otherwise set ``self[key] = default_value``, set
   ``*result`` to a new reference to *default_value*, and return
   ``0``. Return ``-1`` and set ``*result`` to ``NULL`` with an
   exception set on failure (including when *self* is not a
   :class:`~multidict.MultiDict` instance).

.. c:function:: int MultiDict_SetItem(MultiDict_CAPI *capi, PyObject *self, PyObject *key, PyObject *value)

   Replace every item in *self* whose key is *key* with the single
   ``(key, value)`` pair, adding it if *key* is not present.
   Equivalent to ``self[key] = value``. Return ``0`` on success, ``-1``
   with an exception set on failure (including when *self* is not a
   :class:`~multidict.MultiDict` instance).

:c:func:`MultiDict_Size`, :c:func:`MultiDict_Contains` and
:c:func:`MultiDict_GetItem` accept a :class:`~multidict.MultiDict`,
:class:`~multidict.CIMultiDict`, :class:`~multidict.MultiDictProxy` or
:class:`~multidict.CIMultiDictProxy` instance for *self*. The remaining
functions on this page mutate *self* and so only accept a
:class:`~multidict.MultiDict` or :class:`~multidict.CIMultiDict`
instance -- a proxy exposes no mutating methods at the Python level
either, so there is nothing to mutate through one. None of these have a
separate ``CIMultiDict_Contains``, ``CIMultiDict_Add`` and so on:
:class:`~multidict.CIMultiDict` is accepted as a
:class:`~multidict.MultiDict` subclass.

Iteration
=========

.. c:type:: int (*MultiDict_ItemVisitor)(void *user_data, PyObject *key, PyObject *value)

   Callback type for :c:func:`MultiDict_ForEach`.

   Return a positive value to keep the walk going, ``0`` to stop early
   (not an error by itself), or a negative value to abort with an
   error -- a Python exception must already be set in that case.

.. c:function:: Py_ssize_t MultiDict_ForEach(MultiDict_CAPI *capi, PyObject *self, PyObject *key, MultiDict_ItemVisitor visitor, void *user_data)

   Visit items of *self* without building a list. If *key* is
   ``NULL``, call *visitor* once for every ``(key, value)`` pair of
   *self*, in the same order :meth:`~multidict.MultiDict.items` would.
   If *key* is not ``NULL``, call *visitor* only for the entries whose
   key equals *key* -- the same values
   :meth:`~multidict.MultiDict.getall` would return, paired with *key*
   for a uniform callback signature; a missing key visits nothing, it
   is not an error.

   Return the number of items visited (``>= 0``) on success, or ``-1``
   with an exception set on failure (including when *self* is not a
   :class:`~multidict.MultiDict`, :class:`~multidict.CIMultiDict`,
   :class:`~multidict.MultiDictProxy` or
   :class:`~multidict.CIMultiDictProxy` instance).

   *visitor* must not call back into any method on *self* while
   running. The whole walk executes under one internal lock, and for
   the keyed form specifically, reentering *self* would observe
   entries that are temporarily marked while matching duplicate keys
   are being located, which could hide some of them.

Example
=======

::

   #include <multidict_capi.h>

   static int
   module_exec(PyObject *mod)
   {
       my_mod_state *state = PyModule_GetState(mod);
       state->capi = MultiDict_GetCAPI();
       if (state->capi == NULL) {
           return -1;
       }
       return 0;
   }

   static PyObject *
   make_headers(my_mod_state *state)
   {
       PyObject *md = MultiDict_New(state->capi, 2);
       if (md == NULL) {
           return NULL;
       }
       if (MultiDict_Add(state->capi, md, key1, value1) < 0 ||
           MultiDict_Add(state->capi, md, key2, value2) < 0) {
           Py_DECREF(md);
           return NULL;
       }
       return md;
   }

   static int
   print_pair(void *user_data, PyObject *key, PyObject *value)
   {
       PyObject_Print(key, stdout, 0);
       printf(": ");
       PyObject_Print(value, stdout, 0);
       printf("\n");
       return 1;  /* keep going */
   }

   static int
   print_all(my_mod_state *state, PyObject *md)
   {
       /* Visit every item, without building a list. */
       return MultiDict_ForEach(state->capi, md, NULL, print_pair, NULL) < 0
                  ? -1
                  : 0;
   }

   static int
   print_values_for(my_mod_state *state, PyObject *md, PyObject *key)
   {
       /* Visit only the entries for `key`, equivalent to getall(key)
          but without allocating a list. */
       return MultiDict_ForEach(state->capi, md, key, print_pair, NULL) < 0
                  ? -1
                  : 0;
   }
