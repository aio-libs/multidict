4.5.2 (2018-11-28)
------------------

* Fix another memory leak introduced by 4.5.0 release
  `#307 <https://github.com/aio-libs/multidict/issues/307>`_

4.5.1 (2018-11-22)
------------------

* Fix a memory leak introduced by 4.5.0 release
  `#306 <https://github.com/aio-libs/multidict/issues/306>`_

4.5.0 (2018-11-19)
------------------

* Multidict views ported from Cython to C extension
  `#275 <https://github.com/aio-libs/multidict/issues/275>`_


4.4.2 (2018-09-19)
------------------

* Restore Python 3.4 support
  `#289 <https://github.com/aio-libs/multidict/issues/289>`_


4.4.1 (2018-09-17)
------------------

* Fix type annotations
  `#283 <https://github.com/aio-libs/multidict/issues/283>`_)

* Allow to install the library on systems without compilation toolset
  `#281 <https://github.com/aio-libs/multidict/issues/281>`_


4.4.0 (2018-07-04)
------------------

* Rewrite C implementation to use C pair list.

* Fix update order when both ``arg`` and ``kwargs`` are used.


4.3.1 (2018-05-06)
------------------

* Fix a typo in multidict stub file.

4.3.0 (2018-05-06)
------------------

* Polish type hints, make multidict type definitions generic.

4.2.0 (2018-04-15)
------------------

* Publish ``py.typed`` flag for type hinting analyzers (``mypy`` etc).

4.1.0 (2018-01-28)
------------------

* Fix key casing in Pure Python implementation of
  :py:class:`CIMultiDict`
  `#202 <https://github.com/aio-libs/multidict/issues/202>`_

4.0.0 (2018-01-14)
------------------

* Accept multiple keys in :py:meth:`MultiDict.update` and
  :py:meth:`CIMultiDict.update`
  `#199 <https://github.com/aio-libs/multidict/issues/199>`_

3.3.2 (2017-11-02)
------------------

* Fix tarball (again)


3.3.1 (2017-11-01)
------------------

* Include .c files in tarball
  `#181 <https://github.com/aio-libs/multidict/issues/181>`_


3.3.0 (2017-10-15)
------------------

* Introduce abstract base classes
  `#102 <https://github.com/aio-libs/multidict/issues/102>`_

* Publish OSX binary wheels
  `#153 <https://github.com/aio-libs/multidict/issues/153>`_


3.2.0 (2017-09-17)
------------------

* Fix pickling
  `#134 <https://github.com/aio-libs/multidict/issues/134>`_

* Fix equality check when other contains more keys
  `#124 <https://github.com/aio-libs/multidict/issues/124>`_

* Fix :py:class:`CIMultiDict <multidict.CIMultiDict>` copy
  `#107 <https://github.com/aio-libs/multidict/issues/107>`_

3.1.3 (2017-07-14)
------------------

* Fix build

3.1.2 (2017-07-14)
------------------

* Fix type annotations

3.1.1 (2017-07-09)
------------------

* Remove memory leak in :py:func:`istr <multidict.istr>` implementation
  `#105 <https://github.com/aio-libs/multidict/issues/105>`_

3.1.0 (2017-06-25)
------------------

* Raise :py:exc:`RuntimeError` on :py:class:`dict` iterations if the dict was changed (:issue:`99`)

* Update ``__init__.pyi`` signatures

3.0.0 (2017-06-21)
------------------

* Refactor internal data structures: main dict operations are about
  100% faster now.

* Preserve order on multidict updates

  Updates are ``md[key] = val`` and ``md.update(...)`` calls.

  Now **the last** entry is replaced with new key/value pair, all
  previous occurrences are removed.

  If key is not present in dictionary the pair is added to the end

  `#68 <https://github.com/aio-libs/multidict/issues/68>`_

* Force keys to :py:class:`str` instances
  `#88 <https://github.com/aio-libs/multidict/issues/88>`_

* Implement :py:func:`.popall(key[, default]) <multidict.MultiDict.popall>`
  `#84 <https://github.com/aio-libs/multidict/issues/84>`_

* :py:func:`.pop() <multidict.MultiDict.pop>` removes only first occurrence,
  :py:func:`.popone() <multidict.MultiDict.popone>` added
  `#92 <https://github.com/aio-libs/multidict/issues/92>`_

* Implement dict's version
  `#86 <https://github.com/aio-libs/multidict/issues/86>`_

* Proxies are not pickable anymore
  `#77 <https://github.com/aio-libs/multidict/issues/77>`_

2.1.7 (2017-05-29)
------------------

* Fix import warning on Python 3.6
  `#79 <https://github.com/aio-libs/multidict/issues/79>`_

2.1.6 (2017-05-27)
------------------

* Rebuild the library for fixing missing ``__spec__`` attribute
  `#79 <https://github.com/aio-libs/multidict/issues/79>`_

2.1.5 (2017-05-13)
------------------

* Build Python 3.6 binary wheels

2.1.4 (2016-12-1)
------------------

* Remove ``LICENSE`` filename extension @ ``MANIFEST.in`` file
  `#31 <https://github.com/aio-libs/multidict/issues/31>`_

2.1.3 (2016-11-26)
------------------

* Add a fastpath for multidict extending by multidict


2.1.2 (2016-09-25)
------------------

* Fix :py:func:`CIMultiDict.update <multidict.CIMultiDict.update>` for case of accepting
  :py:func:`istr <multidict.istr>`


2.1.1 (2016-09-22)
------------------

* Fix :py:class:`CIMultiDict <multidict.CIMultiDict>` constructor for case of accepting
  :py:func:`istr <multidict.istr>` `#11
  <https://github.com/aio-libs/multidict/issues/11>`_


2.1.0 (2016-09-18)
------------------

* Allow to create proxy from proxy

* Add type hints (:pep:`484`)


2.0.1 (2016-08-02)
------------------

* Don't crash on ``{} - MultiDict().keys()`` and similar operations
  `#6 <https://github.com/aio-libs/multidict/issues/6>`_


2.0.0 (2016-07-28)
------------------

* Switch from uppercase approach for case-insensitive string to
  :py:func:`str.title() <str.title>`
  `#5 <https://github.com/aio-libs/multidict/issues/5>`_

* Deprecate :py:func:`upstr <multidict.upstr>` class in favor of :py:func:`istr <multidict.istr>` alias.

1.2.2 (2016-08-02)
------------------

* Don't crash on ``{} - MultiDict().keys()`` and similar operations
  `#6 <https://github.com/aio-libs/multidict/issues/6>`_

1.2.1 (2016-07-21)
------------------

* Don't expose ``multidict.__version__``


1.2.0 (2016-07-16)
------------------

* Make ``upstr(upstr('abc'))`` much faster


1.1.0 (2016-07-06)
------------------

* Don't double-iterate during :py:class:`MultiDict <multidict.MultiDict>` initialization
  `#3 <https://github.com/aio-libs/multidict/issues/3>`_

* Fix :py:func:`CIMultiDict.pop <multidict.CIMultiDict.pop>`: it is case insensitive now
  `#1 <https://github.com/aio-libs/multidict/issues/1>`_

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
