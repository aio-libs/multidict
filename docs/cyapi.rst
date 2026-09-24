.. _multidict-cyapi:

===========
Cython API
===========

.. highlight:: cython

Third-party Cython code can reach the same :ref:`C API capsule
<multidict-capi>` directly, without going through ``multidict_capi.h``'s
inline wrappers by hand: ``multidict`` ships ``multidict/__init__.pxd``,
declaring the capsule struct and a Cython wrapper for every function from
``multidict_capi.h``, for ``cimport``.

.. versionadded:: 7.0

.. code-block:: cython

   from multidict cimport MultiDict_CAPI, MultiDict_GetCAPI, MultiDict_New, MultiDict_Add

   cdef MultiDict_CAPI *capi = MultiDict_GetCAPI()

   cdef object make_headers():
       md = MultiDict_New(capi, 2)
       MultiDict_Add(capi, md, key1, value1)
       MultiDict_Add(capi, md, key2, value2)
       return md

Every function below has the same name and the same failure semantics
(exceptions raised, sentinel return values) as its C counterpart on the
:ref:`multidict-capi` page, which stays the authoritative reference for
those details; only the Cython signature and a couple of Cython-specific
wrapper behaviors are called out here.

Getting the capsule
=====================

``MultiDict_CAPI`` is an opaque, incomplete ``ctypedef struct`` -- Cython
code never accesses its fields, only passes the pointer through.

``MultiDict_GetCAPI() except NULL`` imports the capsule and returns a
pointer to it, raising :exc:`RuntimeError` (via the ``except NULL`` clause)
on failure -- including a version mismatch against an older ``multidict``
runtime. Call it once (typically at module import time) and keep the
returned pointer around for every other call.

istr
====

- ``IStr_GetType(capi)`` -- returns the :class:`~multidict.istr` type, as
  a plain ``object``.
- ``IStr_CheckExact(capi, op) -> bint`` and ``IStr_Check(capi, op) -> bint``
  -- type checks, mirroring :c:func:`IStr_CheckExact` / :c:func:`IStr_Check`.
- ``IStr_FromUnicode(capi, s) -> object`` -- builds an
  :class:`~multidict.istr` from a :class:`str`.

Version counter
================

- ``MultiDict_GetVersion(capi, self) except? 0 -> uint64_t`` -- *self*'s
  version counter, equivalent to :func:`multidict.getversion`.

Type objects and type checks
==============================

``MultiDict_GetType``, ``CIMultiDict_GetType``, ``MultiDictProxy_GetType``
and ``CIMultiDictProxy_GetType`` each take just *capi* and return the
corresponding type as a plain ``object`` -- for example
``MultiDict_GetType(capi)``.

The C API returns these as ``PyTypeObject *`` (matching the real C
signature: an ``object``-returning extern declaration has to actually
return a ``PyObject *`` at the C level, and ``multidict_capi.h`` genuinely
declares these as ``PyTypeObject *``). The ``.pxd`` wraps each one,
adopting the new reference for you, so callers here get a ready-to-use
``object`` back with no cast and no manual ``Py_DECREF``.

``MultiDict_Check``, ``CIMultiDict_Check``, ``MultiDictProxy_Check`` and
``CIMultiDictProxy_Check`` each take ``(capi, op) -> bint`` and return
whether *op* is an instance of the corresponding type or a subclass.
``MultiDict_CheckExact``, ``CIMultiDict_CheckExact``,
``MultiDictProxy_CheckExact`` and ``CIMultiDictProxy_CheckExact`` are the
exact-type-only equivalents.

Constructors
============

- ``MultiDict_New(capi, prealloc_size) -> object`` and
  ``CIMultiDict_New(capi, prealloc_size) -> object`` -- a new, empty
  :class:`~multidict.MultiDict` or :class:`~multidict.CIMultiDict`.
- ``MultiDictProxy_New(capi, arg) -> object`` and
  ``CIMultiDictProxy_New(capi, arg) -> object`` -- a new proxy wrapping
  *arg*.

Item access
===========

- ``MultiDict_Size(capi, self) except -1 -> Py_ssize_t`` -- ``len(self)``.
- ``MultiDict_Contains(capi, self, key) except -1 -> int`` -- ``key in self``.
- ``MultiDict_GetItem(capi, self, key) -> object`` -- looks up the first
  value for *key*, returning ``(found, value)``: ``(True, value)`` if
  present, ``(False, None)`` if absent. Never raises for a missing key;
  see :c:func:`MultiDict_GetItem` for the underlying ``0``/``1``/``-1`` C
  contract this wraps.
- ``MultiDict_Add(capi, self, key, value) except -1 -> int`` --
  :meth:`~multidict.MultiDict.add`.
- ``MultiDict_Clear(capi, self) except -1 -> int`` --
  :meth:`~multidict.MultiDict.clear`.
- ``MultiDict_DelItem(capi, self, key) except -1 -> int`` -- ``del self[key]``.
- ``MultiDict_Pop(capi, self, key) -> object`` -- ``self.pop(key)`` with no
  default, same ``(found, value)`` return shape as ``MultiDict_GetItem``.
- ``MultiDict_SetDefault(capi, self, key, default_value=None) -> object``
  -- ``self.setdefault(key, default_value)``; returns
  ``(True, existing_value)`` if *key* was already present, or
  ``(False, default_value)`` if it was just inserted.
- ``MultiDict_SetItem(capi, self, key, value) except -1 -> int`` --
  ``self[key] = value``.

``MultiDict_Size``, ``MultiDict_Contains`` and ``MultiDict_GetItem``
accept a :class:`~multidict.MultiDict`, :class:`~multidict.CIMultiDict`,
:class:`~multidict.MultiDictProxy` or :class:`~multidict.CIMultiDictProxy`
instance for *self*. The rest mutate *self* and so only accept a
:class:`~multidict.MultiDict` or :class:`~multidict.CIMultiDict` instance
-- a proxy exposes no mutating methods at the Python level either, so
there is nothing to mutate through one. None of these have a separate
``CIMultiDict_Add``, ``CIMultiDict_Contains`` and so on:
:class:`~multidict.CIMultiDict` is accepted as a
:class:`~multidict.MultiDict` subclass.

Iteration
=========

``MultiDict_ItemVisitor`` is the callback type for
:c:func:`MultiDict_ForEach` below:

.. code-block:: cython

   ctypedef int (*MultiDict_ItemVisitor)(void *user_data, PyObject *identity,
                                         Py_hash_t hash, PyObject *key,
                                         PyObject *value) noexcept

*identity* is the entry's canonical key: the key itself for a
:class:`~multidict.MultiDict`, the internal lower-cased form for a
:class:`~multidict.CIMultiDict`. A visitor can group or compare
entries by it without deriving that form from *key* itself. *hash* is
that identity's hash, the one the mapping stores alongside the entry.

Return a positive value from it to keep the walk going, ``0`` to stop early
(not an error by itself), or a negative value to abort with an error -- a
Python exception must already be set in that case. The typedef is
``noexcept``, so a visitor that wants to raise can't just ``raise``: Cython
would treat that as an unraisable exception in a ``noexcept`` function,
print it, and clear it rather than propagate it. Call ``PyErr_SetString``
(or ``PyErr_Format``/``PyErr_SetObject``, from ``cpython.exc``) directly to
set the exception state, then ``return -1`` yourself.

A visitor's ``identity``/``key``/``value`` parameters are raw
``PyObject *``, not ``object``: Cython does not consider a function
taking ``object`` parameters interchangeable with one taking raw
``PyObject *`` parameters here, even though both compile, so a visitor
must be declared with the same raw parameter types as the typedef
itself. ``hash`` is a plain C ``Py_hash_t``, which needs no cast in
either form.

- ``MultiDict_ForEach(capi, self, key, visitor, user_data) except -1 -> Py_ssize_t``
  -- the low-level entry point, declared here exactly like its C
  counterpart: *key* is a raw ``PyObject *`` where a literal ``NULL``
  means "visit every item" (which has no ``object`` equivalent that
  would not also risk colliding with an actual ``None`` key). Pass
  ``NULL`` directly, or ``<PyObject*>some_key`` for the keyed form.
  Reach for this when code needs to pick between the two cases
  dynamically, or wants to match the C API's signature exactly (for
  example when translating a C example from :ref:`multidict-capi`
  directly); ordinary code should prefer the two functions below.
- ``MultiDict_ForEachAll(capi, self, visitor, user_data) except -1 -> Py_ssize_t``
  -- visits every ``(key, value)`` pair of *self*.
- ``MultiDict_ForEachKey(capi, self, key, visitor, user_data) except -1 -> Py_ssize_t``
  -- visits only the entries for *key*.

*visitor* here is ``MultiDict_CyItemVisitor``, a second callback type
declared alongside ``MultiDict_ItemVisitor``:

.. code-block:: cython

   ctypedef int (*MultiDict_CyItemVisitor)(object identity, Py_hash_t hash,
                                           object key, object value,
                                           void *user_data) except -1

It is still a real ``cdef`` function -- one indirect C call per visited
item, not a Python-level call through an arbitrary callable -- but takes
Cython's own ``object`` type for *identity*/*key*/*value* instead of
``MultiDict_ItemVisitor``'s raw ``PyObject *``, so a visitor needs no
``<object>`` cast of its own. It shares the same three-way return
contract: a positive value keeps the walk going, ``0`` stops early (not
an error), and ``except -1`` reports an exception the visitor itself
raised with an ordinary ``raise`` -- no manual ``PyErr_SetString`` and
``return -1`` required, unlike a ``MultiDict_ItemVisitor``.

All three return the number of items visited (``>= 0``) on success and
execute under one internal lock; *visitor* must not call back into any
method on *self* while running (see :c:func:`MultiDict_ForEach` for why).

Watchers
========

The :ref:`C API's watchers <multidict-capi>` are available unchanged:
:c:func:`MultiDict_AddWatcher`, :c:func:`MultiDict_ClearWatcher`,
:c:func:`MultiDict_Watch` and :c:func:`MultiDict_Unwatch`, together with
the :c:enum:`MultiDict_WatchEvent` enumerators, the
:c:type:`MultiDict_WatchInfo` struct and the
:c:type:`MultiDict_WatchCallback` typedef.

.. versionadded:: 7.0

Writing the callback against the raw typedef has the same wrinkle as
``MultiDict_ItemVisitor``: it is ``noexcept``, so a ``raise`` inside it is
printed and cleared rather than propagated. Report a failure with
``PyErr_SetString(...)`` (or ``PyErr_SetObject(...)``) followed by
``return -1``.

``MultiDict_AddCyWatcher`` wraps that away, the way
``MultiDict_ForEachAll``/``MultiDict_ForEachKey`` wrap the visitor. Unlike
``MultiDict_ForEach``, whose single ``void *`` already belongs to the
caller, ``MultiDict_AddWatcher``'s ``watcher_data`` gives the trampoline
somewhere to keep its own context, so a real Cython callback is possible
here::

   ctypedef int (*MultiDict_CyWatchCallback)(
       void *watcher_data, void *user_data, MultiDict_WatchEvent event,
       PyObject *md, object identity, Py_hash_t hash, object key,
       object value, object old_value) except -1

   cdef struct MultiDict_CyWatcherCtx:
       MultiDict_CyWatchCallback callback
       void *watcher_data

   cdef int MultiDict_AddCyWatcher(MultiDict_CAPI *capi,
                                   MultiDict_CyWatcherCtx *ctx) except -1

*identity*, *key*, *value* and *old_value* arrive as ordinary objects,
``None`` where the C API passes ``NULL``, while *hash* stays the plain
``Py_hash_t`` the C API passes (``-1`` for an event with no key), and the
callback may ``raise``:
``except -1`` carries the exception out to the trampoline, which hands it
back for ``multidict`` to report as unraisable.

*md* stays a raw ``PyObject *`` on purpose, and is the one argument that
does not become an object. On a ``MultiDict_EVENT_DEALLOCATED`` event it
is at refcount 0, and Cython increfs anything typed ``object`` on the way
in, which would resurrect it and then free it twice. Check the event
first and only then cast ``<object>md``; on ``DEALLOCATED`` use the
pointer as an identity and nothing else.

The context is yours to own and keep alive for as long as the watcher
stays registered -- ``MultiDict_AddCyWatcher`` stores the pointer and
allocates nothing. A module-level ``cdef`` is the simple way::

   cdef MultiDict_CyWatcherCtx _ctx

   cdef int on_change(void *watcher_data, void *user_data,
                      MultiDict_WatchEvent event, PyObject *md,
                      object identity, Py_hash_t hash, object key,
                      object value, object old_value) except -1:
       if event == MultiDict_EVENT_DEALLOCATED:
           return 0
       if identity == "content-length":
           raise RuntimeError("reported as unraisable, not propagated")
       return 0

   def register(md, ctx_object):
       _ctx.callback = on_change
       _ctx.watcher_data = <void*>state
       cdef int watcher_id = MultiDict_AddCyWatcher(_capi, &_ctx)
       MultiDict_Watch(_capi, watcher_id, md, <void*>ctx_object)

Worked example
================

``multidict/_testcyapi.pyx`` in the ``multidict`` source tree is a complete
worked example: it mirrors the C test helper (``multidict/_testcapi.c``)
function-for-function purely to exercise this ``.pxd`` from the test suite,
and demonstrates the visitor patterns above (``_collect_pair``,
``_raising_visitor``) end to end.

Building it is optional and never required to install or build
``multidict`` itself -- Cython is never a real build dependency, and an
ordinary install or wheel build never sees it. To build it locally
anyway, install Cython before installing ``multidict`` in editable mode::

   pip install -r requirements/cython.txt
   pip install -e . --no-build-isolation --force-reinstall --no-deps
