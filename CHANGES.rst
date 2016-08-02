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
