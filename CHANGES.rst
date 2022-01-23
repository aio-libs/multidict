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
