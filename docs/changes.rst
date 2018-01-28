.. _changes:

4.1.0 (2018-01-28)
------------------

* Fix key casing in Pure Python impmenetation of
  :py:class:`CIMultiDict` (:pr:`202`)

4.0.0 (2018-01-14)
------------------

* Accept multiple keys in :py:meth:`MultiDict.update` and
  :py:meth:`CIMultiDict.update` (:pr:`199`)

3.3.2 (2017-11-02)
------------------

* Fix tarball (again)


3.3.1 (2017-11-01)
------------------

* Include .c files in tarball (:issue:`181`)


3.3.0 (2017-10-15)
------------------

* Introduce abstract base classes (:issue:`102`)

* Publish OSX binary wheels (:pr:`153`)


3.2.0 (2017-09-17)
------------------

* Fix pickling (:pr:`134`)

* Fix equality check when other contains more keys (:pr:`124`)

* Fix :py:class:`CIMultiDict <multidict.CIMultiDict>` copy (:issue:`107`)

3.1.3 (2017-07-14)
------------------

* Fix build

3.1.2 (2017-07-14)
------------------

* Fix type annotations

3.1.1 (2017-07-09)
------------------

* Remove memory leak in :py:func:`istr <multidict.istr>` implementation (:issue:`105`)

3.1.0 (2017-06-25)
------------------

* Raise :py:exc:`RuntimeError` on :py:class:`dict` iterations if the dict was changed (:issue:`99`)

* Update ``__init__.pyi`` signatures

3.0.0 (2017-06-21)
------------------

* Refactor internal data structures: main dict operations are about
  100% faster now.

* Preserve order on multidict updates (:issue:`68`)

  Updates are ``md[key] = val`` and ``md.update(...)`` calls.

  Now **the last** entry is replaced with new key/value pair, all
  previous occurrences are removed.

  If key is not present in dictionary the pair is added to the end

* Force keys to :py:class:`str` instances (:issue:`88`)

* Implement :py:func:`.popall(key[, default]) <multidict.MultiDict.popall>` (:issue:`84`)

* :py:func:`.pop() <multidict.MultiDict.pop>` removes only first occurence, :py:func:`.popone() <multidict.MultiDict.popone>` added (:issue:`92`)

* Implement dict's version (:issue:`86`)

* Proxies are not pickable anymore (:pr:`77`)

2.1.7 (2017-05-29)
------------------

* Fix import warning on Python 3.6 (:issue:`79`)

2.1.6 (2017-05-27)
------------------

* Rebuild the library for fixning missing ``__spec__`` attribute (:issue:`79`)

2.1.5 (2017-05-13)
------------------

* Build Python 3.6 binary wheels

2.1.4 (2016-12-1)
------------------

* Remove ``LICENSE`` filename extension @ ``MANIFEST.in`` file (:pr:`31`)

2.1.3 (2016-11-26)
------------------

* Add a fastpath for multidict extending by multidict


2.1.2 (2016-09-25)
------------------

* Fix :py:func:`CIMultiDict.update <multidict.CIMultiDict.update>` for case of accepting :py:func:`istr <multidict.istr>`


2.1.1 (2016-09-22)
------------------

* Fix :py:class:`CIMultiDict <multidict.CIMultiDict>` constructor for case of accepting :py:func:`istr <multidict.istr>` (:issue:`11`)


2.1.0 (2016-09-18)
------------------

* Allow to create proxy from proxy

* Add type hints (:pep:`484`)


2.0.1 (2016-08-02)
------------------

* Don't crash on ``{} - MultiDict().keys()`` and similar operations (:issue:`6`)


2.0.0 (2016-07-28)
------------------

* Switch from uppercase approach for case-insensitive string to
  :py:func:`str.title() <str.title>` (:pr:`5`)

* Deprecate :py:func:`upstr <multidict.upstr>` class in favor of :py:func:`istr <multidict.istr>` alias.

1.2.2 (2016-08-02)
------------------

* Don't crash on ``{} - MultiDict().keys()`` and similar operations (:issue:`6`)

1.2.1 (2016-07-21)
------------------

* Don't expose ``multidict.__version__``


1.2.0 (2016-07-16)
------------------

* Make ``upstr(upstr('abc'))`` much faster


1.1.0 (2016-07-06)
------------------

* Don't double-iterate during :py:class:`MultiDict <multidict.MultiDict>` initialization (:pr:`3`)

* Fix :py:func:`CIMultiDict.pop <multidict.CIMultiDict.pop>`: it is case insensitive now (:issue:`1`)

* Provide manylinux wheels as well as Windows ones

1.0.3 (2016-03-24)
------------------

* Add missing MANIFEST.in

1.0.2 (2016-03-24)
------------------

* Fix setup build


1.0.0 (2016-02-19)
------------------

* Initial implementation
