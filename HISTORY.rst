4.7.6 (2020-05-15)
------------------

Bugfixes
^^^^^^^^

- Fixed an issue with some versions of the ``wheel`` dist
  failing because of being unable to detect the license file.
  `#481 <https://github.com/aio-libs/multidict/issues/481>`_


4.7.5 (2020-02-21)
------------------

Bugfixes
^^^^^^^^

- Fixed creating and updating of MultiDict from a sequence of pairs and keyword
  arguments. Previously passing a list argument modified it inplace, and other sequences
  caused an error.
  `#457 <https://github.com/aio-libs/multidict/issues/457>`_
- Fixed comparing with mapping: an exception raised in the
  :py:func:`~object.__len__` method caused raising a SyntaxError.
  `#459 <https://github.com/aio-libs/multidict/issues/459>`_
- Fixed comparing with mapping: all exceptions raised in the
  :py:func:`~object.__getitem__` method were silenced.
  `#460 <https://github.com/aio-libs/multidict/issues/460>`_


4.7.4 (2020-01-11)
------------------

Bugfixes
^^^^^^^^

- ``MultiDict.iter`` fix memory leak when used iterator over
  :py:mod:`multidict` instance.
  `#452 <https://github.com/aio-libs/multidict/issues/452>`_


Multidict 4.7.3 (2019-12-30)
----------------------------

Features
^^^^^^^^

- Implement ``__sizeof__`` function to correctly calculate all internal structures size.
  `#444 <https://github.com/aio-libs/multidict/issues/444>`_
- Expose ``getversion()`` function.
  `#451 <https://github.com/aio-libs/multidict/issues/451>`_


Bugfixes
^^^^^^^^

- Fix crashes in ``popone``/``popall`` when default is returned.
  `#450 <https://github.com/aio-libs/multidict/issues/450>`_


Improved Documentation
^^^^^^^^^^^^^^^^^^^^^^

- Corrected the documentation for ``MultiDict.extend()``
  `#446 <https://github.com/aio-libs/multidict/issues/446>`_



4.7.2 (2019-12-20)
------------------

Bugfixes
^^^^^^^^

- Fix crashing when multidict is used pyinstaller
  `#432 <https://github.com/aio-libs/multidict/issues/432>`_
- Fix typing for :py:meth:`CIMultiDict.copy`
  `#434 <https://github.com/aio-libs/multidict/issues/434>`_
- Fix memory leak in ``MultiDict.copy()``
  `#443 <https://github.com/aio-libs/multidict/issues/443>`_


4.7.1 (2019-12-12)
------------------

Bugfixes
^^^^^^^^

- :py:meth:`CIMultiDictProxy.copy` return object type
  :py:class:`multidict._multidict.CIMultiDict`
  `#427 <https://github.com/aio-libs/multidict/issues/427>`_
- Make :py:class:`CIMultiDict` subclassable again
  `#416 <https://github.com/aio-libs/multidict/issues/416>`_
- Fix regression, multidict can be constructed from arbitrary iterable of pairs again.
  `#418 <https://github.com/aio-libs/multidict/issues/418>`_
- :py:meth:`CIMultiDict.add` may be called with keyword arguments
  `#421 <https://github.com/aio-libs/multidict/issues/421>`_


Improved Documentation
^^^^^^^^^^^^^^^^^^^^^^

- Mention ``MULTIDICT_NO_EXTENSIONS`` environment variable in docs.
  `#393 <https://github.com/aio-libs/multidict/issues/393>`_
- Document the fact that ``istr`` preserves the casing of argument untouched but uses internal lower-cased copy for keys comparison.
  `#419 <https://github.com/aio-libs/multidict/issues/419>`_


4.7.0 (2019-12-10)
------------------

Features
^^^^^^^^

- Replace Cython optimization with pure C
  `#249 <https://github.com/aio-libs/multidict/issues/249>`_
- Implement ``__length_hint__()`` for iterators
  `#310 <https://github.com/aio-libs/multidict/issues/310>`_
- Support the MultiDict[str] generic specialization in the runtime.
  `#392 <https://github.com/aio-libs/multidict/issues/392>`_
- Embed pair_list_t structure into MultiDict Python object
  `#395 <https://github.com/aio-libs/multidict/issues/395>`_
- Embed multidict pairs for small dictionaries to amortize the memory usage.
  `#396 <https://github.com/aio-libs/multidict/issues/396>`_
- Support weak references to C Extension classes.
  `#399 <https://github.com/aio-libs/multidict/issues/399>`_
- Add docstrings to provided classes.
  `#400 <https://github.com/aio-libs/multidict/issues/400>`_
- Merge ``multidict._istr`` back with ``multidict._multidict``.
  `#409 <https://github.com/aio-libs/multidict/issues/409>`_


Bugfixes
^^^^^^^^

- Explicitly call ``tp_free`` slot on deallocation.
  `#407 <https://github.com/aio-libs/multidict/issues/407>`_
- Return class from __class_getitem__ to simplify subclassing
  `#413 <https://github.com/aio-libs/multidict/issues/413>`_


4.6.1 (2019-11-21)
------------------

Bugfixes
^^^^^^^^

- Fix PyPI link for GitHub Issues badge.
  `#391 <https://github.com/aio-libs/aiohttp/issues/391>`_

4.6.0 (2019-11-20)
------------------

Bugfixes
^^^^^^^^

- Fix GC object tracking.
  `#314 <https://github.com/aio-libs/aiohttp/issues/314>`_
- Preserve the case of `istr` strings.
  `#374 <https://github.com/aio-libs/aiohttp/issues/374>`_
- Generate binary wheels for Python 3.8.


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
