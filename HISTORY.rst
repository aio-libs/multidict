4.7.6 (2020-05-15)
------------------

Bugfixes
^^^^^^^^

- Fixed an issue with some versions of the ``wheel`` dist
  failing because of being unable to detect the license file.
  :issue:`481`


4.7.5 (2020-02-21)
------------------

Bugfixes
^^^^^^^^

- Fixed creating and updating of MultiDict from a sequence of pairs and keyword
  arguments. Previously passing a list argument modified it inplace, and other sequences
  caused an error.
  :issue:`457`
- Fixed comparing with mapping: an exception raised in the
  :py:func:`~object.__len__` method caused raising a SyntaxError.
  :issue:`459`
- Fixed comparing with mapping: all exceptions raised in the
  :py:func:`~object.__getitem__` method were silenced.
  :issue:`460`


4.7.4 (2020-01-11)
------------------

Bugfixes
^^^^^^^^

- ``MultiDict.iter`` fix memory leak when used iterator over
  :py:mod:`multidict` instance.
  :issue:`452`


Multidict 4.7.3 (2019-12-30)
----------------------------

Features
^^^^^^^^

- Implement ``__sizeof__`` function to correctly calculate all internal structures size.
  :issue:`444`
- Expose ``getversion()`` function.
  :issue:`451`


Bugfixes
^^^^^^^^

- Fix crashes in ``popone``/``popall`` when default is returned.
  :issue:`450`


Improved Documentation
^^^^^^^^^^^^^^^^^^^^^^

- Corrected the documentation for ``MultiDict.extend()``
  :issue:`446`



4.7.2 (2019-12-20)
------------------

Bugfixes
^^^^^^^^

- Fix crashing when multidict is used pyinstaller
  :issue:`432`
- Fix typing for :py:meth:`CIMultiDict.copy`
  :issue:`434`
- Fix memory leak in ``MultiDict.copy()``
  :issue:`443`


4.7.1 (2019-12-12)
------------------

Bugfixes
^^^^^^^^

- :py:meth:`CIMultiDictProxy.copy` return object type
  :py:class:`multidict._multidict.CIMultiDict`
  :issue:`427`
- Make :py:class:`CIMultiDict` subclassable again
  :issue:`416`
- Fix regression, multidict can be constructed from arbitrary iterable of pairs again.
  :issue:`418`
- :py:meth:`CIMultiDict.add` may be called with keyword arguments
  :issue:`421`


Improved Documentation
^^^^^^^^^^^^^^^^^^^^^^

- Mention ``MULTIDICT_NO_EXTENSIONS`` environment variable in docs.
  :issue:`393`
- Document the fact that ``istr`` preserves the casing of argument untouched but uses internal lower-cased copy for keys comparison.
  :issue:`419`


4.7.0 (2019-12-10)
------------------

Features
^^^^^^^^

- Replace Cython optimization with pure C
  :issue:`249`
- Implement ``__length_hint__()`` for iterators
  :issue:`310`
- Support the MultiDict[str] generic specialization in the runtime.
  :issue:`392`
- Embed pair_list_t structure into MultiDict Python object
  :issue:`395`
- Embed multidict pairs for small dictionaries to amortize the memory usage.
  :issue:`396`
- Support weak references to C Extension classes.
  :issue:`399`
- Add docstrings to provided classes.
  :issue:`400`
- Merge ``multidict._istr`` back with ``multidict._multidict``.
  :issue:`409`


Bugfixes
^^^^^^^^

- Explicitly call ``tp_free`` slot on deallocation.
  :issue:`407`
- Return class from __class_getitem__ to simplify subclassing
  :issue:`413`


4.6.1 (2019-11-21)
------------------

Bugfixes
^^^^^^^^

- Fix PyPI link for GitHub Issues badge.
  :issue:`391`

4.6.0 (2019-11-20)
------------------

Bugfixes
^^^^^^^^

- Fix GC object tracking.
  :issue:`314`
- Preserve the case of `istr` strings.
  :issue:`374`
- Generate binary wheels for Python 3.8.


4.5.2 (2018-11-28)
------------------

* Fix another memory leak introduced by 4.5.0 release
  :issue:`307`

4.5.1 (2018-11-22)
------------------

* Fix a memory leak introduced by 4.5.0 release
  :issue:`306`

4.5.0 (2018-11-19)
------------------

* Multidict views ported from Cython to C extension
  :issue:`275`


4.4.2 (2018-09-19)
------------------

* Restore Python 3.4 support
  :issue:`289`


4.4.1 (2018-09-17)
------------------

* Fix type annotations
  :issue:`283`)

* Allow to install the library on systems without compilation toolset
  :issue:`281`


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
  :issue:`202`

4.0.0 (2018-01-14)
------------------

* Accept multiple keys in :py:meth:`MultiDict.update` and
  :py:meth:`CIMultiDict.update`
  :issue:`199`

3.3.2 (2017-11-02)
------------------

* Fix tarball (again)


3.3.1 (2017-11-01)
------------------

* Include .c files in tarball
  :issue:`181`


3.3.0 (2017-10-15)
------------------

* Introduce abstract base classes
  :issue:`102`

* Publish OSX binary wheels
  :issue:`153`


3.2.0 (2017-09-17)
------------------

* Fix pickling
  :issue:`134`

* Fix equality check when other contains more keys
  :issue:`124`

* Fix :py:class:`CIMultiDict <multidict.CIMultiDict>` copy
  :issue:`107`

3.1.3 (2017-07-14)
------------------

* Fix build

3.1.2 (2017-07-14)
------------------

* Fix type annotations

3.1.1 (2017-07-09)
------------------

* Remove memory leak in :py:func:`istr <multidict.istr>` implementation
  :issue:`105`

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

  :issue:`68`

* Force keys to :py:class:`str` instances
  :issue:`88`

* Implement :py:func:`.popall(key[, default]) <multidict.MultiDict.popall>`
  :issue:`84`

* :py:func:`.pop() <multidict.MultiDict.pop>` removes only first occurrence,
  :py:func:`.popone() <multidict.MultiDict.popone>` added
  :issue:`92`

* Implement dict's version
  :issue:`86`

* Proxies are not pickable anymore
  :issue:`77`

2.1.7 (2017-05-29)
------------------

* Fix import warning on Python 3.6
  :issue:`79`

2.1.6 (2017-05-27)
------------------

* Rebuild the library for fixing missing ``__spec__`` attribute
  :issue:`79`

2.1.5 (2017-05-13)
------------------

* Build Python 3.6 binary wheels

2.1.4 (2016-12-1)
------------------

* Remove ``LICENSE`` filename extension @ ``MANIFEST.in`` file
  :issue:`31`

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
  :py:func:`istr <multidict.istr>` :issue:`11`


2.1.0 (2016-09-18)
------------------

* Allow to create proxy from proxy

* Add type hints (:pep:`484`)


2.0.1 (2016-08-02)
------------------

* Don't crash on ``{} - MultiDict().keys()`` and similar operations
  :issue:`6`


2.0.0 (2016-07-28)
------------------

* Switch from uppercase approach for case-insensitive string to
  :py:func:`str.title() <str.title>`
  :issue:`5`

* Deprecate :py:func:`upstr <multidict.upstr>` class in favor of :py:func:`istr <multidict.istr>` alias.

1.2.2 (2016-08-02)
------------------

* Don't crash on ``{} - MultiDict().keys()`` and similar operations
  :issue:`6`

1.2.1 (2016-07-21)
------------------

* Don't expose ``multidict.__version__``


1.2.0 (2016-07-16)
------------------

* Make ``upstr(upstr('abc'))`` much faster


1.1.0 (2016-07-06)
------------------

* Don't double-iterate during :py:class:`MultiDict <multidict.MultiDict>` initialization
  :issue:`3`

* Fix :py:func:`CIMultiDict.pop <multidict.CIMultiDict.pop>`: it is case insensitive now
  :issue:`1`

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
