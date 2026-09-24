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

Thread safety
=============

Most functions below carry a **Thread safety** note, using the same two
guarantees `CPython's own thread safety documentation
<https://docs.python.org/3/builtins/threadsafety.html#threadsafety-levels>`_
uses for :class:`dict`:

- **Atomic** -- either the call touches no mutable state shared with
  any other :class:`~multidict.MultiDict` instance (a type check, a
  type object lookup, or a freshly allocated object nothing else can
  see yet), or it is a single relaxed atomic read of one already-
  consistent counter on *self*. Either way it needs no lock.
- **Safe for concurrent use on the same object** -- the call reads or
  mutates *self*'s own contents through more than a single atomic
  access. Concurrent calls on the *same* instance, from any threads,
  cannot corrupt it or crash, whether through a lock-free read path or
  *self*'s own internal critical section.

istr
====

.. c:function:: PyTypeObject *IStr_GetType(MultiDict_CAPI *capi)

   **Thread safety:** Atomic.

   Return a new reference to the :class:`~multidict.istr` type object.

.. c:function:: int IStr_Check(MultiDict_CAPI *capi, PyObject *op)
                 int IStr_CheckExact(MultiDict_CAPI *capi, PyObject *op)

   **Thread safety:** Atomic.

   Like :c:func:`MultiDict_Check` / :c:func:`MultiDict_CheckExact`, but
   for :class:`~multidict.istr`.

.. c:function:: PyObject *IStr_FromUnicode(MultiDict_CAPI *capi, PyObject *str)

   **Thread safety:** Atomic.

   Return a new reference to an :class:`~multidict.istr` built from
   *str*, or ``NULL`` with an exception set on failure. Equivalent to
   the Python-level ``istr(str)``: if *str* is already an
   :class:`~multidict.istr`, the same object is returned instead of a
   copy. *str* must be a :class:`str` instance (an :class:`~multidict.istr`
   qualifies, being a subclass); anything else raises :exc:`TypeError`.

Version counter
===============

.. c:function:: uint64_t MultiDict_GetVersion(MultiDict_CAPI *capi, PyObject *self)

   **Thread safety:** Atomic. A lock-free relaxed read of *self*'s
   version counter, same as :c:func:`MultiDict_Size`.

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

   **Thread safety:** Atomic.

   Return a new reference to the :class:`~multidict.MultiDict`,
   :class:`~multidict.CIMultiDict`, :class:`~multidict.MultiDictProxy`
   or :class:`~multidict.CIMultiDictProxy` type object.

.. c:function:: int MultiDict_Check(MultiDict_CAPI *capi, PyObject *op)
                 int CIMultiDict_Check(MultiDict_CAPI *capi, PyObject *op)
                 int MultiDictProxy_Check(MultiDict_CAPI *capi, PyObject *op)
                 int CIMultiDictProxy_Check(MultiDict_CAPI *capi, PyObject *op)

   **Thread safety:** Atomic.

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

   **Thread safety:** Atomic.

   Like the ``_Check`` variants above, but return true only if *op*'s
   type is exactly the corresponding type, not a subclass.

Constructors
============

.. c:function:: PyObject *MultiDict_New(MultiDict_CAPI *capi, Py_ssize_t prealloc_size)
                 PyObject *CIMultiDict_New(MultiDict_CAPI *capi, Py_ssize_t prealloc_size)

   **Thread safety:** Atomic.

   Return a new, empty :class:`~multidict.MultiDict` or
   :class:`~multidict.CIMultiDict` instance, or ``NULL`` with an
   exception set on failure.

   *prealloc_size* is a hint for the number of items the multidict is
   expected to hold; pass ``0`` when there is no such estimate.

.. c:function:: PyObject *MultiDictProxy_New(MultiDict_CAPI *capi, PyObject *arg)

   **Thread safety:** Atomic. The only touch on *arg* itself is a new
   reference (an atomic refcount increment); *arg*'s hash table is
   never read.

   Return a new :class:`~multidict.MultiDictProxy` wrapping *arg*, or
   ``NULL`` with an exception set on failure. *arg* may be a
   :class:`~multidict.MultiDict`, :class:`~multidict.CIMultiDict`,
   :class:`~multidict.MultiDictProxy` or
   :class:`~multidict.CIMultiDictProxy` instance; anything else raises
   :exc:`TypeError`. This is equivalent to the Python-level
   ``MultiDictProxy(arg)``.

.. c:function:: PyObject *CIMultiDictProxy_New(MultiDict_CAPI *capi, PyObject *arg)

   **Thread safety:** Atomic, for the same reason as
   :c:func:`MultiDictProxy_New`.

   Same as :c:func:`MultiDictProxy_New`, but returns a
   :class:`~multidict.CIMultiDictProxy` and only accepts a
   :class:`~multidict.CIMultiDict` or
   :class:`~multidict.CIMultiDictProxy` instance for *arg* (a plain
   :class:`~multidict.MultiDict` is rejected with :exc:`TypeError`).

Item access
===========

.. c:function:: Py_ssize_t MultiDict_Size(MultiDict_CAPI *capi, PyObject *self)

   **Thread safety:** Atomic. A lock-free relaxed read of *self*'s item
   count.

   Return the number of items in *self*, equivalent to ``len(self)``.

   *self* may be a :class:`~multidict.MultiDict`,
   :class:`~multidict.CIMultiDict`, :class:`~multidict.MultiDictProxy`
   or :class:`~multidict.CIMultiDictProxy` instance; anything else
   raises :exc:`TypeError` and returns ``-1``.

.. c:function:: int MultiDict_Contains(MultiDict_CAPI *capi, PyObject *self, PyObject *key)

   **Thread safety:** Safe for concurrent use on the same object.

   Return ``1`` if *key* is in *self*, ``0`` if not, or ``-1`` with an
   exception set on failure (including when *self* is not a
   :class:`~multidict.MultiDict`, :class:`~multidict.CIMultiDict`,
   :class:`~multidict.MultiDictProxy` or
   :class:`~multidict.CIMultiDictProxy` instance). Equivalent to
   ``key in self``.

.. c:function:: int MultiDict_GetItem(MultiDict_CAPI *capi, PyObject *self, PyObject *key, PyObject **result)

   **Thread safety:** Safe for concurrent use on the same object.

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

   **Thread safety:** Safe for concurrent use on the same object.

   Append the ``(key, value)`` pair to *self*, equivalent to
   :meth:`~multidict.MultiDict.add`. Return ``0`` on
   success, ``-1`` with an exception set on failure (including when
   *self* is not a :class:`~multidict.MultiDict` instance).

.. c:function:: int MultiDict_Clear(MultiDict_CAPI *capi, PyObject *self)

   **Thread safety:** Safe for concurrent use on the same object.

   Remove all items from *self*, equivalent to
   :meth:`~multidict.MultiDict.clear`. Return ``0``
   on success, ``-1`` with an exception set on failure (including when
   *self* is not a :class:`~multidict.MultiDict` instance).

.. c:function:: int MultiDict_DelItem(MultiDict_CAPI *capi, PyObject *self, PyObject *key)

   **Thread safety:** Safe for concurrent use on the same object.

   Remove every item in *self* whose key is *key*, equivalent to ``del
   self[key]``. Return ``0`` on success, ``-1`` with :exc:`KeyError`
   set if *key* is not in *self*, or with another exception set on
   other failures (including when *self* is not a
   :class:`~multidict.MultiDict` instance).

.. c:function:: int MultiDict_Pop(MultiDict_CAPI *capi, PyObject *self, PyObject *key, PyObject **result)

   **Thread safety:** Safe for concurrent use on the same object.

   Remove *key* from *self*, equivalent to ``self.pop(key)`` with no
   default. Return ``1`` and set ``*result`` to a
   new reference to the removed *first* value if *key* was present;
   return ``0`` and set ``*result`` to ``NULL`` if *key* was absent,
   without setting an exception; or return ``-1`` and set ``*result``
   to ``NULL`` with an exception set on failure (including when *self*
   is not a :class:`~multidict.MultiDict` instance).

.. c:function:: int MultiDict_SetDefault(MultiDict_CAPI *capi, PyObject *self, PyObject *key, PyObject *default_value, PyObject **result)

   **Thread safety:** Safe for concurrent use on the same object.

   Equivalent to ``self.setdefault(key, default_value)``.
   *default_value* may be ``NULL``, which is read as ``None``, matching
   the default of the Python method. If *key* is already in *self*, set
   ``*result`` to a new
   reference to its *first* value and return ``1``, without touching
   *self*. Otherwise set ``self[key] = default_value``, set
   ``*result`` to a new reference to *default_value*, and return
   ``0``. Return ``-1`` and set ``*result`` to ``NULL`` with an
   exception set on failure (including when *self* is not a
   :class:`~multidict.MultiDict` instance).

.. c:function:: int MultiDict_SetItem(MultiDict_CAPI *capi, PyObject *self, PyObject *key, PyObject *value)

   **Thread safety:** Safe for concurrent use on the same object.

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

.. c:type:: int (*MultiDict_ItemVisitor)(void *user_data, PyObject *identity, Py_hash_t hash, PyObject *key, PyObject *value)

   Callback type for :c:func:`MultiDict_ForEach`.

   *identity* is the entry's canonical key: the key itself for a
   :class:`~multidict.MultiDict`, the internal lower-cased form for a
   :class:`~multidict.CIMultiDict`. It is what the mapping actually
   looks entries up by, so a visitor can group or compare entries
   without deriving that form from *key* itself.

   *hash* is *identity*'s hash, the one the mapping stores alongside
   the entry, so a visitor bucketing entries of its own does not have
   to hash *identity* again.

   *identity*, *key* and *value* are borrowed references, kept alive
   for the duration of the call.

   Return a positive value to keep the walk going, ``0`` to stop early
   (not an error by itself), or a negative value to abort with an
   error -- a Python exception must already be set in that case.

.. c:function:: Py_ssize_t MultiDict_ForEach(MultiDict_CAPI *capi, PyObject *self, PyObject *key, MultiDict_ItemVisitor visitor, void *user_data)

   **Thread safety:** Safe for concurrent use on the same object. The
   whole walk, including every *visitor* call, runs under *self*'s
   internal critical section; see the note on reentrancy below for the
   one thing that guarantee does not cover.

   Visit items of *self* without building a list. If *key* is
   ``NULL``, call *visitor* once for every ``(key, value)`` pair of
   *self*, in the same order :meth:`~multidict.MultiDict.items` would.
   If *key* is not ``NULL``, call *visitor* only for the entries whose
   key equals *key* -- the same values
   :meth:`~multidict.MultiDict.getall` would return, paired with *key*
   for a uniform callback signature; a missing key visits nothing, it
   is not an error. In that form the *identity* and *hash* passed to
   *visitor* are the ones computed from the *key* argument, which
   compare equal to every visited entry's own.

   Return the number of items visited (``>= 0``) on success, or ``-1``
   with an exception set on failure (including when *self* is not a
   :class:`~multidict.MultiDict`, :class:`~multidict.CIMultiDict`,
   :class:`~multidict.MultiDictProxy` or
   :class:`~multidict.CIMultiDictProxy` instance).

   *visitor* must not call back into any method on *self* while
   running. The whole walk executes under one internal lock, and the
   walk itself compares a version stamped at its start against
   *self*'s current version on every step, raising a
   :exc:`RuntimeError` the moment a reentrant mutation changes it,
   rather than silently corrupting or hiding results.

Watchers
========

A watcher is a callback ``multidict`` calls whenever a multidict it is
watching changes. It is modeled on CPython's :c:func:`!PyDict_AddWatcher`
family, with two deliberate differences.

The first is why the API exists: the callback receives **two context
pointers**. ``watcher_data`` is fixed when the callback is registered, and
``user_data`` belongs to one watched multidict. One registered callback can
therefore serve any number of multidicts and still know, on every event,
what owns the one that changed -- a server response and its headers, say.

The second is **when** events arrive. CPython calls a dict watcher in the
middle of the mutation; ``multidict`` records the events instead and
delivers them once the operation has finished and every internal lock on
the multidict has been released. So a callback sees a fully consistent
multidict and may read it freely, but it is called after the fact rather
than at the moment of the change. Events are delivered in the order they
happened, and everything delivered in one burst belongs to one operation.

At most ``MULTIDICT_MAX_WATCHERS`` (8) watchers can be registered at a
time, matching CPython's limit.

.. versionadded:: 7.0

.. c:macro:: MULTIDICT_MAX_WATCHERS

   The number of watcher slots, ``8``.

.. c:enum:: MultiDict_WatchEvent

   What happened. ``multidict`` is a multi-value mapping, so CPython's
   five dict events do not carry over unchanged: ``add()`` on a key that
   is already present is an append rather than a modification, and a
   single ``__setitem__`` can both replace one pair and delete several.

   .. c:enumerator:: MultiDict_EVENT_ADDED

      A ``(key, value)`` pair was appended. Fired by
      :meth:`~multidict.MultiDict.add`, by each pair of
      :meth:`~multidict.MultiDict.extend`, by the pairs
      :meth:`~multidict.MultiDict.merge` actually adds, by
      :meth:`~multidict.MultiDict.setdefault` when the key was absent,
      and by ``self[key] = value`` when the key was absent.

   .. c:enumerator:: MultiDict_EVENT_REPLACED

      A pair's value was overwritten in place, keeping the pair's
      position. Fired by ``self[key] = value`` and by
      :meth:`~multidict.MultiDict.update` for the first match of a key
      that is already present. ``old_value`` carries the displaced
      value.

   .. c:enumerator:: MultiDict_EVENT_DELETED

      One pair was removed, with ``value`` carrying the removed value --
      CPython passes ``NULL`` here, but a multi-value mapping's caller
      usually needs to know which value went. Fired by ``del self[key]``
      and :meth:`~multidict.MultiDict.popall` once per removed pair, by
      :meth:`~multidict.MultiDict.popone`,
      :meth:`~multidict.MultiDict.pop` and
      :meth:`~multidict.MultiDict.popitem`, and for the further
      occurrences ``self[key] = value`` and
      :meth:`~multidict.MultiDict.update` drop.

   .. c:enumerator:: MultiDict_EVENT_CLEARED

      :meth:`~multidict.MultiDict.clear` emptied a non-empty multidict,
      or ``__init__()`` was called again on a live one. One event, not
      one per pair, the same choice CPython makes.

   .. c:enumerator:: MultiDict_EVENT_CLONED

      The contents were replaced wholesale by those of another
      multidict of the same kind, by calling ``__init__()`` on a live
      multidict. Means "resynchronize from scratch".

   .. c:enumerator:: MultiDict_EVENT_DEALLOCATED

      The multidict is being deallocated. ``self`` is at refcount zero:
      use it as an identity and nothing else. Do not incref it and do
      not pass it to any function on this page.

   .. c:enumerator:: MultiDict_EVENT_BATCH_BEGIN
   .. c:enumerator:: MultiDict_EVENT_BATCH_END

      Bracket the events of one operation that can touch several pairs:
      ``self[key] = value``, ``del self[key]``,
      :meth:`~multidict.MultiDict.extend`,
      :meth:`~multidict.MultiDict.update`,
      :meth:`~multidict.MultiDict.merge`,
      :meth:`~multidict.MultiDict.popall` and ``__init__()``. A watcher
      maintaining something derived from the multidict can rebuild it
      once per bracket instead of once per pair. There is no CPython
      analogue; :class:`dict` has no bulk operation that reports detail.

      Brackets never nest. An operation that emits one emits both, but
      a watcher only ever sees the events delivered to *it*, and
      :c:func:`MultiDict_Watch` and :c:func:`MultiDict_Unwatch` take
      effect from the next event on, even when called from inside a
      callback. A watcher that starts watching from within a bracket
      therefore sees that bracket's ``BATCH_END`` with no
      ``BATCH_BEGIN``, and one that unwatches from within a bracket sees
      the ``BATCH_BEGIN`` with no ``BATCH_END``. Treat an unmatched end
      as a signal to rebuild from *self*, or begin watching from outside
      a callback, where no bracket can be open.

      ``del self[key]`` and :meth:`~multidict.MultiDict.popall` emit a
      bracket only when they actually removed something; the rest emit
      one whether or not anything changed.

   .. c:enumerator:: MultiDict_EVENT_LOST

      Recording ran out of memory, so some events were dropped and the
      stream no longer describes what changed. The mutation itself still
      happened; resynchronize from the multidict. This replaces the
      whole burst it belongs to, rather than being mixed into it.

.. c:type:: MultiDict_WatchInfo

   What an event carries. Fields are only ever appended to the end of
   this struct, under the same rule as :c:type:`MultiDict_CAPI`, so a
   client built against an older header keeps working.

   .. c:member:: MultiDict_WatchEvent event

      Which event this is.

   .. c:member:: PyObject *self

      The multidict that changed.

   .. c:member:: PyObject *identity

      The canonical form ``multidict`` looks keys up by: always an exact
      :class:`str`, never an :class:`~multidict.istr`, and the lowercased
      key for a :class:`~multidict.CIMultiDict`. This is the field to
      compare against -- matching ``"content-length"`` needs no
      :meth:`~str.lower` of your own. ``NULL`` for the events that do not
      concern one key.

   .. c:member:: Py_hash_t hash

      The hash of :c:member:`~MultiDict_WatchInfo.identity`, the one
      ``multidict`` looked the entry up by, so a watcher keeping its own
      table can reuse it instead of hashing the key again. ``-1``, which
      no Python hash ever is, for the events that carry no key.

   .. c:member:: PyObject *key

      The key as stored. On a :class:`~multidict.CIMultiDict` this may be
      a plain :class:`str` where :meth:`~multidict.MultiDict.keys` would
      yield an :class:`~multidict.istr`: building the
      :class:`~multidict.istr` can run Python code, which recording an
      event must not do. Branch on :c:member:`~MultiDict_WatchInfo.identity`,
      not on this. ``NULL`` where *identity* is.

   .. c:member:: PyObject *value

      The new value for ``ADDED`` and ``REPLACED``, the removed value for
      ``DELETED``, ``NULL`` otherwise.

   .. c:member:: PyObject *old_value

      The displaced value for ``REPLACED``, ``NULL`` otherwise.

   Every :c:expr:`PyObject *` above is borrowed and valid only for the
   duration of the call.

.. c:type:: int (*MultiDict_WatchCallback)(void *watcher_data, void *user_data, const MultiDict_WatchInfo *info)

   Callback type for :c:func:`MultiDict_AddWatcher`.

   Return ``0`` on success, or ``-1`` with a Python exception set on
   failure. There is deliberately no "stop early" value: unlike a
   :c:type:`MultiDict_ItemVisitor`, the callback drives no walk.

   A failure cannot be propagated, because the mutation it describes has
   already happened and cannot be undone. ``multidict`` reports it with
   `PyErr_WriteUnraisable()
   <https://docs.python.org/3/c-api/exceptions.html#c.PyErr_WriteUnraisable>`_
   and carries on delivering the remaining events. This is CPython's rule
   for dict watchers too.

   The callback runs with no lock on *self* held, so it **may** read
   *self*: :c:func:`MultiDict_Size`, :c:func:`MultiDict_GetItem` and
   :c:func:`MultiDict_ForEach` are all safe on it. This is the opposite
   of the rule for a :c:type:`MultiDict_ItemVisitor`, which runs mid-walk
   under the lock. The callback **may** also mutate *self* without
   deadlocking, but should not as a matter of course: the events that
   produces are queued and delivered to the same callback before the
   current delivery returns, so a callback that mutates on every event
   will not terminate.

.. c:function:: int MultiDict_AddWatcher(MultiDict_CAPI *capi, MultiDict_WatchCallback callback, void *watcher_data)

   **Thread safety:** not safe against a concurrent
   :c:func:`MultiDict_AddWatcher` or :c:func:`MultiDict_ClearWatcher`.
   Register during module initialization, before the watcher can fire.

   Register *callback*, to be passed *watcher_data* on every event.
   Return a watcher ID in ``[0, MULTIDICT_MAX_WATCHERS)``, or ``-1`` with
   an exception set on failure -- :exc:`RuntimeError` when all slots are
   taken, :exc:`ValueError` when *callback* is ``NULL``.

   Watcher IDs are per-interpreter, like CPython's, so a client
   supporting :pep:`684` per-interpreter GIL registers once per
   interpreter.

   ``multidict`` never increfs, decrefs or frees *watcher_data*; its
   lifetime is entirely yours, and it must outlive the registration.

.. c:function:: int MultiDict_ClearWatcher(MultiDict_CAPI *capi, int watcher_id)

   **Thread safety:** see :c:func:`MultiDict_AddWatcher`.

   Free the slot *watcher_id* occupies. Return ``0`` on success, ``-1``
   with :exc:`ValueError` set when *watcher_id* was never registered or
   is out of range.

   Multidicts still being watched by *watcher_id* are **not** visited:
   nothing enumerates them. Their watch bit stays set and resolves to the
   now-empty slot, so nothing is delivered while that slot is free.
   CPython's :c:func:`!PyDict_ClearWatcher` behaves the same way.

   .. warning::

      A later :c:func:`MultiDict_AddWatcher` hands the same ID out
      again, and any multidict left carrying the bit then reports to the
      **new** callback, carrying the **old** *user_data*. ``multidict``
      does not own that pointer, so by then it may be freed.

      Register once during module initialization and leave the watcher
      registered for the life of the interpreter, which is what CPython
      recommends for dict watchers too. If you do clear a watcher,
      :c:func:`MultiDict_Unwatch` every multidict you watched first, or
      be certain that none of them is still alive.

.. c:function:: int MultiDict_Watch(MultiDict_CAPI *capi, int watcher_id, PyObject *self, void *user_data)

   **Thread safety:** Safe for concurrent use on the same object.

   Start reporting *self*'s changes to *watcher_id*'s callback, which
   will be passed *user_data* on every one of *self*'s events. Return
   ``0`` on success, ``-1`` with an exception set on failure.

   Watching again with a different *user_data* replaces it.

   The watch takes effect immediately, so calling this from inside a
   callback starts delivery with the very next event, which may be one
   the operation in progress has already recorded. See
   :c:enumerator:`MultiDict_EVENT_BATCH_BEGIN` for what that means for
   bracket pairing.

   *self* may be a :class:`~multidict.MultiDict`,
   :class:`~multidict.CIMultiDict`, :class:`~multidict.MultiDictProxy` or
   :class:`~multidict.CIMultiDictProxy` instance: watching is an
   observation, not a mutation. A proxy watches the multidict underneath
   it, so changes made through the original object, or through any other
   proxy of it, are reported too.

   ``multidict`` never increfs, decrefs or frees *user_data*. It must
   outlive the watch, so either unwatch before releasing it or treat
   ``MultiDict_EVENT_DEALLOCATED`` as the signal to release it. Because
   ``multidict`` holds no reference, a *user_data* that points at a
   Python object is invisible to the cycle collector and creates no cycle
   of its own.

.. c:function:: int MultiDict_Unwatch(MultiDict_CAPI *capi, int watcher_id, PyObject *self)

   **Thread safety:** Safe for concurrent use on the same object.

   Stop reporting *self*'s changes to *watcher_id*. Return ``0`` on
   success, ``-1`` with an exception set on failure. Unwatching a
   multidict that is not being watched succeeds and does nothing.

   *self* is resolved as in :c:func:`MultiDict_Watch`, so unwatching
   through a different proxy of the same multidict removes the same
   watch.

   Like :c:func:`MultiDict_Watch`, this takes effect immediately: a
   callback that unwatches receives nothing further, not even the
   remaining events of the operation being delivered, so *user_data* can
   be released as soon as it returns.

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
   print_pair(void *user_data, PyObject *identity, Py_hash_t hash,
              PyObject *key, PyObject *value)
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

Watching a multidict
--------------------

The shape the two context pointers are there for: one registered
callback, many watched multidicts, each carrying whatever owns it.

::

   #include <multidict_capi.h>

   typedef struct {
       PyObject *headers;
       Py_ssize_t content_length;
   } response;

   static int
   on_header_change(void *watcher_data, void *user_data,
                    const MultiDict_WatchInfo *info)
   {
       my_mod_state *state = (my_mod_state *)watcher_data;
       response *resp = (response *)user_data;

       if (info->event == MultiDict_EVENT_DEALLOCATED) {
           /* `info->self` is at refcount 0 here: identity only. */
           resp->headers = NULL;
           return 0;
       }
       if (info->identity == NULL) {
           /* CLEARED, CLONED, LOST or a batch bracket: recompute. */
           return recompute(state, resp);
       }
       /* `identity` is already lowercased for a CIMultiDict. */
       if (PyUnicode_CompareWithASCIIString(info->identity,
                                            "content-length") == 0) {
           return recompute(state, resp);
       }
       return 0;
   }

   static int
   module_exec(PyObject *mod)
   {
       my_mod_state *state = PyModule_GetState(mod);
       state->capi = MultiDict_GetCAPI();
       if (state->capi == NULL) {
           return -1;
       }
       state->watcher_id =
           MultiDict_AddWatcher(state->capi, on_header_change, state);
       return state->watcher_id < 0 ? -1 : 0;
   }

   static int
   response_init(my_mod_state *state, response *resp)
   {
       resp->headers = CIMultiDict_New(state->capi, 8);
       if (resp->headers == NULL) {
           return -1;
       }
       /* Every change to *these* headers arrives with *this* response. */
       return MultiDict_Watch(state->capi, state->watcher_id,
                              resp->headers, resp);
   }
