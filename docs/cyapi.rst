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
- ``MultiDict_SetDefault(capi, self, key, default_value) -> object`` --
  ``self.setdefault(key, default_value)`` (*default_value* required);
  returns ``(True, existing_value)`` if *key* was already present, or
  ``(False, default_value)`` if it was just inserted.
- ``MultiDict_SetItem(capi, self, key, value) except -1 -> int`` --
  ``self[key] = value``.

All but ``MultiDict_Size`` accept a :class:`~multidict.MultiDict` or
:class:`~multidict.CIMultiDict` instance for *self* -- there is no separate
``CIMultiDict_Add``, ``CIMultiDict_Contains`` and so on.

Iteration
=========

``MultiDict_ItemVisitor`` is the callback type for the ``ForEach``
functions below:

.. code-block:: cython

   ctypedef int (*MultiDict_ItemVisitor)(void *user_data, PyObject *key,
                                         PyObject *value) noexcept

Return a positive value from it to keep the walk going, ``0`` to stop early
(not an error by itself), or a negative value to abort with an error -- a
Python exception must already be set in that case. The typedef is
``noexcept``, so a visitor that wants to raise can't just ``raise``: Cython
would treat that as an unraisable exception in a ``noexcept`` function,
print it, and clear it rather than propagate it. Call ``PyErr_SetString``
(or ``PyErr_Format``/``PyErr_SetObject``, from ``cpython.exc``) directly to
set the exception state, then ``return -1`` yourself.

A visitor's ``key``/``value`` parameters are raw ``PyObject *``, not
``object``: Cython does not consider a function taking ``object``
parameters interchangeable with one taking raw ``PyObject *`` parameters
here, even though both compile, so a visitor must be declared with the
same raw parameter types as the typedef itself.

- ``MultiDict_ForEach(capi, self, key, visitor, user_data) except -1 -> Py_ssize_t``
  -- the low-level entry point, declared here exactly like its C
  counterpart: *key* is a raw ``PyObject *`` where a literal ``NULL``
  means "visit every item" (which has no ``object`` equivalent that
  would not also risk colliding with an actual ``None`` key). Pass
  ``NULL`` directly, or ``<PyObject*>some_key`` for the keyed form.
- ``MultiDict_ForEachAll(capi, self, visitor, user_data) except -1 -> Py_ssize_t``
  -- ``MultiDict_ForEach`` with *key* fixed to ``NULL``: visits every
  ``(key, value)`` pair of *self*, no raw pointer needed.
- ``MultiDict_ForEachKey(capi, self, key, visitor, user_data) except -1 -> Py_ssize_t``
  -- ``MultiDict_ForEach`` with *key* taken as a plain ``object``: visits
  only the entries for *key*.

All three take a raw-pointer ``visitor``/``user_data`` pair and return the
number of items visited (``>= 0``) on success. Prefer ``ForEachAll``/
``ForEachKey`` for ordinary use; drop to the raw ``MultiDict_ForEach``
only when code needs to pick between the two cases dynamically, or wants
to match the C API's signature exactly (for example when translating a
C example from :ref:`multidict-capi` directly).

For callers who would rather pass an ordinary Python callable than write
a raw-pointer visitor function, two more entry points wrap ``ForEachAll``/
``ForEachKey`` with a small trampoline:

- ``MultiDict_ForEachAllPy(capi, self, callback) except -1 -> Py_ssize_t``
- ``MultiDict_ForEachKeyPy(capi, self, key, callback) except -1 -> Py_ssize_t``

*callback* is an ordinary ``(key, value) -> bool``-ish callable -- any
truthy/falsy return decides continue/stop, and a raised exception
propagates out of ``ForEachAllPy``/``ForEachKeyPy`` as a genuine Python
exception, no manual ``PyErr_SetString`` and ``return -1`` required. The
trade-off is one Python call per visited item instead of a raw C
callback; reach for the raw ``visitor`` forms above when that matters.

All five execute under one internal lock; *visitor* (or *callback*) must
not call back into any method on *self* while running (see
:c:func:`MultiDict_ForEach` for why).

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
