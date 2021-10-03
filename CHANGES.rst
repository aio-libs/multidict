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

5.2.0a25 (2021-10-03)
=====================

Features
--------

- 1. Added support Python 3.10
  2. Started shipping platform-specific wheels with the ``musl`` tag targeting typical Alpine Linux runtimes.
  3. Started shipping platform-specific arm64 wheels for Apple Silicon. (`#629 <https://github.com/aio-libs/multidict/issues/629>`_)


Bugfixes
--------

- Fixed pure-python implementation that used to raise "Dictionary changed during iteration" error when iterated view (``.keys()``, ``.values()`` or ``.items()``) was created before the dictionary's content change. (`#620 <https://github.com/aio-libs/multidict/issues/620>`_)


5.1.0 (2020-12-03)
==================

Features
--------

- Supported ``GenericAliases`` (``MultiDict[str]``) for Python 3.9+
  `#553 <https://github.com/aio-libs/multidict/issues/553>`_


Bugfixes
--------

- Synchronized the declared supported Python versions in ``setup.py`` with actually supported and tested ones.
  `#552 <https://github.com/aio-libs/multidict/issues/552>`_


----


5.0.1 (2020-11-14)
==================

Bugfixes
--------

- Provided x86 Windows wheels
  `#550 <https://github.com/aio-libs/multidict/issues/550>`_


----


5.0.0 (2020-10-12)
==================

Features
--------

- Provided wheels for ``aarch64``, ``i686``, ``ppc64le``, ``s390x`` architectures on Linux
  as well as ``x86_64``.
  `#500 <https://github.com/aio-libs/multidict/issues/500>`_
- Provided wheels for Python 3.9.
  `#534 <https://github.com/aio-libs/multidict/issues/534>`_

Removal
-------

- Dropped Python 3.5 support; Python 3.6 is the minimal supported Python version.

Misc
----

- `#503 <https://github.com/aio-libs/multidict/issues/503>`_


----
