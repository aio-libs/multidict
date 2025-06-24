=========
Changelog
=========

..
    You should *NOT* be adding new change log entries to this file, this
    file is managed by towncrier. You *may* edit previous change logs to
    fix problems like typo corrections or such.
    To add a new change log entry, please see
    https://pip.pypa.io/en/latest/development/#adding-a-news-entry
    we named the news folder "changes".

    WARNING: Don't drop the next directive!

.. towncrier release notes start

6.5.1
=====

*(2025-06-24)*


Bug fixes
---------

- Fixed a bug in C implementation when multidict is resized and it has deleted slots.

  The bug was introduced by multidict 6.5.0 release.

  Patch by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1195`.


Contributor-facing changes
--------------------------

- A pair of code formatters for Python and C have been configured in the pre-commit tool.

  *Related issues and pull requests on GitHub:*
  :issue:`1123`.

- Shorted fixture parametrization ids.

  For example, ``test_keys_view_xor[case-insensitive-pure-python-module]`` becomes ``test_keys_view_xor[ci-py]`` -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1192`.

- The :file:`reusable-cibuildwheel.yml` workflow has been refactored to
  be more generic and :file:`ci-cd.yml` now holds all the configuration
  toggles -- by :user:`webknjaz`.

  *Related issues and pull requests on GitHub:*
  :issue:`1193`.


----


6.5.0
=====

*(2025-06-17)*

.. note::

  The release was yanked because of :issue:`1195`, multidict 6.5.1 should be used
  instead.


Features
--------

- Replace internal implementation from an array of items to hash table.
  algorithmic complexity for lookups is switched from O(N) to O(1).

  The hash table is very similar to :class:`dict` from CPython but it allows keys duplication.

  The benchmark shows 25-50% boost for single lookups, x2-x3 for bulk updates, and x20 for
  some multidict view operations.  The gain is not for free:
  :class:`~multidict.MultiDict.add` and :class:`~multidict.MultiDict.extend` are 25-50%
  slower now. We consider it as acceptable because the lookup is much more common
  operation that addition for the library domain.

  *Related issues and pull requests on GitHub:*
  :issue:`1128`.


Contributor-facing changes
--------------------------

- Builds have been added for arm64 Windows
  wheels and the ``reusable-build-wheel.yml``
  template has been modified to allow for
  an os value (``windows-11-arm``) which
  does not end with the ``-latest`` postfix.

  *Related issues and pull requests on GitHub:*
  :issue:`1167`.


----


6.4.4
=====

*(2025-05-19)*


Bug fixes
---------

- Fixed a segmentation fault when calling :py:meth:`multidict.MultiDict.setdefault` with a single argument -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1160`.

- Fixed a segmentation fault when attempting to directly instantiate view objects
  (``multidict._ItemsView``, ``multidict._KeysView``, ``multidict._ValuesView``) -- by :user:`bdraco`.

  View objects now raise a proper :exc:`TypeError` with the message "cannot create '...' instances directly"
  when direct instantiation is attempted.

  View objects should only be created through the proper methods: :py:meth:`multidict.MultiDict.items`,
  :py:meth:`multidict.MultiDict.keys`, and :py:meth:`multidict.MultiDict.values`.

  *Related issues and pull requests on GitHub:*
  :issue:`1164`.


Miscellaneous internal changes
------------------------------

- :class:`multidict.MultiDictProxy` was refactored to rely only on
  :class:`multidict.MultiDict` public interface and don't touch any implementation
  details.

  *Related issues and pull requests on GitHub:*
  :issue:`1150`.

- Multidict views were refactored to rely only on
  :class:`multidict.MultiDict` API and don't touch any implementation
  details.

  *Related issues and pull requests on GitHub:*
  :issue:`1152`.

- Dropped internal ``_Impl`` class from pure Python implementation, both pure Python and C
  Extension follows the same design internally now.

  *Related issues and pull requests on GitHub:*
  :issue:`1153`.


----


6.4.3
=====

*(2025-04-10)*


Bug fixes
---------

- Fixed building the library in debug mode.

  *Related issues and pull requests on GitHub:*
  :issue:`1144`.

- Fixed custom ``PyType_GetModuleByDef()`` when non-heap type object was passed.

  *Related issues and pull requests on GitHub:*
  :issue:`1147`.


Packaging updates and notes for downstreams
-------------------------------------------

- Added the ability to build in debug mode by setting :envvar:`MULTIDICT_DEBUG_BUILD` in the environment -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1145`.


----


6.4.2
=====

*(2025-04-09)*


Bug fixes
---------

- Fixed a segmentation fault when creating subclassed :py:class:`~multidict.MultiDict` objects on Python < 3.11 -- by :user:`bdraco`.

  The problem first appeared in 6.4.0

  *Related issues and pull requests on GitHub:*
  :issue:`1141`.


----


6.4.1
=====

*(2025-04-09)*


No significant changes.


----


6.4.0
=====

*(2025-04-09)*


Bug fixes
---------

- Fixed a memory leak creating new :class:`~multidict.istr` objects -- by :user:`bdraco`.

  The leak was introduced in 6.3.0

  *Related issues and pull requests on GitHub:*
  :issue:`1133`.

- Fixed reference counting when calling :py:meth:`multidict.MultiDict.update` -- by :user:`bdraco`.

  The leak was introduced in 4.4.0

  *Related issues and pull requests on GitHub:*
  :issue:`1135`.


Features
--------

- Switched C Extension to use heap types and the module state.

  *Related issues and pull requests on GitHub:*
  :issue:`1125`.

- Started building armv7l wheels -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1127`.


----


6.3.2
=====

*(2025-04-03)*


Bug fixes
---------

- Resolved a memory leak by ensuring proper reference count decrementation -- by :user:`asvetlov` and :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1121`.


----


6.3.1
=====

*(2025-04-01)*


Bug fixes
---------

- Fixed keys not becoming case-insensitive when :class:`multidict.CIMultiDict` is created by passing in a :class:`multidict.MultiDict` -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1112`.

- Fixed the pure Python version mutating the original :class:`multidict.MultiDict` when creating a new :class:`multidict.CIMultiDict` from an existing one when keyword arguments are also passed -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1113`.

- Prevented crashing with a segfault when :func:`repr` is called for recursive multidicts and their proxies and views.

  *Related issues and pull requests on GitHub:*
  :issue:`1115`.


----


6.3.0
=====

*(2025-03-31)*


Bug fixes
---------

- Set operations for ``KeysView`` and ``ItemsView`` of case-insensitive multidicts and their proxies are processed in case-insensitive manner.

  *Related issues and pull requests on GitHub:*
  :issue:`965`.

- Rewrote :class:`multidict.CIMultiDict` and it proxy to always return
  :class:`multidict.istr` keys. ``istr`` is derived from :class:`str`,
  thus the change is backward compatible.

  The performance boost is about 15% for some operations for C Extension,
  pure Python implementation have got a visible (15% - 230%) speedup as well.

  *Related issues and pull requests on GitHub:*
  :issue:`1097`.

- Fixed a crash when extending a multidict from multidict proxy if C Extensions were used.

  *Related issues and pull requests on GitHub:*
  :issue:`1100`.


Features
--------

- Implemented a custom parser for ``METH_FASTCALL | METH_KEYWORDS`` protocol
  -- by :user:`asvetlov`.

  The patch re-enables fast call protocol in the :py:mod:`multidict` C Extension.

  Speedup is about 25%-30% for the library benchmarks for Python 3.12+.

  *Related issues and pull requests on GitHub:*
  :issue:`1070`.

- The C-extension no longer pre-allocates a Python exception object in
  lookup-related methods of :py:class:`~multidict.MultiDict` when the
  passed-in *key* is not found but *default* value is provided.

  Namely, this affects :py:meth:`MultiDict.getone()
  <multidict.MultiDict.getone>`, :py:meth:`MultiDict.getall()
  <multidict.MultiDict.getall>`, :py:meth:`MultiDict.get()
  <multidict.MultiDict.get>`, :py:meth:`MultiDict.pop()
  <multidict.MultiDict.pop>`, :py:meth:`MultiDict.popone()
  <multidict.MultiDict.popone>`, and :py:meth:`MultiDict.popall()
  <multidict.MultiDict.popall>`.

  Additionally, the :py:class:`~multidict.MultiDict` comparison with
  regular :py:class:`dict`\ ionaries is now about 60% faster
  on Python 3.13+ in the fallback-to-default case.

  *Related issues and pull requests on GitHub:*
  :issue:`1078`.

- Implemented ``__repr__()`` for C Extension classes in C.

  The speedup is about 2.5 times.

  *Related issues and pull requests on GitHub:*
  :issue:`1081`.

- Made C version of :class:`multidict.istr` pickleable.

  *Related issues and pull requests on GitHub:*
  :issue:`1098`.

- Optimized multidict creation and extending / updating if C Extensions are used.

  The speedup is between 25% and 70% depending on the usage scenario.

  *Related issues and pull requests on GitHub:*
  :issue:`1101`.

- :meth:`multidict.MultiDict.popitem` is changed to remove
  the latest entry instead of the first.

  It gives O(1) amortized complexity.

  The standard :meth:`dict.popitem` removes the last entry also.

  *Related issues and pull requests on GitHub:*
  :issue:`1105`.


Contributor-facing changes
--------------------------

- Started running benchmarks for the pure Python implementation in addition to the C implementation -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1092`.

- The the project-wide Codecov_ metric is no longer reported
  via GitHub Checks API. The combined value is not very useful
  because one of the sources (MyPy) cannot reach 100% with the
  current state of the ecosystem. We may want to reconsider in
  the future. Instead, we now have two separate
  “runtime coverage” metrics for library code and tests.
  They are to be kept at 100% at all times.
  And the “type coverage” metric will remain advisory, at a
  lower threshold.

  The default patch metric check is renamed to “runtime”
  to better reflect its semantics. This one will also require
  100% coverage.
  Another “typing” patch coverage metric is now reported
  alongside it. It's considered advisory, just like its
  project counterpart.

  When looking at Codecov_, one will likely want to look at
  MyPy and pytest flags separately. It is usually best to
  avoid looking at the PR pages that sometimes display
  combined coverage incorrectly.

  The change additionally disables the deprecated GitHub
  Annotations integration in Codecov_.

  Finally, the badge coloring range now starts at 100%.


  .. image:: https://codecov.io/gh/aio-libs/multidict/branch/master/graph/badge.svg?flag=pytest
     :target: https://codecov.io/gh/aio-libs/multidict?flags[]=pytest
     :alt: Coverage metrics


  -- by :user:`webknjaz`

  *Related issues and pull requests on GitHub:*
  :issue:`1093`.


Miscellaneous internal changes
------------------------------

- Synchronized :file:`pythoncapi_compat.h` with the latest available version.

  *Related issues and pull requests on GitHub:*
  :issue:`1063`.

- Moved registering ABCs for C Extension classes from C to Python.

  *Related issues and pull requests on GitHub:*
  :issue:`1083`.

- Refactored the internal ``pair_list`` implementation.

  *Related issues and pull requests on GitHub:*
  :issue:`1084`.

- Implemented views comparison and disjoints in C instead of Python helpers.

  The performance boost is about 40%.

  *Related issues and pull requests on GitHub:*
  :issue:`1096`.


----


6.2.0
======

*(2025-03-17)*


Bug fixes
---------

- Fixed ``in`` checks throwing an exception instead of returning :data:`False` when testing non-strings.

  *Related issues and pull requests on GitHub:*
  :issue:`1045`.

- Fixed a leak when the last accessed module in ``PyInit__multidict()`` init is not released.

  *Related issues and pull requests on GitHub:*
  :issue:`1061`.


Features
--------

- Implemented support for the free-threaded build of CPython 3.13 -- by :user:`lysnikolaou`.

  *Related issues and pull requests on GitHub:*
  :issue:`1015`.


Packaging updates and notes for downstreams
-------------------------------------------

- Started publishing wheels made for the free-threaded build of CPython 3.13 -- by :user:`lysnikolaou`.

  *Related issues and pull requests on GitHub:*
  :issue:`1015`.


Miscellaneous internal changes
------------------------------

- Used stricter typing across the code base, resulting in improved typing accuracy across multidict classes.
  Funded by an ``NLnet`` grant.

  *Related issues and pull requests on GitHub:*
  :issue:`1046`.


----


6.1.0 (2024-09-09)
==================

Bug fixes
---------

- Covered the unreachable code path in
  ``multidict._multidict_base._abc_itemsview_register()``
  with typing -- by :user:`skinnyBat`.


  *Related issues and pull requests on GitHub:*
  :issue:`928`.




Features
--------

- Added support for Python 3.13 -- by :user:`bdraco`.


  *Related issues and pull requests on GitHub:*
  :issue:`1002`.




Removals and backward incompatible breaking changes
---------------------------------------------------

- Removed Python 3.7 support -- by :user:`bdraco`.


  *Related issues and pull requests on GitHub:*
  :issue:`997`.




Contributor-facing changes
--------------------------

- Added tests to have full code coverage of the
  ``multidict._multidict_base._viewbaseset_richcmp()`` function
  -- by :user:`skinnyBat`.


  *Related issues and pull requests on GitHub:*
  :issue:`928`.



- `The deprecated <https://hynek.me/til/set-output-deprecation-github-actions/>`_
  ``::set-output`` workflow command has been replaced
  by the ``$GITHUB_OUTPUT`` environment variable
  in the GitHub Actions CI/CD workflow definition.


  *Related issues and pull requests on GitHub:*
  :issue:`940`.



- `codecov-action <https://github.com/codecov/codecov-action>`_
  has been temporarily downgraded to ``v3``
  in the GitHub Actions CI/CD workflow definitions
  in order to fix uploading coverage to Codecov_.
  See `this issue <https://github.com/codecov/codecov-action/issues/1252>`_
  for more details.


  .. _Codecov: https://codecov.io/gh/aio-libs/multidict?flags[]=pytest


  *Related issues and pull requests on GitHub:*
  :issue:`941`.



- In the GitHub Actions CI/CD workflow definition,
  the ``Get pip cache dir`` step has been fixed for
  Windows runners by adding ``shell: bash``.
  See `actions/runner#2224 <https://github.com/actions/runner/issues/2224>`_
  for more details.


  *Related issues and pull requests on GitHub:*
  :issue:`942`.



- Interpolation of the ``pip`` cache keys has been
  fixed by adding missing ``$`` syntax
  in the GitHub Actions CI/CD workflow definition.


  *Related issues and pull requests on GitHub:*
  :issue:`943`.




----


6.0.5 (2024-02-01)
==================

Bug fixes
---------

- Upgraded the C-API macros that have been deprecated in Python 3.9
  and later removed in 3.13 -- by :user:`iemelyanov`.


  *Related issues and pull requests on GitHub:*
  :issue:`862`, :issue:`864`, :issue:`868`, :issue:`898`.



- Reverted to using the public argument parsing API
  :c:func:`PyArg_ParseTupleAndKeywords` under Python 3.12
  -- by :user:`charles-dyfis-net` and :user:`webknjaz`.

  The effect is that this change prevents build failures with
  clang 16.9.6 and gcc-14 reported in :issue:`926`. It also
  fixes a segmentation fault crash caused by passing keyword
  arguments to :py:meth:`MultiDict.getall()
  <multidict.MultiDict.getall>` discovered by :user:`jonaslb`
  and :user:`hroncok` while examining the problem.


  *Related issues and pull requests on GitHub:*
  :issue:`862`, :issue:`909`, :issue:`926`, :issue:`929`.



- Fixed a ``SystemError: null argument to internal routine`` error on
  a ``MultiDict.items().isdisjoint()`` call when using C Extensions.


  *Related issues and pull requests on GitHub:*
  :issue:`927`.




Improved documentation
----------------------

- On the `Contributing docs <https://github.com/aio-libs/multidict/blob/master/CHANGES/README.rst>`_ page,
  a link to the ``Towncrier philosophy`` has been fixed.


  *Related issues and pull requests on GitHub:*
  :issue:`911`.




Packaging updates and notes for downstreams
-------------------------------------------

- Stopped marking all files as installable package data
  -- by :user:`webknjaz`.

  This change helps ``setuptools`` understand that C-headers are
  not to be installed under :file:`lib/python3.{x}/site-packages/`.



  *Related commits on GitHub:*
  :commit:`31e1170`.


- Started publishing pure-python wheels to be installed
  as a fallback -- by :user:`webknjaz`.



  *Related commits on GitHub:*
  :commit:`7ba0e72`.


- Switched from ``setuptools``' legacy backend (``setuptools.build_meta:__legacy__``)
  to the modern one (``setuptools.build_meta``) by actually specifying the
  the ``[build-system] build-backend`` option in :file:`pyproject.toml`
  -- by :user:`Jackenmen`.


  *Related issues and pull requests on GitHub:*
  :issue:`802`.



- Declared Python 3.12 supported officially in the
  distribution package metadata -- by :user:`hugovk`.


  *Related issues and pull requests on GitHub:*
  :issue:`877`.




Contributor-facing changes
--------------------------

- The test framework has been refactored. In the previous state, the circular
  imports reported in :issue:`837` caused the C-extension tests to be skipped.

  Now, there is a set of the ``pytest`` fixtures that is set up in a parametrized
  manner allowing to have a consistent way of accessing mirrored ``multidict``
  implementations across all the tests.

  This change also implemented a pair of CLI flags (``--c-extensions`` /
  ``--no-c-extensions``) that allow to explicitly request deselecting the tests
  running against the C-extension.

  -- by :user:`webknjaz`.


  *Related issues and pull requests on GitHub:*
  :issue:`98`, :issue:`837`, :issue:`915`.



- Updated the test pins lockfile used in the
  ``cibuildwheel`` test stage -- by :user:`hoodmane`.


  *Related issues and pull requests on GitHub:*
  :issue:`827`.



- Added an explicit ``void`` for arguments in C-function signatures
  which addresses the following compiler warning:

  .. code-block:: console

     warning: a function declaration without a prototype is deprecated in all versions of C [-Wstrict-prototypes]

  -- by :user:`hoodmane`


  *Related issues and pull requests on GitHub:*
  :issue:`828`.



- An experimental Python 3.13 job now runs in the CI
  -- :user:`webknjaz`.


  *Related issues and pull requests on GitHub:*
  :issue:`920`.



- Added test coverage for the :ref:`and <python:and>`, :ref:`or
  <python:or>`, :py:obj:`sub <python:object.__sub__>`, and
  :py:obj:`xor <python:object.__xor__>` operators in the
  :file:`multidict/_multidict_base.py` module. It also covers
  :py:data:`NotImplemented` and
  ":py:class:`~typing.Iterable`-but-not-:py:class:`~typing.Set`"
  cases there.

  -- by :user:`a5r0n`


  *Related issues and pull requests on GitHub:*
  :issue:`936`.



- The version of pytest is now capped below 8, when running MyPy
  against Python 3.7. This pytest release dropped support for
  said runtime.


  *Related issues and pull requests on GitHub:*
  :issue:`937`.




----


6.0.4 (2022-12-24)
==================

Bugfixes
--------

- Fixed a type annotations regression introduced in v6.0.2 under Python versions <3.10. It was caused by importing certain types only available in newer versions. (:issue:`798`)


6.0.3 (2022-12-03)
==================

Features
--------

- Declared the official support for Python 3.11 — by :user:`mlegner`. (:issue:`872`)


6.0.2 (2022-01-24)
==================

Bugfixes
--------

- Revert :issue:`644`, restore type annotations to as-of 5.2.0 version. (:issue:`688`)


6.0.1 (2022-01-23)
==================

Bugfixes
--------

- Restored back ``MultiDict``, ``CIMultiDict``, ``MultiDictProxy``, and
  ``CIMutiDictProxy`` generic type arguments; they are parameterized by value type, but the
  key type is fixed by container class.

  ``MultiDict[int]`` means ``MutableMultiMapping[str, int]``. The key type of
  ``MultiDict`` is always ``str``, while all str-like keys are accepted by API and
  converted to ``str`` internally.

  The same is true for ``CIMultiDict[int]`` which means ``MutableMultiMapping[istr,
  int]``. str-like keys are accepted but converted to ``istr`` internally. (:issue:`682`)


6.0.0 (2022-01-22)
==================

Features
--------

- Use ``METH_FASTCALL`` where it makes sense.

  ``MultiDict.add()`` is 2.2 times faster now, ``CIMultiDict.add()`` is 1.5 times faster.
  The same boost is applied to ``get*()``, ``setdefault()``, and ``pop*()`` methods. (:issue:`681`)


Bugfixes
--------

- Fixed type annotations for keys of multidict mapping classes. (:issue:`644`)
- Support Multidict[int] for pure-python version.
  ``__class_getitem__`` is already provided by C Extension, making it work with the pure-extension too. (:issue:`678`)


Deprecations and Removals
-------------------------

- Dropped Python 3.6 support (:issue:`680`)


Misc
----

- :issue:`659`


5.2.0 (2021-10-03)
=====================

Features
--------

- 1. Added support Python 3.10
  2. Started shipping platform-specific wheels with the ``musl`` tag targeting typical Alpine Linux runtimes.
  3. Started shipping platform-specific arm64 wheels for Apple Silicon. (:issue:`629`)


Bugfixes
--------

- Fixed pure-python implementation that used to raise "Dictionary changed during iteration" error when iterated view (``.keys()``, ``.values()`` or ``.items()``) was created before the dictionary's content change. (:issue:`620`)


5.1.0 (2020-12-03)
==================

Features
--------

- Supported ``GenericAliases`` (``MultiDict[str]``) for Python 3.9+
  :issue:`553`


Bugfixes
--------

- Synchronized the declared supported Python versions in ``setup.py`` with actually supported and tested ones.
  :issue:`552`


----


5.0.1 (2020-11-14)
==================

Bugfixes
--------

- Provided x86 Windows wheels
  :issue:`550`


----


5.0.0 (2020-10-12)
==================

Features
--------

- Provided wheels for ``aarch64``, ``i686``, ``ppc64le``, ``s390x`` architectures on Linux
  as well as ``x86_64``.
  :issue:`500`
- Provided wheels for Python 3.9.
  :issue:`534`

Removal
-------

- Dropped Python 3.5 support; Python 3.6 is the minimal supported Python version.

Misc
----

- :issue:`503`


----
