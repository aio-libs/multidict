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

4.6.0 (2019-11-20)
====================

Bugfixes
--------

- Fix GC object tracking.
  `#314 <https://github.com/aio-libs/aiohttp/issues/314>`_
- Preserve the case of `istr` strings.
  `#374 <https://github.com/aio-libs/aiohttp/issues/374>`_
- Generate binary wheels for Python 3.8.
