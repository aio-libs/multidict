.. _multidict-cyapi:

===========
Cython API
===========

.. highlight:: cython

Third-party Cython code can reach the same :ref:`C API capsule
<multidict-capi>` directly, without going through ``multidict_capi.h``'s
inline wrappers by hand: ``multidict`` ships ``multidict/__init__.pxd``,
declaring the capsule struct and every wrapper function from
``multidict_capi.h`` for ``cimport``.

.. code-block:: cython

   from multidict cimport capi, MultiDict_New, MultiDict_Add

   cdef object make_headers():
       md = MultiDict_New(capi(), 2)
       MultiDict_Add(capi(), md, key1, value1)
       MultiDict_Add(capi(), md, key2, value2)
       return md

Every function documented in :ref:`multidict-capi` is declared here under
the same name, with the same semantics; that page stays the authoritative
reference. A few Cython-specific notes:

- Passing the same ``MultiDict_CAPI *`` to every single call gets tedious,
  so the ``.pxd`` also declares ``capi()``: it imports ``multidict`` and
  calls :c:func:`MultiDict_GetCAPI` on first use, caches the result, and
  returns the cached pointer on every later call -- once per compiled
  extension module that ``cimport``\ s it, not once per call. Prefer it
  over calling :c:func:`MultiDict_GetCAPI` and threading the result
  through by hand, unless you specifically need to control when the first
  call (and thus the version check it performs) happens.
- Functions returning a type object (``MultiDict_GetType`` and friends) are
  declared as returning ``PyTypeObject *``, matching their real C
  signature, not ``object`` -- Cython does not let an ``object``-returning
  extern declaration have a return type more specific than plain
  ``PyObject *``. Wrap the result in ``<object>`` (and account for the new
  reference it carries) to get a usable Python object back.
- ``MultiDict_ForEach``'s ``key`` parameter is declared as a raw
  ``PyObject *``, not ``object``: the C contract uses a literal ``NULL`` to
  mean "visit every item", which has no `object` equivalent that would not
  also risk colliding with an actual ``None`` key. Pass ``NULL`` directly,
  or ``<PyObject*>some_key`` for the keyed form.
- A ``MultiDict_ItemVisitor`` passed to ``MultiDict_ForEach`` must be
  declared with the same raw ``PyObject *key, PyObject *value`` parameters
  as the typedef itself (not ``object``) -- Cython does not consider a
  function taking ``object`` parameters interchangeable with one taking raw
  ``PyObject *`` parameters for this purpose, even though both compile.

``multidict/_testcyapi.pyx`` in the ``multidict`` source tree is a complete
worked example: it mirrors the C test helper (``multidict/_testcapi.c``)
function-for-function purely to exercise this ``.pxd`` from the test suite,
and demonstrates the reference-counting patterns above, including safely
adopting a new reference out of a ``PyObject **result`` out-parameter
(``MultiDict_GetItem``, ``MultiDict_Pop``, ``MultiDict_SetDefault``).

Building it is optional and never required to install or build
``multidict`` itself: see ``AGENTS.md``'s "Public C API" section in the
source repository for how to opt in locally.
