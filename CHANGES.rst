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
