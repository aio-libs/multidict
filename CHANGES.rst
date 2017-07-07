3.1.0 (2017-06-25)
------------------

* Fix #99: raise `RuntimeError` on dict iterations if the dict was changed

* Update `__init__.pyi` signatures

3.0.0 (2017-06-21)
------------------

* Refactor internal data structures: main dict operations are about
  100% faster now.

* Preserve order on multidict updates #68

  Updates are `md[key] = val` and `md.update(...)` calls.

  Now **the last** entry is replaced with new key/value pair, all
  previous occurrences are removed.

  If key is not present in dictionary the pair is added to the end

* Force keys to `str` instances #88

* Implement `.popall(key[, default])` #84

* `.pop()` removes only first occurence, `.popone()` added #92

* Implement dict's version #86

* Proxies are not pickable anymore #77

2.1.7 (2017-05-29)
------------------

* Fix import warning on Python 3.6 #79

2.1.6 (2017-05-27)
------------------

* Rebuild the library for fixning missing `__spec__` attribute #79

2.1.5 (2017-05-13)
------------------

* Build Python 3.6 binary wheels

2.1.4 (2016-12-1)
------------------

* Remove LICENSE filename extension @ MANIFEST.in file #31

2.1.3 (2016-11-26)
------------------

* Add a fastpath for multidict extending by multidict


2.1.2 (2016-09-25)
------------------

* Fix `CIMultiDict.update()` for case of accepting `istr`


2.1.1 (2016-09-22)
------------------

* Fix `CIMultiDict` constructor for case of accepting `istr` #11


2.1.0 (2016-09-18)
------------------

* Allow to create proxy from proxy

* Add type hints (PEP-484)


2.0.1 (2016-08-02)
------------------

* Don't crash on `{} - MultiDict().keys()` and similar operations #6


2.0.0 (2016-07-28)
------------------

* Switch from uppercase approach for case-insensitive string to
  `str.title()` #5

* Deprecase `upstr` class in favor of `istr` alias.

1.2.2 (2016-08-02)
------------------

* Don't crash on `{} - MultiDict().keys()` and similar operations #6

1.2.1 (2016-07-21)
------------------

* Don't expose `multidict.__version__`


1.2.0 (2016-07-16)
------------------

* Make `upstr(upstr('abc'))` much faster


1.1.0 (2016-07-06)
------------------

* Don't double-iterate during MultiDict initialization #3

* Fix CIMultiDict.pop: it is case insensitive now #1

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
