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

7.0.0
=====

*(2026-09-26)*

7.0.0 is a major release. It makes :class:`~multidict.istr` final, adds a
public C API for third-party extensions, and makes the C extension
substantially faster across the board.

**Breaking change.** :class:`~multidict.istr` can no longer be subclassed,
on either backend. Code that subclassed it has to wrap or convert instead.
The C extension also started rejecting an argument passed both
positionally and by name with :exc:`TypeError`, where it used to silently
misbehave.

**Public C and Cython API.** Other C extensions and Cython modules can now
create, read and mutate multidicts through a capsule, without going through
the Python-level API. The capsule comes with ``multidict_capi.h``, a
``cimport``-able ``multidict/__init__.pxd`` and
:func:`multidict.get_include`. The API includes ``MultiDict_ForEach()`` and
a watchers API modeled on CPython's dict watchers. See the :doc:`C API <capi>` and
:doc:`Cython API <cyapi>` references.

**Performance.** On CodSpeed's GIL-build benchmarks against 6.9.1, 82
C-extension benchmarks got faster and none got slower. Rough figures:

- :class:`~multidict.CIMultiDict` keyed by plain :class:`str`: construction,
  ``add()``, ``extend()``, ``update()`` and item assignment got 2 to 3.8
  times faster, and lookups about 2 times faster.
- :class:`~multidict.CIMultiDict` keyed by :class:`~multidict.istr`, and
  case-sensitive :class:`~multidict.MultiDict`: lookups, insertion and
  deletion got 10% to 70% faster.
- :meth:`~multidict.MultiDict.getall` and iterating
  :meth:`~multidict.MultiDict.items` got about 35% faster, and the view set
  operations 10% to 85% faster.
- :meth:`~multidict.MultiDict.popitem` on a whole mapping went from
  quadratic to linear time.
- On the free-threaded build, measured in instructions against 6.9.1,
  lookups got 24% to 33% cheaper on :class:`~multidict.MultiDict` and 4 to 5
  times cheaper on :class:`~multidict.CIMultiDict` with lowercase :class:`str`
  keys, and item assignment 15% to 19% cheaper. Construction and deletion on
  :class:`~multidict.MultiDict` cost 2% to 7% more. With several threads
  mutating at once, a ``d[key] = value`` got about four times faster.

The pure-Python backend is unchanged in speed.

**Robustness.** This release fixed several crashes and leaks in the C
extension, including use-after-free bugs when a key's ``lower()`` or a
value's ``__eq__`` mutated a multidict, a crash at interpreter shutdown,
and table and reference leaks on the free-threaded build.


Bug fixes
---------

- Fixed a segmentation fault in the C extension when a finalizer let the
  same multidict be resized while ``del md[key]`` or
  :meth:`~multidict.MultiDict.popall` was still scanning the table, and a
  spurious :exc:`RuntimeError` from :meth:`~multidict.MultiDict.getall`
  when a garbage collection triggered while building its result mutated
  the multidict -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1492`, :issue:`1512`.

- Fixed a reference leak of the key and value in the C extension when
  growing the hash table fails with :exc:`MemoryError` during
  :meth:`~multidict.MultiDict.add`, :meth:`~multidict.MultiDict.update`,
  :meth:`~multidict.MultiDict.merge`, :meth:`~multidict.MultiDict.setdefault`
  or item assignment -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1507`.

- Fixed a data race on the free-threaded build of the C extension where a
  ``MultiDict``'s own version counter was read by
  :func:`~multidict.getversion` and written on every mutation with a plain,
  non-atomic load and store. Both sides switched to a relaxed atomic
  load/store, matching the pattern already used for the object's item
  count -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1514`.

- Fixed the C extension discarding a :exc:`MemoryError` or
  :exc:`KeyboardInterrupt` raised by a positional argument's ``__len__``
  while the size of an update was estimated. ``extend()``, ``update()``,
  ``merge()`` and subclass construction started propagating it, matching the
  pure-Python backend; only the :exc:`TypeError` from an unusable
  ``__length_hint__`` stayed ignored -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1517`.

- Ensured that canonical keys in pure-Python :class:`~multidict.MultiDict` and
  :class:`~multidict.CIMultiDict` are converted to exact :class:`str` instances
  without invoking :meth:`object.__str__` overrides, matching the C extension --
  by :user:`agustin18`.

  *Related issues and pull requests on GitHub:*
  :issue:`1535`.

- Fixed the argument counts reported by the C extension when a method is
  called with too many positional arguments; methods with an optional
  second argument claimed to take exactly one, and methods requiring two
  claimed to take a range. A method called with none of its two required
  arguments now reports both as missing -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1545`.

- Fixed the C extension accepting an argument passed both positionally and
  by name instead of raising :exc:`TypeError`. ``md.add("a", key="k",
  value="v")`` stored a wrong value, and ``md.get("a", "b", key="c")``
  silently ignored the surplus arguments -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1545`.

- Fixed a memory leak in the free-threaded build of the C extension.
  :meth:`~multidict.MultiDict.clear` hands its hash table to a drain that
  frees it once no lock-free reader is in flight, and a drain that finds
  one leaves the table for the next drain to free. Deallocation ran no
  drain of its own, so a multidict dropped after such a clear, with no
  operation in between, never freed that table nor released the references
  its entries still held. Builds with the GIL enabled emit byte-identical
  code and are unaffected -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1555`, :issue:`1562`.

- Fixed a crash during interpreter shutdown in the C extension: a
  multidict started keeping a reference to its own module, so the module
  state it reads while being torn down could not be freed first
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1563`.

- Fixed retired hash tables lingering in the free-threaded build of the C
  extension. A drain that finds a lock-free reader in flight puts the tables
  it holds back for the next drain to run, and the reader it saw could
  already have looked, so nothing was left to free them: they stayed until
  the multidict was next used, and if it was dropped instead, the references
  their entries held were never released. Builds with the GIL enabled emit
  byte-identical code and are unaffected -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1565`.

- Fixed reference cycles through a multidict going uncollected in the
  free-threaded build of the C extension. A hash table waiting to be freed
  still owns the references of the entries left in it, and those were not
  reported to the garbage collector, which then read the values as reachable
  from outside the cycle and kept it alive. Builds with the GIL enabled emit
  byte-identical code and are unaffected -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1566`.

- Fixed ``extend()``, ``update()`` and ``merge()`` of the C extension
  raising :exc:`SystemError` when given an object whose
  ``__length_hint__()`` reported close to :data:`sys.maxsize` together
  with keyword arguments, and overflowing the table size computation for
  such a hint; a hint too large to reserve for is ignored, as
  :meth:`list.extend` does -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1577`.

- Fixed a use-after-free in the C extension's ``MultiDict.__eq__``
  when a value's ``__eq__`` mutated either multidict; comparing
  exact :class:`str` values also became faster -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1579`.

- Fixed a use-after-free in the C extension when a :class:`str` subclass
  key's ``lower()`` mutated the source while a
  :class:`~multidict.CIMultiDict` was built from, extended, updated or
  merged with a :class:`dict` or a multidict of the other case
  sensitivity. The extra references this takes are limited to the keys
  whose ``lower()`` can run Python code, so other updates do not pay for
  them -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1581`, :issue:`1598`.


Features
--------

- Added a public C and Cython API for third-party extensions. Its
  ``MultiDict_ForEach()`` family hands a visitor each entry's key, value,
  identity (the canonical key the mapping looks entries up by, lower-cased
  on a :class:`~multidict.CIMultiDict`) and the identity's hash
  -- by :user:`Vizonex` and :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1478`.

- Changed the C extension to track the entries that
  :meth:`~multidict.MultiDict.getall`, item assignment,
  :meth:`~multidict.MultiDict.update`, :meth:`~multidict.MultiDict.merge`
  and :meth:`~multidict.MultiDict.to_dict` have visited in a bitmap private
  to the call, instead of temporarily marking entry hashes in the table
  itself. Concurrent readers on free-threaded builds no longer have to
  tolerate marked entries. Item assignment,
  :meth:`~multidict.MultiDict.to_dict` and lookups of keys with many values
  got faster, while :meth:`~multidict.MultiDict.update` with many repeated
  keys got somewhat slower
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1505`.

- Replaced the C extension's per-key finder object with a single
  callback-driven walk in a new ``_multilib/walk.h`` header. The entries a
  walk has already reported were tracked by the walk itself rather than
  by every caller, and the walk kept its position in registers instead of
  reloading it from a struct on each step, so
  :meth:`~multidict.MultiDict.getall` and the items and keys view set
  operations got faster on keys with many values -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1522`.

- Sped up ``CIMultiDict`` operations keyed by a plain ``str`` in the C
  extension. An all-ASCII key was lowered directly instead of through a
  ``str.lower()`` call, and a key that is already lowercase became its own
  identity, which saved both a copy and a rehash. Looking up a lowercase key
  got about three times faster and inserting one about twice as fast, keys
  that do carry uppercase gained 20% to 35%, and bulk operations on
  already-lowercase keys, such as :meth:`~multidict.MultiDict.extend` and
  construction from a :class:`dict`, got up to three times faster
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1527`.

- Sped up building an ``istr`` in the C extension. The class gained its own
  vectorcall, so the one-argument form stopped packing its argument into a
  tuple for :c:func:`!type_call` only to unpack it again with
  :c:func:`PyArg_ParseTupleAndKeywords`. Building an ``istr`` from a
  :class:`str` got about 19% faster, and building one from an ``istr``,
  which hands back the original, about three times faster. In exchange
  :meth:`~multidict.MultiDict.getall` costs nine more instructions per
  call, since the added code moves where the compiler draws its inlining
  line -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1537`.

- Sped up building and dropping multidicts in the C extension by reusing
  freed hash tables instead of returning them to the allocator. Small
  tables are kept in a bounded per-size-class pool held in the module
  state, the way CPython reuses key objects for :class:`dict`. Only on
  builds with the GIL: a shared pool needs an atomic exchange to take a
  table and another to put one back, which measured slower than the
  per-thread allocator a free-threaded build already has
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1539`.

- Sped up :meth:`~multidict.MultiDict.copy`, constructing a multidict from
  another one, and growing one past its current size in the C extension, by
  not zeroing the parts of a hash table that are about to be overwritten. A
  copied table is overwritten in full, and a resized one has the entries it
  carries over written straight onto it -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1540`.

- Sped up :meth:`~multidict.MultiDict.items`,
  :meth:`~multidict.MultiDict.keys`, :meth:`~multidict.MultiDict.values`
  and iterating a multidict in the C extension, by reusing the view and
  iterator objects they allocate instead of returning them to the
  allocator -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1541`.

- Sped up constructing a :class:`~multidict.MultiDict`,
  :class:`~multidict.CIMultiDict`, :class:`~multidict.MultiDictProxy` or
  :class:`~multidict.CIMultiDictProxy` in the C extension, by reusing the
  object itself as well as its hash table. Subclasses are unaffected and
  keep allocating as before -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1542`.

- Sped up :meth:`~multidict.MultiDict.getall` in the C extension, by
  shrinking the scratch buffer the walk carries on the stack from 4 KB to
  1 KB. The buffer was large enough to stop the compiler inlining the walk
  into its callers, which cost more than it saved -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1543`.

- Sped up lock-free reads on the free-threaded build of the C extension.
  Every reader that leaves a :class:`~multidict.MultiDict` last used to
  call into the retired-table drain, whose first act is a sequentially
  consistent read-modify-write, even though the retired list is empty
  almost every time; a plain load took over that case and the drain
  itself moved out of line. ``key in md`` got about 10% cheaper,
  ``md[key]`` about 8% and a missing ``md.get(key)`` about 5%, measured as
  instruction counts on CPython 3.14. Builds with the GIL enabled emit
  byte-identical code and are unaffected -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1546`.

- Added a watchers C API, so another C extension can be notified when a
  :class:`~multidict.MultiDict` or :class:`~multidict.CIMultiDict` changes.
  It follows CPython's :c:func:`!PyDict_AddWatcher` family, with two
  differences: the callback is passed two context pointers, one fixed per
  registered callback and one per watched multidict, and events are
  delivered once the operation that produced them has finished rather than
  mid-mutation. The events are multi-value aware, and each one that concerns
  a key carries the identity and its hash alongside it. Up to
  :c:macro:`MULTIDICT_MAX_WATCHERS` (32) watchers can be registered, and a
  failing callback is reported through :func:`sys.unraisablehook` with the
  event and the multidict's type and address, as CPython does for dict
  watchers -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1551`.

- Sped up lookups, insertions and deletions in the C extension by always
  inlining the hash-table probe's initializer. Left to itself the compiler
  emitted it out of line, so every probe opened with a call for five
  stores. Free-threaded builds already inlined it and are unchanged
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1554`.

- Sped up every method call on :class:`~multidict.CIMultiDict` and
  :class:`~multidict.CIMultiDictProxy` in the C extension by binding
  their method descriptors to the class itself instead of inheriting
  them from :class:`~multidict.MultiDict` and
  :class:`~multidict.MultiDictProxy`; CPython only specializes a
  method call when the descriptor belongs to the exact type of the
  receiver, so every such call was taking the generic path
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1567`.

- Made :meth:`~multidict.MultiDict.popitem` in the C extension drop the
  trailing deleted entries it walked over, so that popping every item
  out of a mapping costs linear time rather than quadratic; the
  pure-Python implementation already did this -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1568`.

- Sped up iterating over :meth:`~multidict.MultiDict.items` in the C
  extension by handing out the same ``(key, value)`` tuple again when
  the caller has already let go of it, as ``for key, value in
  d.items()`` does on every step; a tuple the caller keeps is left
  alone and a new one is used from then on -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1569`.

- Sped up lookups on the free-threaded build by comparing the hash before
  touching an entry's key and by matching a key that is the very same
  object as the one looked up without taking a reference to it, so the
  common hit costs two fewer atomic operations -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1570`.

- Sped up mutations on the free-threaded build when several threads
  mutated multidicts at the same time: each thread reserved version
  numbers in batches instead of bumping one counter shared by every
  thread on each mutation, so with six threads a ``d[key] = value`` was
  about four times faster -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1587`, :issue:`1599`.

- Sped up the C extension by keeping the table resize on the insert path and
  the operations behind Python slots out of line, so they stopped crowding the
  hot paths out of the compiler's inlining budget. On GIL builds, building a
  multidict from 200 pairs got about 4% faster in instruction count and
  inserting a new key about 2%; on free-threaded builds, key lookups got up to
  5% faster -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1600`, :issue:`1601`.


Removals and backward incompatible breaking changes
---------------------------------------------------

- Made :class:`~multidict.istr` a final class to disallow subclassing in both
  the C extension and pure-Python implementations -- by :user:`agustin18`.

  *Related issues and pull requests on GitHub:*
  :issue:`1535`.


Improved documentation
----------------------

- Documented how :class:`~multidict.MultiDict` and
  :class:`~multidict.CIMultiDict` compare with :class:`dict` on both the
  default and the free-threaded builds, with per-operation measurements,
  and how to run the benchmarks in a stable environment
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1532`.

- Documented what :class:`~multidict.istr` keys are worth on a
  :class:`~multidict.CIMultiDict`, as instruction counts next to the
  equivalent plain :class:`str` keys. The advice to create them once and
  reuse them was already there; the numbers behind it were not
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1547`.

- Documented that a few rows of the benchmarking reference are measured on
  an empty or 20-item mapping rather than the 200-item one the section
  describes -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1558`.

- Mentioned in the README and docs index that ``multidict`` is optimized for
  both the GIL and free-threaded builds, and that its performance is typically
  within 20-30% of :class:`dict` for common operations, with a link to the
  benchmarks page for the full breakdown -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1559`.

- Fixed the C extension's ``popone()`` and ``pop()`` docstrings, which
  claimed the last occurrence of the key was removed; the first one is,
  as on the pure-Python backend. Added a note on rejecting duplicate
  values for keys that must carry exactly one, such as security-sensitive
  HTTP headers -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1578`.

- Added a C API example of a watcher that invalidates a cache: it unwatches
  from its own callback, watches again on the next rebuild, and gives up
  on a multidict that keeps changing -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1586`.

- Refreshed the tables in the benchmarking reference for the 7.0.0 release,
  and replaced the range quoted for the pure-Python backend with its average
  slowdown -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1602`.


Packaging updates and notes for downstreams
-------------------------------------------

- Removed the C extension code path that existed only for free-threaded
  Python 3.13, whose support was dropped in v6.8.0. The free-threaded
  build started requiring CPython 3.14 or newer and failing with a clear
  compiler error on older free-threaded interpreters; builds with the GIL
  enabled were unaffected and still go down to Python 3.10
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1524`.


Contributor-facing changes
--------------------------

- Added CodSpeed benchmarks for ``getall()`` on a large table, ``update()``
  with duplicate keys, ``merge()``, ``to_dict()`` and items view containment
  with duplicate keys -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1506`.

- Added CodSpeed benchmarks for ``getall()`` on keys with many values, both
  in a small table and in a table large enough to need a heap allocated
  bitmap -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1509`.

- Moved the compiler hint macros used by the C extension out of
  ``htkeys.h`` into ``compiler.h`` and dropped the ``HT_`` prefix from
  their names -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1510`.

- Moved the debug-only ``ASSERT_CONSISTENT`` macro and the
  ``_md_check_consistency()`` and ``_md_dump()`` helpers used by the C
  extension out of ``hashtable.h`` into their own ``debug.h`` header
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1513`.

- Added CodSpeed benchmarks for ``CIMultiDict`` keyed by plain ``str`` instead
  of ``istr``, covering already-lowercase, mixed-case and non-ASCII keys
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1521`.

- Documented that the pure-Python test leg needs ``--no-c-extensions``
  alongside ``MULTIDICT_NO_EXTENSIONS=1``; the environment variable picks
  the backend that gets imported, but only the flag deselects the
  C-extension half of the test matrix -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1523`.

- Documented how to run the Hypothesis property tests, which a default
  ``pytest`` run excludes via the ``-m "not hypothesis"`` marker
  expression in ``pytest.ini`` and whose pin ``make install-dev`` does
  not install -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1525`.

- Added ``tools/codegen_diff.py``, which reports the functions GCC emits
  differently before and after a change to the C extension. Addresses,
  RIP-relative displacements and branch offsets inside a symbol are
  normalized away first, so a single instruction-length change no longer
  makes every later function look modified, while constants and structure
  offsets are left alone so that a real change is not folded away. A tree
  that fails to build is reported by name rather than compared
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1529`.

- Fixed ``benchmarks/istr.py`` to import ``pyperf`` under its current
  name; it had been failing to start since the package was renamed from
  ``perf`` -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1531`.

- Rewrote the benchmark scripts around a single registry of benchmarked
  operations, and added a Callgrind driver that records deterministic
  instruction counts per operation. The measurements of destructive
  operations such as ``pop()`` and ``clear()`` stopped counting the cost
  of rebuilding the mapping -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1532`.

- Moved the bulk ``extend()``, ``update()`` and ``merge()`` paths of the C
  extension out of ``hashtable.h`` into their own ``bulk_update.h``
  header, leaving ``hashtable.h`` to the table itself and the single-key
  operations -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1533`.

- Moved the two list-reading helpers behind ``itemsview.__contains__()`` and
  the ``extend()``/``update()``/``merge()`` sequence parser out of
  ``hashtable.h`` and into a new ``multidict/_multilib/unpack.h``, together with
  a shared ``unpack_pair()`` that both call to read a ``(key, value)``
  pair out of an exact two-element tuple or list. C extension only; the pure
  Python implementation has no header layout to mirror
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1534`.

- Added a ``MULTIDICT_NO_FREELIST=1`` build option that stops the C
  extension reusing freed blocks. A reused block never reaches ``free()``,
  so AddressSanitizer can neither poison it nor report a use-after-free on
  it; an instrumented run wants a second pass with this set, as
  ``AGENTS.md`` describes.

  Also added a private ``_freelist_clear()`` to the C extension, for tests
  that inject an allocation failure: a reused block lets an operation run
  without calling the allocator at all, which would put the recovery path
  out of reach -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1539`.

- Added a visitor to the public C API test harnesses that mutates the multidict
  it is walking, covering the reentrancy guard in ``MultiDict_ForEach()`` from
  both ``multidict._testcapi`` and ``multidict._testcyapi``
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1544`.

- Added benchmarks for the shapes where an allocation is most of the work:
  an empty mapping, a 20-item one built or copied, a view or an iterator
  taken on its own, and a proxy. The allocation an operation makes is a
  fixed cost, so at the existing 200-item size it was divided across 200
  entries and all but vanished; ``d.items()`` measured 1.7% faster where
  it is 46% faster -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1556`.

- Sorted ``docs/spelling_wordlist.txt`` case-insensitively, and documented
  in ``AGENTS.md`` that it is to be re-sorted rather than appended to. The
  list had grown in append order, which made a word hard to find and put
  every PR adding one on the same last line -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1557`.

- Stopped asking every performance pull request to regenerate the tables in
  the benchmarking reference, since each run shifts every row slightly and the
  churn hides the rows that moved, and moved the refresh to once per release.
  Added :file:`RELEASE.md`, which documents the release procedure, including
  that refresh -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1564`.

- Renamed the C extension's internal helpers in the ``_multilib`` headers
  so that a leading underscore marks exactly the functions used only
  within their own header; type slots and method callbacks keep their
  names -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1571`.

- Spelled out in ``AGENTS.md`` that every verb in a news fragment is
  past tense, and added a bad and good example pair plus a matching
  "Things not to do" entry -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1588`.

- Changed the release workflow to rewrite the Sphinx roles of the changelog
  section as Markdown before publishing it as the GitHub Release body, so
  that class, method and exception references, contributor handles and
  documentation links render there instead of showing as raw markup
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1596`.


Miscellaneous internal changes
------------------------------

- Changed the internal ``htkeys_build_indices()`` helper in the C
  extension to return ``void``; it cannot fail, so both callers were
  checking for an error that never occurred -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1516`.

- Routed every field of the C extension's hash table that more than one
  thread can reach through a named accessor in the new
  ``_multilib/freethreading.h`` header, instead of each call site choosing
  between an atomic and a plain access behind ``#ifdef Py_GIL_DISABLED``.
  Each accessor kept the operation and memory ordering its call sites
  already used, so neither build changed behavior -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1518`.

- Removed ``get_mod_state_by_cls()``, an unused helper in the C extension
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1520`.

- Documented why ``md_clear()`` keeps clearing entry fields with
  ``Py_CLEAR()`` on the GIL build rather than routing them through
  ``_multilib/freethreading.h``: ``Py_CLEAR()`` skips the store when a
  field is already ``NULL``, so no accessor there is a substitute for it
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1526`.

- Collapsed the two ``#ifdef Py_GIL_DISABLED`` arms that fill in a freshly
  inserted hash table entry in the C extension into one. Both builds started
  writing the entry's fields in the order the free-threaded build needs, and
  publishing the value without first loading and releasing the one already
  there, since a fresh entry holds none -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1528`.

- Merged the C extension's deferred-decref accumulator into ``reflist_t``,
  so one collector of strong references, in the ``_multilib/reflist.h``
  header, came to serve both the callers that build a result list and the
  ones that postpone a decref until a mutation finishes. Its storage became an
  inline array that spills into a chain of heap blocks, which also made
  :meth:`~multidict.MultiDict.getall` a little faster on keys with many
  values, since the collected values stopped being copied to a bigger
  buffer as they accumulate -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1530`.

- Dropped two module state lookups in the C extension that had already been
  answered elsewhere. ``MultiDict.__init__()`` and ``CIMultiDict.__init__()``
  switched to reading the state pointer that ``tp_new()`` stored on the object instead
  of resolving it from the type a second time, and an ``istr`` stopped
  carrying a state pointer of its own, which was written on every creation
  and never read. Constructing a subclass of either container got about 1%
  cheaper, and every ``istr`` is eight bytes smaller
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1536`.

- Narrowed the critical section of the single-key methods (``add()``,
  ``__setitem__()``, ``__delitem__()``, ``setdefault()``, ``pop()``,
  ``popone()``, ``popall()`` and ``getall()``): a key is converted to its
  identity and hashed before the lock is taken, rather than while a
  free-threaded build holds the multidict locked -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1538`.

- Changed ``MultiDict_ForEach()`` to walk every entry with a linear scan
  instead of the resumable iterator cursor, which its caller does not need while
  it holds the critical section for the whole walk -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1544`.

- Sped up the argument parsing of the C extension. The all-positional form
  of every method taking ``key`` started binding its arguments inline instead of
  calling into the keyword machinery, and keyword names were matched against
  interned parameter names by identity before falling back to a string
  comparison -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1549`.

- Stopped building the implicit :data:`None` default of ``MultiDict.get()``
  on every call in the C extension, and produced it only when the key is
  actually missing -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1549`.

- Laid out the common branch in line in the single-key lookup probe loop and in
  the key to identity conversion: the deleted-slot check and the ``str``
  subclass check were marked as the rare cases, so the compiler stopped putting
  the frequent path behind a taken branch -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1550`.

- Stopped building the implicit :data:`None` default of
  ``MultiDict.setdefault()`` on every call in the C extension -- by
  :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1553`.

- Reordered the fields of the C extension's multidict object so the ones a
  lookup reads share a cache line, and dropped its separate module
  reference in favor of the one the module state already holds, which
  made every multidict 16 bytes smaller -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1584`.

- Dropped the C extension's ``multidict_tp_traverse()`` and
  ``multidict_tp_clear()`` wrappers; ``md_traverse()`` and ``md_clear()``
  now serve as the type's GC slots directly -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1589`.

- Removed a redundant ``NULL`` check and a stray ``PyErr_Format()``
  argument from the C constructors, and an always true condition from the
  pure Python ``getall()`` -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1590`.

- Merged the duplicated ``__init__()`` bodies of the C ``MultiDict`` and
  ``CIMultiDict`` into one helper -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1591`.

- Replaced the allocate, set state and take the module reference sequence
  repeated across the C constructors with one helper -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1592`.

- Renamed internal helpers whose names no longer matched what they did,
  and fixed misspelled docstring identifiers in the C extension
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1593`.

- Renamed the C extension's functions that are used only as type slots
  after the slot they fill, as ``<type>_tp_<slot>``, ``<type>_nb_<slot>``,
  ``<type>_sq_<slot>`` or ``<type>_mp_<slot>`` -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1594`.

- Replaced the custom ``tp_new`` slots that blocked direct instantiation of
  the C view and iterator types with ``Py_TPFLAGS_DISALLOW_INSTANTIATION``
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1595`.


----


6.9.1
=====

*(2026-09-21)*


Bug fixes
---------

- Fixed the C extension reading freed memory on free-threaded builds when a
  list handed to :py:meth:`~multidict.MultiDict.update`,
  :py:meth:`~multidict.MultiDict.extend`, :py:meth:`~multidict.MultiDict.merge`
  or the :py:class:`~multidict.MultiDict` and :py:class:`~multidict.CIMultiDict`
  constructors, a ``[key, value]`` item inside any iterable handed to them, or a
  list tested with ``in`` against :py:meth:`~multidict.MultiDict.items`, is
  changed by another thread; a call that catches the list shrinking under it
  now raises :py:exc:`RuntimeError` -- by :user:`rodrigobnogueira`.

  *Related issues and pull requests on GitHub:*
  :issue:`1437`.

- Fixed a data race on the free-threaded build where a retired hash table's
  reader count used relaxed atomics, letting a lock-free ``get()``/``getone()``/
  ``__getitem__()`` read race a concurrent free of that table. The reader-exit
  decrement and the drain's free check now use release/acquire ordering
  instead -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1481`.

- Fixed a free-threaded build bug where two threads calling ``update()``,
  ``merge()``, or ``__setitem__()`` on the same key at the same time could lose
  the key entirely instead of just racing on which value wins. A decref of the
  replaced value could transiently suspend the writer's critical section,
  letting a second writer for the same key observe the first writer's
  in-progress entry as absent and, once both settled, mistake it for a stale
  duplicate and delete it. Every such decref is now deferred until the writer
  has released its critical section, so the window can no longer open.
  ``setdefault()`` had an unrelated instance of the same blind spot (it could
  insert a duplicate rather than recognizing an in-flight key), fixed alongside
  it -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1483`.

- Fixed a free-threaded build bug where ``getall()`` and the ``items()``/
  ``keys()``/``values()`` equality path could raise ``KeyError`` or report a
  present, never-deleted key as missing. A concurrent ``update()``/``extend()``/
  ``__setitem__()`` call can have its critical section transiently suspended
  (a decref triggering a blocking allocator call) while an entry is marked as
  part of its own bookkeeping; a reader landing in that window used to treat
  the mark as "not found" instead of "still there, in flight" -- by
  :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1484`.

- Fixed a reference leak in the C extension where ``operand | md.items()``
  and ``md.items() - operand`` leaked one key and one value reference per
  element of ``operand``, letting a large operand grow memory without bound
  (:gh:`GHSA-54p9-h82j-f925 <aio-libs/multidict/security/advisories/GHSA-54p9-h82j-f925>`)
  -- by :user:`asvetlov`.

  The issue was reported by :user:`waydeshi`.

  *Related commits on GitHub:*
  :commit:`350b4a0`.

- Fixed a segmentation fault on the standard (non-free-threaded) C extension
  build when a value type's ``__del__`` released the GIL (for example by
  calling ``time.sleep()``) while ``update()``, ``merge()``, ``__setitem__()``,
  ``__delitem__()``, ``pop()``, ``popone()``, or ``popall()`` was dropping a
  replaced or removed value. ``Py_BEGIN_CRITICAL_SECTION`` compiles to a no-op
  on this build, so nothing else was stopping a second thread from mutating the
  very same ``MultiDict`` concurrently once the GIL was released mid-mutation.
  Every such decref is now deferred until the mutation has fully finished, the
  same technique already used to close the analogous free-threaded-build race
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1489`.

- Fixed the C extension reading freed memory while iterating a
  :py:class:`~multidict.CIMultiDict` whose keys are plain :py:class:`str`.
  Converting such a key to :py:class:`~multidict.istr` could run Python code
  (a :py:class:`str` subclass's ``__str__`` or ``__del__``) or, on free-threaded
  builds, suspend the iterator's critical section, after which the iterator read
  the entry again even though a concurrent mutation could already have freed it.
  As part of the fix, :py:meth:`~multidict.MultiDict.copy` and re-initializing
  from another multidict now assign a new version in the C extension instead of
  reusing the source's, matching the pure Python implementation
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1496`.

- Fixed a use-after-free on the free-threaded build where a lock-free
  ``get()``, ``[]`` or ``in`` could read a hash table that a concurrent
  resize had just retired and another reader was freeing
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1497`.


Contributor-facing changes
--------------------------

- Removed a redundant ``include`` and a duplicated ``exclude`` line from
  ``MANIFEST.in``; sdist contents are unchanged -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1478`.

- Added ``.claudeignore`` file -- by :user:`asvetlov`

  *Related issues and pull requests on GitHub:*
  :issue:`1479`.

- Scaled up the pure-Python ``pop()``, ``popitem()``, ``__delitem__()``,
  ``add()`` and item-insertion benchmarks to do more work per measurement.
  Repeated CodSpeed runs on the same commit showed these particular
  benchmarks flagged as dominated by syscalls, understating their real
  cost and adding noise to the reported values; a larger working set
  amortizes that overhead -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1485`.

- Replaced deprecated *instrumentation* codspeed mode with *simulation* -- by :user:`asvetlov`

  *Related issues and pull requests on GitHub:*
  :issue:`1493`.

- Reorganized the mutating benchmarks (item insertion, ``update()``,
  ``add()`` of the same key, ``pop()``, ``popitem()``, ``clear()``,
  ``__delitem__()`` and ``__setitem__()``) to copy a fresh multidict and
  apply the operation in a loop, like the ``add()`` and ``extend()``
  benchmarks already do. The insertion, ``update()`` and ``clear()``
  benchmarks previously mutated a single multidict shared across
  rounds, so only the first round measured the intended operation; the
  rest did a single copy per round, letting per-round overhead dominate
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1494`.

- The ``repr()`` and view inequality benchmarks were updated to repeat their
  operation in a loop, like the other benchmarks, and the CodSpeed benchmark
  job was moved to Python 3.14 -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1498`.

- Dropped ``-I`` from the AddressSanitizer test command in ``AGENTS.md``
  and in the CI job. It implies ``-E``, which made Python ignore
  ``PYTHONMALLOC=malloc``, so small hash tables were still served from
  ``pymalloc`` arenas where use-after-free went undetected
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1499`.

- The CI/CD workflow was updated to stop superseded runs of the same pull request
  when a new commit is pushed; runs on ``master``, release branches, tags,
  the merge queue, and the daily schedule are never interrupted
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1500`.

- The release job was changed to upload distributions and their signatures
  to the GitHub Release one file at a time, skipping assets that were
  already attached and retrying after a pause, so that a parallel upload
  burst no longer tripped the GitHub secondary rate limit
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1503`.


Miscellaneous internal changes
------------------------------

- Corrected several comments in the free-threaded C extension that
  attributed critical-section suspension to a blocking ``PyMem_Malloc()``
  call; allocation alone never suspends an acquired critical section, and
  the real risk at those sites is a decref running
  a finalizer or weakref callback. Also dropped a retry loop in
  ``md_clone_from_ht()`` that guarded against the same, non-existent
  allocation-triggered suspension -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1486`.

- Changed the free-threaded build's deferred decref buffer, used by
  ``update()`` and ``__setitem__()``, to a chain of fixed-size blocks
  with a large inline first block instead of a small inline array that
  was reallocated on growth -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1501`.


----


6.9.0
==========

*(2026-09-18)*


Bug fixes
---------

- Protected ``repr()`` of ``MultiDict``, ``MultiDictProxy``, and their views
  in the C extension with a critical section, avoiding data races on the
  free-threaded build of CPython -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1431`.

- Guarded ``repr()`` of ``MultiDictProxy`` in the C extension and of
  ``KeysView`` in both the C extension and the pure-Python
  implementation against infinite recursion on self-referential
  containers, matching the existing guard on ``MultiDict``,
  ``ItemsView``, and ``ValuesView`` -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1431`.

- Protected ``MultiDict.update()``, ``.extend()``, ``.merge()``, ``.clear()``,
  ``.copy()``, and the ``MultiDict``/``CIMultiDict`` constructors in the C
  extension with a critical section, using the two-object form when a
  second multidict, multidict proxy, or plain ``dict`` instance is
  involved, avoiding data races and a segmentation fault on the
  free-threaded build of CPython. ``.clear()`` now also publishes the
  empty table before releasing any entry's references, so a concurrent
  caller can never observe a partially-cleared multidict even if releasing
  a value runs arbitrary Python code that suspends the held critical
  section -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1433`.

- Protected ``MultiDict.add()``, ``__setitem__``/``__delitem__``,
  ``get()``/``getone()``/``__getitem__``, ``__contains__``, ``getall()``,
  ``setdefault()``, ``pop()``/``popone()``/``popall()``/``popitem()``,
  ``__eq__``, iteration, and the ``&``/``|``/``-``/``^``/``in``/
  ``isdisjoint()`` operations on ``.keys()`` and ``.items()`` views in the
  C extension with a critical section, avoiding data races and
  use-after-free crashes on the free-threaded build of CPython
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1438`.

- Fixed a data race on the free-threaded build where ``MultiDictObject.used``
  was written non-atomically while ``len()`` read it with a relaxed atomic
  load -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1441`.

- Fixed several pre-existing use-after-free races on the free-threaded build: ``_md_resize()``
  and ``md_clone_from_ht()`` could allocate a new hash table, have their
  critical section transiently suspended during that allocation, and then use
  a keys-table pointer or size a concurrent operation had already invalidated;
  ``_md_del_at()`` (plain ``del``/``pop()``) could likewise leave ``self`` in
  an inconsistent state across a suspending decref -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1441`.

- Fixed a data race on the free-threaded build where ``md->keys`` was
  published with a plain store while lock-free readers loaded it
  atomically, and a false-negative race in ``get()``/``__contains__``
  where a hash temporarily marked by a concurrent replace could make a
  present key look absent -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1441`.

- Fixed several more pre-existing use-after-free and data-corruption races on
  the free-threaded build, this time in the ``__setitem__()``/``update()``/
  ``extend()``/``merge()`` replace path: a decref suspending the critical
  section mid-replace could leave a stale table pointer in use, misplace a
  temporarily-marked entry during a concurrent resize, or let one thread's
  duplicate-tracking mark get overwritten by an unrelated key's entry sharing
  the same hash bucket -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1441`.

- Fixed an unbounded memory leak on the free-threaded build where a retired
  hash table could sit on ``md->retired`` for the rest of the object's
  lifetime under sustained concurrent read traffic instead of being freed
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1443`.

- Fixed a data race on the free-threaded build of the C extension where
  ``state->global_version``, the version counter shared by every
  multidict instance and used to derive ``getversion()``, was bumped with
  a plain increment instead of an atomic op. Two threads mutating
  different instances at the same time could step on each other's update
  and hand out a duplicate (or non-monotonic) version number
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1444`.

- Fixed a data race on the free-threaded build of the C extension where
  ``getall()``/``popall()`` and ``update()``/``extend()``/``merge()``
  temporarily marked and unmarked a matching entry's hash with a plain,
  non-atomic store, while a lock-free ``__contains__``/``get()`` on another
  thread could load that same field with an atomic operation and no lock at
  all -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1448`.

- Fixed a data race in the pure-Python ``MultiDict``/``CIMultiDict``
  fallback where concurrent mutating calls (``add()``, ``__setitem__``,
  ``__delitem__``, ``setdefault()``, ``pop()``/``popone()``,
  ``popall()``, ``popitem()``, ``update()``, ``extend()``, ``merge()``,
  ``clear()``, and re-``__init__``) could corrupt the shared hash table,
  or silently drop a concurrent write, on a regular, GIL-enabled
  interpreter. Pure-Python bytecode is not atomic under the GIL, so two
  threads could interleave mid hash-table insert or deletion, leaving
  the index table pointing at stale or out-of-range entries and causing
  an infinite probe loop, an ``AttributeError``, or a lost mutation.
  Each ``MultiDict``/``CIMultiDict`` instance now holds its own lock for
  the duration of these operations, with two-object operations locking
  both instances in a fixed order to avoid deadlock. The C extension
  already serializes these operations with a critical section and was
  not affected
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1449`.

- Fixed a data race on the free-threaded build of the C extension where
  ``to_dict()`` cleared every entry's temporary "seen" mark with a plain,
  non-atomic store, while a lock-free ``__contains__``/``get()`` on another
  thread could load that same field with an atomic operation and no lock at
  all -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1450`.

- Fixed a data race on the free-threaded build of the C extension where
  ``update()``/``extend()``/``merge()`` read ``self``'s module state without
  holding ``self``'s lock, while a concurrent ``__init__()`` on the same,
  already-published multidict could rewrite that same field under lock.
  Rather than routing every read through a lock-free lookup, the field is
  now written only once, at object construction, and never touched again by
  ``__init__()``, so it is safe to read unlocked anywhere -- by
  :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1452`.

- Fixed ``items()`` set algebra (``&``, ``|``, ``-``, ``in``, ``isdisjoint()``) so it no
  longer corrupts or hangs when a compared value's ``__eq__()`` re-enters the
  same :class:`~multidict.MultiDict`, for example by calling ``getall()`` on
  it. The comparison now runs only after the internal hash-chain walk has
  been fully materialized and restored, so the callback can no longer
  observe entries the walk still has marked -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1453`.

- Fixed a data race on the free-threaded build of the C extension where
  ``popall()``/``popone()``/``__delitem__``/``add()`` rewrote a hash table
  index slot with a plain, non-atomic store, while a lock-free
  ``get()``/``getone()``/``__getitem__``/``__contains__`` on another thread
  walked that same index array with a plain, non-atomic load and no lock at
  all -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1454`.

- Stopped folding the case of :class:`~multidict.istr` keys in the
  case-sensitive :class:`~multidict.MultiDict`, so both the C and the pure
  Python implementations now keep such a key exactly as given
  -- by :user:`youdie006`.

  *Related issues and pull requests on GitHub:*
  :issue:`1457`.

- Fixed the C extension's ``update()`` (and ``merge()``, which shares the same
  code path) silently failing to invalidate an in-progress ``keys()``/
  ``items()``/``values()`` iterator when the call only overwrote an
  already-present key's value in place. Adding a new key already invalidated
  iterators correctly; only the in-place overwrite branch was missing the
  version bump -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1458`.

- Fixed the pure-Python ``MultiDict``/``CIMultiDict`` iterator guard so that
  mutating the mapping before the first ``next()`` call on an ``items()``,
  ``keys()``, or ``values()`` iterator (or on ``iter(md)``) now reliably raises
  ``RuntimeError``, matching the C extension. Previously a mutation that
  happened before the iterator was ever advanced, such as ``clear()`` or a
  ``del``/``popone()`` that removed the only remaining entry, could make the
  iterator silently raise ``StopIteration`` instead -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1459`.

- Fixed a use-after-free race on the free-threaded build of the C extension
  where a retired hash table could be freed while a lock-free
  ``get()``/``getone()``/``__getitem__``/``__contains__`` reader on another
  thread was still walking it: the coarse "no reader in flight" gate
  reaching zero did not reliably mean every such reader had also reached
  its own per-table exit, so a table whose own reader count is still
  nonzero is now deferred for a later retry instead of freed
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1461`.


Features
--------

- Added a :meth:`~multidict.MultiDict.to_dict` method returning a plain
  :class:`dict` that maps every key to the list of all its values. Unlike
  ``dict(md)``, which keeps only the first value per key, and unlike a
  ``{k: md.getall(k) for k in md}`` comprehension, which emits one entry per
  spelling of a case-insensitive key, it groups by key identity
  -- by :user:`rodrigobnogueira`.

  *Related issues and pull requests on GitHub:*
  :issue:`783`.

- ``MultiDict``, ``CIMultiDict``, ``MultiDictProxy`` and ``CIMultiDictProxy`` in the
  C extension implemented the vectorcall calling convention for construction.
  This sped up ``MultiDict(...)``-style calls by avoiding an intermediate
  positional-arguments tuple and keyword-arguments dictionary in the common case; the gained burst is ~10.
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1436`.

- Made ``MultiDict.__contains__()``, ``.get()``, ``.getone()`` and
  ``__getitem__()`` lock-free on CPython 3.14+'s free-threaded build, instead
  of taking a critical section like the rest of the C extension's API (on
  3.13, which lacks the public API these need to safely read an entry
  concurrently, they still take the critical section, exactly as before). A
  resize/shrink/clear no longer frees the outgoing hash table immediately; it
  is deferred until no lock-free reader could still be walking it -- by
  :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1441`.

- Made the pure-Python ``MultiDict``/``CIMultiDict`` implementation safe to
  use from multiple threads under a free-threaded (no-GIL) build. Every
  method that touches an instance's hash table now takes that instance's own
  lock, two-object operations (``update()``, ``extend()``, ``merge()``,
  ``__eq__()``, copying) lock both objects in a fixed, deadlock-safe order,
  and the version counter shared by every instance is guarded separately. On
  a regular (GIL-enabled) interpreter this adds no overhead: the locked
  methods are the exact same function objects as before -- by
  :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1447`.

- Sped up adding many values for the same key to a
  :class:`~multidict.MultiDict` or :class:`~multidict.CIMultiDict`, each
  added value no longer gets slower than the one before it
  -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1466`.


Improved documentation
----------------------

- Added the plural form "fallbacks" to the docs spell checker's allowed
  word list so :file:`CHANGES.rst` builds cleanly -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1439`.


Contributor-facing changes
--------------------------

- Added race tests that iterate and extend a :class:`~multidict.MultiDict`
  from several threads on a free-threaded build, so a regression in the C
  extension's locking fails a free-threaded CI leg instead of going unnoticed
  -- by :user:`rodrigobnogueira`.

  *Related issues and pull requests on GitHub:*
  :issue:`1317`.

- Added ``AddressSanitizer``/``UndefinedBehaviorSanitizer`` and
  ``ThreadSanitizer`` CI jobs. The former runs the suite under a
  regular CPython with ``MULTIDICT_ASAN_BUILD=1``; the latter builds a
  free-threaded CPython instrumented with ``--with-thread-sanitizer``
  and runs the suite against it via ``MULTIDICT_TSAN_BUILD=1`` -- by
  :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1455`.

- Added Hypothesis-based property and stateful fuzz tests covering
  :class:`~multidict.MultiDict`/:class:`~multidict.CIMultiDict` semantics,
  views, iterators, and threaded stress scenarios, run against both the
  C-extension and pure-Python backends -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1460`.

- Added core dump collection to the AddressSanitizer and ThreadSanitizer
  CI jobs: a crash now uploads the core file alongside the crashing
  interpreter binary and the compiled extension as a downloadable
  artifact, for offline debugging with a matching symbol-carrying binary
  instead of raw hex offsets -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1462`.

- Added benchmarks for adding many values for the same key and for
  creating a multidict with many items -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1465`.

- Skipped abstractmethods from coverage leaks report, they are empty placeholders that are never executed -- by :user:`asvetlov`

  *Related issues and pull requests on GitHub:*
  :issue:`1470`.

- Add benchmarks for ``__setitem__`` -- by :user:`asvetlov`

  *Related issues and pull requests on GitHub:*
  :issue:`1471`.

- The AddressSanitizer and ThreadSanitizer CI jobs were changed to run pytest
  with ``--capture=no``. Previously a sanitizer abort could exit the process
  before pytest flushed its per-test output buffer, silently dropping the
  sanitizer report from the job log -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1472`.

- Moved the ``hypothesis`` pin out of ``requirements/pytest.txt`` into its own
  ``requirements/pytest-hypothesis.txt``, installed only by the dedicated
  Hypothesis CI jobs. ``hypothesis`` now ships a Rust extension with no
  prebuilt wheel for some emulated architectures (e.g. musllinux i686),
  which broke wheel-testing jobs that never run the Hypothesis-marked tests
  in the first place -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1475`.

Miscellaneous internal changes
------------------------------

- Replaced ``PyIter_Next()`` calls with the newer ``PyIter_NextItem()`` API in
  the C extension, so the iterator-exhausted and error cases are told apart by
  the return code instead of an ambiguous ``NULL`` result -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1440`.


----


6.8.0
=====

*(2026-09-09)*


Bug fixes
---------

- A segmentation fault that could be triggered when getting an item is now fixed
  -- by :user:`Vizonex`.

  *Related issues and pull requests on GitHub:*
  :issue:`1310`.

- Fixed reference leak in iterators, views and ``istr``
  -- by :user:`Vizonex`.

  *Related issues and pull requests on GitHub:*
  :issue:`1311`.

- Fixed the pure-Python :class:`~multidict.MultiDict` constructor and
  :py:meth:`~multidict.MultiDict.extend`,
  :py:meth:`~multidict.MultiDict.update`, and
  :py:meth:`~multidict.MultiDict.merge` methods over-allocating their
  internal hash table when called with both a positional argument and
  keyword arguments, because keyword arguments were counted twice in the
  size estimate -- by :user:`aiolibsbot`.

  *Related issues and pull requests on GitHub:*
  :issue:`1338`.

- Fixed ``__repr__`` of :class:`~multidict.MultiDict`,
  :class:`~multidict.CIMultiDict`, their proxies, and the keys/items views
  producing invalid output when keys contained quote characters --
  keys are now formatted with :func:`repr` so the result is a valid Python
  string literal -- by :user:`aiolibsbot`.

  *Related issues and pull requests on GitHub:*
  :issue:`1342`.

- Fixed a segfault when calling :py:meth:`~multidict.MultiDict.add` with only one of its two required arguments supplied by keyword, e.g. ``d.add(key="k")``. Extra keyword arguments passed to the lookup and removal methods are also now rejected with :exc:`TypeError` instead of being silently ignored.

  -- by :user:`devdanzin`

  *Related issues and pull requests on GitHub:*
  :issue:`1376`.

- Fixed a segfault when constructing a multidict iterator type directly, e.g. ``type(iter(md.keys())).__new__(...)``. Such an iterator had a NULL internal pointer that ``next()`` dereferenced. The iterator types now forbid direct instantiation, the same way the view types were fixed in :issue:`1163`.

  -- by :user:`devdanzin`

  *Related issues and pull requests on GitHub:*
  :issue:`1377`.

- Fixed a segfault when using a :py:class:`~multidict.MultiDict` or :py:class:`~multidict.CIMultiDict` created via ``__new__`` without calling ``__init__`` (for example a subclass that does not call ``super().__init__()``). The internal state was left as NULL pointers that the first method call dereferenced. ``tp_new`` now initializes the object to a valid empty mapping.

  -- by :user:`devdanzin`

  *Related issues and pull requests on GitHub:*
  :issue:`1378`.

- Fixed two crashes in the C extension caused by holding a raw pointer into a hash table across an operation that could reshape it. Updating a multidict from itself (e.g. ``d.extend(d)``) freed the very table being iterated -- a use-after-free; ``extend(self)`` now doubles the contents and ``update(self)``/``merge(self)`` are no-ops. Building a multidict from a list of pairs whose case-insensitive key ``.lower()`` shrinks that list read past the end of the list; the length is now re-checked on every iteration.

  -- by :user:`devdanzin`

  *Related issues and pull requests on GitHub:*
  :issue:`1379`.

- Fixed three reference/resource leaks on error paths in the C extension: the items-view ``__contains__`` leaked the first element of a candidate pair when reading the second one raised; ``__repr__`` leaked its ``PyUnicodeWriter`` when the multidict was mutated during iteration; and the internal iteration helper leaked the identity reference when key materialization failed under low memory.

  -- by :user:`devdanzin`

  *Related issues and pull requests on GitHub:*
  :issue:`1381`.

- Stopped several feature-detection fallbacks in the C extension from swallowing every exception. When probing an argument (``arg.items()``/``arg.keys()``) or measuring it (``len(other)``) fails, the code now clears only the expected :exc:`TypeError`/:exc:`AttributeError` and lets everything else -- notably :exc:`MemoryError` and :exc:`KeyboardInterrupt` -- propagate, matching the already-correct sites elsewhere in the module.

  -- by :user:`devdanzin`

  *Related issues and pull requests on GitHub:*
  :issue:`1382`.

- Fixed a segmentation fault when extending a multidict with itself
  -- by :user:`cananoo` and :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1398`.

- Fixed a memory leak when ``MultiDict`` and ``CIMultiDict`` instances are
  initialized more than once -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1412`.

- Fixed a reference leak in items-view set operations
  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1413`.

- Fixed a memory leak when destroying C-extension multidict instances
  and proxies -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1415`.

- Fixed a reference leak from repeated ``__init__`` calls on
  ``MultiDictProxy`` and ``CIMultiDictProxy`` instances -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1419`.

- Fixed missing decref for :data:`None` on error in :meth:`~multidict.MultiDict.setdefault`
  -- by :user:`asvetlov`

  *Related issues and pull requests on GitHub:*
  :issue:`1421`.


Features
--------

- Added :py:func:`reversed` support to the keys, values, and items views of
  :class:`~multidict.MultiDict`, :class:`~multidict.CIMultiDict`, and their
  proxies in both the C-extension and pure-Python implementations
  -- by :user:`aiolibsbot`.

  *Related issues and pull requests on GitHub:*
  :issue:`448`.

- Optimized key identity comparison for C Extension.

  Now it uses fast path that is equal to :c:func:`PyUnicode_Equal` from
  Python 3.14+ but without redundant type checks. It gives ~15% speed-up on benchmarks with many key comparisons.

  -- by :user:`asvetlov`

  *Related issues and pull requests on GitHub:*
  :issue:`1406`.

- Changed both the pure-Python and C implementations to mark a temporarily
  removed hash table entry by setting the high bit of its hash instead of
  overwriting it with a sentinel value, so restoring the entry no longer
  recomputes the hash; :meth:`~multidict.MultiDict.getall` is faster ~10% now in C version
  -- by :user:`asvetlov`

  *Related issues and pull requests on GitHub:*
  :issue:`1426`.


Removals and backward incompatible breaking changes
---------------------------------------------------

- Dropped support for Python 3.9 as it has reached end of life.

  *Related issues and pull requests on GitHub:*
  :issue:`1316`.

- Dropped support for free-threaded Python 3.13 -- by :user:`ngoldbaum`.

  *Related issues and pull requests on GitHub:*
  :issue:`1326`.


Improved documentation
----------------------

- Fixed ``CIMultiDictProxy`` documentation to state it inherits from
  ``MultiDictProxy`` (not ``MultiDict``), and fixed missing word in
  ``istr`` section -- by :user:`veeceey`.

  *Related issues and pull requests on GitHub:*
  :issue:`1298`.

- Clarified that ``istr`` preserves the original casing and
  compares as a regular ``str``; case-insensitive matching is
  handled by ``CIMultiDict`` -- by :user:`gyanu2507`.

  *Related issues and pull requests on GitHub:*
  :issue:`1397`.

- Fixed broken RST markup in ``items()`` docstrings
  -- by :user:`veeceey`.

  *Related issues and pull requests on GitHub:*
  :issue:`1299`.


Packaging updates and notes for downstreams
-------------------------------------------

- Dropped support for free-threaded Python 3.13 -- by :user:`ngoldbaum`.

  *Related issues and pull requests on GitHub:*
  :issue:`1326`.

- Wheels for iOS and Android (CPython 3.13+) are now published on
  release-tag builds, so downstreams on those platforms no longer need a
  local compiler to install ``multidict``; these wheels are build-verified
  only, since running the test suite on mobile needs a simulator or an
  emulator -- by :user:`aiolibsbot`.

  *Related issues and pull requests on GitHub:*
  :issue:`1337`.

- Added support for building and shipping riscv64 wheels
  -- by :user:`justeph`.

  *Related issues and pull requests on GitHub:*
  :issue:`1293`.

- The ``setuptools`` build dependency lower bound has been restored to be
  ``>= 47`` after an incorrect automated increase in :pr:`1315`
  -- by :user:`aiolibsbot`.

  *Related issues and pull requests on GitHub:*
  :issue:`1400`.


Contributor-facing changes
--------------------------

- Added support for collecting code coverage of isolated multidict tests
  -- by :user:`Vizonex`.

  *Related issues and pull requests on GitHub:*
  :issue:`1314`.

- Switched to ``mirrors-clang-format`` and enabled clang-format in ``pre-commit.ci``
  -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1318`.

- Added a release-tag-gated CI job that cross-compiles the C extension
  for iOS and Android through ``cibuildwheel`` -- by :user:`aiolibsbot`.

  *Related issues and pull requests on GitHub:*
  :issue:`1337`.

- Added an ``AGENTS.md`` orientation file at the repository root, covering the
  pull request template, the ``CHANGES/`` news fragment conventions, the
  draft-PR / human-review workflow, and the dual pure-Python and C-extension
  parity rule, so LLM contributors land changes that match project style
  -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1339`.

- Added ``init``, ``installable``, ``instantiation``, ``parametrization``,
  ``parametrized``, ``postfix``, and ``unparseable`` to the docs spelling list
  so news fragments and docs can use these words without failing the spell
  check build -- by :user:`pctablet505`.

  *Related issues and pull requests on GitHub:*
  :issue:`1343`.

- Documented three common agent failure modes in
  :file:`AGENTS.md`: the docs spell check
  (``make doc-spelling``) catches unknown words in news fragments
  before CI does; coverage runs over the test tree too, so
  unreachable defensive ``raise`` guards and one-sided cleanup
  branches in tests will surface as uncovered on the codecov
  patch report; and branch creation is restricted on
  ``aio-libs/multidict``, so PRs must be pushed from a fork
  -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1343`, :issue:`1345`.

- Trimmed the CI test matrix to drop redundant ``Py_DEBUG`` jobs on
  macOS and Windows and to gate the ``windows-11-arm`` wheel build to
  release-time only, cutting CI wall-clock by roughly a third without
  reducing code coverage -- by :user:`aiolibsbot`.

  *Related issues and pull requests on GitHub:*
  :issue:`1346`.

- Added a ``CLAUDE.md`` at the repository root that imports
  :file:`AGENTS.md` via Claude Code's ``@``-syntax, so the
  project's LLM contributor rules load automatically when
  working in Claude Code
  -- by :user:`aiolibsbot`.

  *Related issues and pull requests on GitHub:*
  :issue:`1347`.

- Enabled the ``I`` (import sorting) rule in
  ``[tool.ruff.lint]`` so ``ruff check`` covers import order on top
  of the existing ``UP`` (pyupgrade) group, and updated the
  ``ruff-check`` pre-commit hook to run with
  ``--fix --exit-non-zero-on-fix --show-fixes`` so import-order
  fixes apply on commit. Sorted imports in four ``tests/`` files
  to match the new rule -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1348`.

- Enabled the ``PLC0415`` (import-outside-top-level) rule in
  ``[tool.ruff.lint]``. Function-scoped imports must now opt in
  with ``# noqa: PLC0415`` plus a comment explaining the reason
  (typically avoiding a heavy optional dependency at import time).
  This catches a pattern that LLM contributors frequently introduce.
  There are currently no opt-outs in the tree, so the rule is
  enforced everywhere ``ruff check`` runs
  -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1349`.

- Switched the ``cibuildwheel`` build frontend to ``build[uv]`` so
  that ``uv`` provisions every build and test virtual environment
  in the wheel matrix. Test-dependency installation in particular
  drops from a multi-second ``pip install`` per ABI to a roughly
  sub-second ``uv`` resolve
  -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1350`.

- Overrode ``CIBW_BUILD_FRONTEND=build`` for the odd-arch wheel
  matrix; the upstream ``manylinux``/``musllinux`` images for
  those arches do not ship ``uv``
  -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1352`.

- Allowed re-running the deploy job after a partial release failure: the
  ``Make Release`` step now skips when the GitHub Release already exists,
  and the PyPI publish step uses ``skip-existing`` so dists that were
  already uploaded on a prior attempt do not break the retry
  -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1353`.

- Switched the aarch64 and armv7l wheel builds to GitHub's native ARM
  runners. The aarch64 wheels now build without QEMU emulation, and
  armv7l runs on aarch64 hosts so its 32-bit ARM execution is far
  cheaper than the previous aarch64-on-x86_64 path
  -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1354`.

- Documented design principles for pure-Python :class:`~multidict.istr` implementation
  -- by :user:`asvetlov`

  *Related issues and pull requests on GitHub:*
  :issue:`1360`.

- Pinned ``coverage`` to the ``ctrace`` measurement core so the test suite runs
  on Python 3.14. Coverage picks ``sysmon`` there, which cannot record the
  dynamic contexts ``pytest-cov`` switches at runtime, and the resulting warning
  became an error under the suite's ``filterwarnings`` setting
  -- by :user:`rodrigobnogueira`.

  *Related issues and pull requests on GitHub:*
  :issue:`1393`.

- Dependabot has been restricted to the ``requirements`` subdirectory to
  avoid unintended updates outside dependency requirement files
  -- by :user:`aiolibsbot`.

  *Related issues and pull requests on GitHub:*
  :issue:`1400`.

- Enabled automatic upgrades from ruff after each python version that is dropped
  -- by :user:`Vizonex`.

  *Related issues and pull requests on GitHub:*
  :issue:`1325`.


Miscellaneous internal changes
------------------------------

- Explicitly marked ``empty_htkeys`` as ``const``.

  Moved the structure to ``.rodata`` linker section.

  Unexpected modification of the structure will lead to segfault instead
  of corrupting the data and fail fast.

  ``.rodata`` is mount as read-only-mapped, it is shared well across
  multiple threads without cache misses.

  The change is pretty trivial, it is made for the sake of correctness,
  not for speedup.

  -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1405`.

- Slightly reorganized multidict creating process.

  1. Removed unnecessary calculations when ``md_init()`` creates an empty multidict.
  2. Now ``tp_alloc`` slot is used in object cloning instead of bare :c:func:`PyType_GenericNew` call.

  -- by :user:`asvetlov`

  *Related issues and pull requests on GitHub:*
  :issue:`1420`.

- Dropped dead code, ``_md_del_at()`` and ``_md_del_at_for_upd()`` never fails
  -- by :user:`asvetlov`

  *Related issues and pull requests on GitHub:*
  :issue:`1423`.

- Fixed a missing space in the :exc:`ValueError` message
  raised when constructing a multidict from a sequence
  -- by :user:`veeceey`.

  *Related issues and pull requests on GitHub:*
  :issue:`1295`.

- Renamed the benchmark script from ``benchmarks/becnhmark.py`` to
  ``benchmarks/benchmark.py`` so it matches the invocation documented in
  :doc:`benchmark` -- by :user:`aiolibsbot`.

  *Related issues and pull requests on GitHub:*
  :issue:`1335`.


----


6.7.1
=====

*(2026-01-25)*


Bug fixes
---------

- Fixed slow memory leak caused by identity by adding ``Py_DECREF`` to identity value before leaving ``md_pop_one`` on success
  -- by :user:`Vizonex`.

  *Related issues and pull requests on GitHub:*
  :issue:`1284`.


----


6.7.0
=====

*(2025-10-05)*


Contributor-facing changes
--------------------------

- Updated tests and added CI for CPython 3.14 -- by :user:`kumaraditya303`.

  *Related issues and pull requests on GitHub:*
  :issue:`1235`.


----


6.6.4
=====

*(2025-08-11)*


Bug fixes
---------

- Fixed ``MutliDict`` & ``CIMultiDict`` memory leak when deleting values or clearing them 
  -- by :user:`Vizonex`

  *Related issues and pull requests on GitHub:*
  :issue:`1233`.


Contributor-facing changes
--------------------------

- The type preciseness coverage report generated by `MyPy
  <https://mypy-lang.org>`__ is now uploaded to `Coveralls
  <https://coveralls.io/github/aio-libs/multidict>`__ and
  will not be included in the `Codecov views
  <https://app.codecov.io/gh/aio-libs/multidict>`__ going forward
  -- by :user:`webknjaz`.

  *Related issues and pull requests on GitHub:*
  :issue:`1122`, :issue:`1231`.

- Added memory leak test for popping or deleting attributes from a multidict to prevent future issues or bogus claims.
  -- by :user:`Vizonex`

  *Related issues and pull requests on GitHub:*
  :issue:`1233`.


----


6.6.3
=====

*(2025-06-30)*


Bug fixes
---------

- Fixed inconsistencies generated by the C implementation of ``_md_shrink()`` which might later lead to assertion failures and crash -- by :user:`Romain-Geissler-1A`.

  *Related issues and pull requests on GitHub:*
  :issue:`1229`.


----


6.6.2
=====

*(2025-06-28)*


Bug fixes
---------

- Fixed a memory corruption issue in the C implementation of ``_md_shrink()`` that could lead to segmentation faults and data loss when items were deleted from a :class:`~multidict.MultiDict`. The issue was an edge case in the pointer arithmetic during the compaction phase -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1221`, :issue:`1222`.

- Fixed format string compilation errors in debug builds on 32-bit platforms by using portable ``%zd`` format specifiers for ``Py_ssize_t`` values instead of ``%ld`` -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1225`, :issue:`1226`.


Packaging updates and notes for downstreams
-------------------------------------------

- Re-enabled 32-bit Linux wheel builds that were disabled by default in cibuildwheel 3.0.0 -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1225`, :issue:`1227`.


----


6.6.1
=====

*(2025-06-28)*


Bug fixes
---------

- If :meth:`multidict.MultiDict.extend`, :meth:`multidict.MultiDict.merge`, or :meth:`multidict.MultiDict.update` raises an exception, now the multidict internal state is correctly restored.
  Patch by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1215`.


Contributor-facing changes
--------------------------

- Fixed ``setuptools`` deprecation warning about the license specification -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1216`.

- Fix compiler warnings and convert them to errors -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1217`.


----


6.6.0
=====

*(2025-06-27)*


Features
--------

- Added :meth:`multidict.MultiDict.merge` which copies all items from arguments if its key
  not exist in the dictionary -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`292`.

- Stopped reallocating memory for the internal ``htkeys_t`` structure when inserting new items if the
  multidict has deleted items and it could be collapsed in-place.  Removal of
  ``malloc()``/``free()`` improves the performance slightly.

  The change affects C implementation only, pure Python code is not changed.

  Patch by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1200`.

- C implementation of :class:`multidict.MultiDict.getall` now is slightly faster if it returns nothing -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1212`.


Improved documentation
----------------------

- Replaced docstring for :meth:`multidict.MultiDict.update` to don't use RST/markdown markup.

  *Related issues and pull requests on GitHub:*
  :issue:`1203`.

- Improved documentation for :meth:`multidict.MultiDict.extend` and :meth:`multidict.MultiDict.update` -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1205`.


Contributor-facing changes
--------------------------

- When building wheels, the source distribution is now passed directly
  to the ``cibuildwheel`` invocation -- by :user:`webknjaz`.

  *Related issues and pull requests on GitHub:*
  :issue:`1199`.

- Set up ``PYTHONHASHSEED`` for benchmarks execution to make measured times stable -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1202`.


----


6.5.1
=====

*(2025-06-24)*


Bug fixes
---------

- Fixed a bug in C implementation when multidict is resized and it has deleted slots.

  The bug was introduced by multidict 6.5.0 release.

  Patch by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1195`.


Contributor-facing changes
--------------------------

- A pair of code formatters for Python and C have been configured in the pre-commit tool.

  *Related issues and pull requests on GitHub:*
  :issue:`1123`.

- Shorted fixture parametrization ids.

  For example, ``test_keys_view_xor[case-insensitive-pure-python-module]`` becomes ``test_keys_view_xor[ci-py]`` -- by :user:`asvetlov`.

  *Related issues and pull requests on GitHub:*
  :issue:`1192`.

- The :file:`reusable-cibuildwheel.yml` workflow has been refactored to
  be more generic and :file:`ci-cd.yml` now holds all the configuration
  toggles -- by :user:`webknjaz`.

  *Related issues and pull requests on GitHub:*
  :issue:`1193`.


----


6.5.0
=====

*(2025-06-17)*

.. note::

  The release was yanked because of :issue:`1195`, multidict 6.5.1 should be used
  instead.


Features
--------

- Replace internal implementation from an array of items to hash table.
  algorithmic complexity for lookups is switched from O(N) to O(1).

  The hash table is very similar to :class:`dict` from CPython but it allows keys duplication.

  The benchmark shows 25-50% boost for single lookups, x2-x3 for bulk updates, and x20 for
  some multidict view operations.  The gain is not for free:
  :class:`~multidict.MultiDict.add` and :class:`~multidict.MultiDict.extend` are 25-50%
  slower now. We consider it as acceptable because the lookup is much more common
  operation that addition for the library domain.

  *Related issues and pull requests on GitHub:*
  :issue:`1128`.


Contributor-facing changes
--------------------------

- Builds have been added for arm64 Windows
  wheels and the ``reusable-build-wheel.yml``
  template has been modified to allow for
  an os value (``windows-11-arm``) which
  does not end with the ``-latest`` postfix.

  *Related issues and pull requests on GitHub:*
  :issue:`1167`.


----


6.4.4
=====

*(2025-05-19)*


Bug fixes
---------

- Fixed a segmentation fault when calling :py:meth:`multidict.MultiDict.setdefault` with a single argument -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1160`.

- Fixed a segmentation fault when attempting to directly instantiate view objects
  (``multidict._ItemsView``, ``multidict._KeysView``, ``multidict._ValuesView``) -- by :user:`bdraco`.

  View objects now raise a proper :exc:`TypeError` with the message "cannot create '...' instances directly"
  when direct instantiation is attempted.

  View objects should only be created through the proper methods: :py:meth:`multidict.MultiDict.items`,
  :py:meth:`multidict.MultiDict.keys`, and :py:meth:`multidict.MultiDict.values`.

  *Related issues and pull requests on GitHub:*
  :issue:`1164`.


Miscellaneous internal changes
------------------------------

- :class:`multidict.MultiDictProxy` was refactored to rely only on
  :class:`multidict.MultiDict` public interface and don't touch any implementation
  details.

  *Related issues and pull requests on GitHub:*
  :issue:`1150`.

- Multidict views were refactored to rely only on
  :class:`multidict.MultiDict` API and don't touch any implementation
  details.

  *Related issues and pull requests on GitHub:*
  :issue:`1152`.

- Dropped internal ``_Impl`` class from pure Python implementation, both pure Python and C
  Extension follows the same design internally now.

  *Related issues and pull requests on GitHub:*
  :issue:`1153`.


----


6.4.3
=====

*(2025-04-10)*


Bug fixes
---------

- Fixed building the library in debug mode.

  *Related issues and pull requests on GitHub:*
  :issue:`1144`.

- Fixed custom ``PyType_GetModuleByDef()`` when non-heap type object was passed.

  *Related issues and pull requests on GitHub:*
  :issue:`1147`.


Packaging updates and notes for downstreams
-------------------------------------------

- Added the ability to build in debug mode by setting :envvar:`MULTIDICT_DEBUG_BUILD` in the environment -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1145`.


----


6.4.2
=====

*(2025-04-09)*


Bug fixes
---------

- Fixed a segmentation fault when creating subclassed :py:class:`~multidict.MultiDict` objects on Python < 3.11 -- by :user:`bdraco`.

  The problem first appeared in 6.4.0

  *Related issues and pull requests on GitHub:*
  :issue:`1141`.


----


6.4.1
=====

*(2025-04-09)*


No significant changes.


----


6.4.0
=====

*(2025-04-09)*


Bug fixes
---------

- Fixed a memory leak creating new :class:`~multidict.istr` objects -- by :user:`bdraco`.

  The leak was introduced in 6.3.0

  *Related issues and pull requests on GitHub:*
  :issue:`1133`.

- Fixed reference counting when calling :py:meth:`multidict.MultiDict.update` -- by :user:`bdraco`.

  The leak was introduced in 4.4.0

  *Related issues and pull requests on GitHub:*
  :issue:`1135`.


Features
--------

- Switched C Extension to use heap types and the module state.

  *Related issues and pull requests on GitHub:*
  :issue:`1125`.

- Started building armv7l wheels -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1127`.


----


6.3.2
=====

*(2025-04-03)*


Bug fixes
---------

- Resolved a memory leak by ensuring proper reference count decrementation -- by :user:`asvetlov` and :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1121`.


----


6.3.1
=====

*(2025-04-01)*


Bug fixes
---------

- Fixed keys not becoming case-insensitive when :class:`multidict.CIMultiDict` is created by passing in a :class:`multidict.MultiDict` -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1112`.

- Fixed the pure Python version mutating the original :class:`multidict.MultiDict` when creating a new :class:`multidict.CIMultiDict` from an existing one when keyword arguments are also passed -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1113`.

- Prevented crashing with a segfault when :func:`repr` is called for recursive multidicts and their proxies and views.

  *Related issues and pull requests on GitHub:*
  :issue:`1115`.


----


6.3.0
=====

*(2025-03-31)*


Bug fixes
---------

- Set operations for ``KeysView`` and ``ItemsView`` of case-insensitive multidicts and their proxies are processed in case-insensitive manner.

  *Related issues and pull requests on GitHub:*
  :issue:`965`.

- Rewrote :class:`multidict.CIMultiDict` and it proxy to always return
  :class:`multidict.istr` keys. ``istr`` is derived from :class:`str`,
  thus the change is backward compatible.

  The performance boost is about 15% for some operations for C Extension,
  pure Python implementation have got a visible (15% - 230%) speedup as well.

  *Related issues and pull requests on GitHub:*
  :issue:`1097`.

- Fixed a crash when extending a multidict from multidict proxy if C Extensions were used.

  *Related issues and pull requests on GitHub:*
  :issue:`1100`.


Features
--------

- Implemented a custom parser for ``METH_FASTCALL | METH_KEYWORDS`` protocol
  -- by :user:`asvetlov`.

  The patch re-enables fast call protocol in the :py:mod:`multidict` C Extension.

  Speedup is about 25%-30% for the library benchmarks for Python 3.12+.

  *Related issues and pull requests on GitHub:*
  :issue:`1070`.

- The C-extension no longer pre-allocates a Python exception object in
  lookup-related methods of :py:class:`~multidict.MultiDict` when the
  passed-in *key* is not found but *default* value is provided.

  Namely, this affects :py:meth:`MultiDict.getone()
  <multidict.MultiDict.getone>`, :py:meth:`MultiDict.getall()
  <multidict.MultiDict.getall>`, :py:meth:`MultiDict.get()
  <multidict.MultiDict.get>`, :py:meth:`MultiDict.pop()
  <multidict.MultiDict.pop>`, :py:meth:`MultiDict.popone()
  <multidict.MultiDict.popone>`, and :py:meth:`MultiDict.popall()
  <multidict.MultiDict.popall>`.

  Additionally, the :py:class:`~multidict.MultiDict` comparison with
  regular :py:class:`dict`\ ionaries is now about 60% faster
  on Python 3.13+ in the fallback-to-default case.

  *Related issues and pull requests on GitHub:*
  :issue:`1078`.

- Implemented ``__repr__()`` for C Extension classes in C.

  The speedup is about 2.5 times.

  *Related issues and pull requests on GitHub:*
  :issue:`1081`.

- Made C version of :class:`multidict.istr` pickleable.

  *Related issues and pull requests on GitHub:*
  :issue:`1098`.

- Optimized multidict creation and extending / updating if C Extensions are used.

  The speedup is between 25% and 70% depending on the usage scenario.

  *Related issues and pull requests on GitHub:*
  :issue:`1101`.

- :meth:`multidict.MultiDict.popitem` is changed to remove
  the latest entry instead of the first.

  It gives O(1) amortized complexity.

  The standard :meth:`dict.popitem` removes the last entry also.

  *Related issues and pull requests on GitHub:*
  :issue:`1105`.


Contributor-facing changes
--------------------------

- Started running benchmarks for the pure Python implementation in addition to the C implementation -- by :user:`bdraco`.

  *Related issues and pull requests on GitHub:*
  :issue:`1092`.

- The the project-wide Codecov_ metric is no longer reported
  via GitHub Checks API. The combined value is not very useful
  because one of the sources (MyPy) cannot reach 100% with the
  current state of the ecosystem. We may want to reconsider in
  the future. Instead, we now have two separate
  “runtime coverage” metrics for library code and tests.
  They are to be kept at 100% at all times.
  And the “type coverage” metric will remain advisory, at a
  lower threshold.

  The default patch metric check is renamed to “runtime”
  to better reflect its semantics. This one will also require
  100% coverage.
  Another “typing” patch coverage metric is now reported
  alongside it. It's considered advisory, just like its
  project counterpart.

  When looking at Codecov_, one will likely want to look at
  MyPy and pytest flags separately. It is usually best to
  avoid looking at the PR pages that sometimes display
  combined coverage incorrectly.

  The change additionally disables the deprecated GitHub
  Annotations integration in Codecov_.

  Finally, the badge coloring range now starts at 100%.


  .. image:: https://codecov.io/gh/aio-libs/multidict/branch/master/graph/badge.svg?flag=pytest
     :target: https://codecov.io/gh/aio-libs/multidict?flags[]=pytest
     :alt: Coverage metrics


  -- by :user:`webknjaz`

  *Related issues and pull requests on GitHub:*
  :issue:`1093`.


Miscellaneous internal changes
------------------------------

- Synchronized :file:`pythoncapi_compat.h` with the latest available version.

  *Related issues and pull requests on GitHub:*
  :issue:`1063`.

- Moved registering ABCs for C Extension classes from C to Python.

  *Related issues and pull requests on GitHub:*
  :issue:`1083`.

- Refactored the internal ``pair_list`` implementation.

  *Related issues and pull requests on GitHub:*
  :issue:`1084`.

- Implemented views comparison and disjoints in C instead of Python helpers.

  The performance boost is about 40%.

  *Related issues and pull requests on GitHub:*
  :issue:`1096`.


----


6.2.0
======

*(2025-03-17)*


Bug fixes
---------

- Fixed ``in`` checks throwing an exception instead of returning :data:`False` when testing non-strings.

  *Related issues and pull requests on GitHub:*
  :issue:`1045`.

- Fixed a leak when the last accessed module in ``PyInit__multidict()`` init is not released.

  *Related issues and pull requests on GitHub:*
  :issue:`1061`.


Features
--------

- Implemented support for the free-threaded build of CPython 3.13 -- by :user:`lysnikolaou`.

  *Related issues and pull requests on GitHub:*
  :issue:`1015`.


Packaging updates and notes for downstreams
-------------------------------------------

- Started publishing wheels made for the free-threaded build of CPython 3.13 -- by :user:`lysnikolaou`.

  *Related issues and pull requests on GitHub:*
  :issue:`1015`.


Miscellaneous internal changes
------------------------------

- Used stricter typing across the code base, resulting in improved typing accuracy across multidict classes.
  Funded by an ``NLnet`` grant.

  *Related issues and pull requests on GitHub:*
  :issue:`1046`.


----


6.1.0 (2024-09-09)
==================

Bug fixes
---------

- Covered the unreachable code path in
  ``multidict._multidict_base._abc_itemsview_register()``
  with typing -- by :user:`skinnyBat`.


  *Related issues and pull requests on GitHub:*
  :issue:`928`.




Features
--------

- Added support for Python 3.13 -- by :user:`bdraco`.


  *Related issues and pull requests on GitHub:*
  :issue:`1002`.




Removals and backward incompatible breaking changes
---------------------------------------------------

- Removed Python 3.7 support -- by :user:`bdraco`.


  *Related issues and pull requests on GitHub:*
  :issue:`997`.




Contributor-facing changes
--------------------------

- Added tests to have full code coverage of the
  ``multidict._multidict_base._viewbaseset_richcmp()`` function
  -- by :user:`skinnyBat`.


  *Related issues and pull requests on GitHub:*
  :issue:`928`.



- `The deprecated <https://hynek.me/til/set-output-deprecation-github-actions/>`_
  ``::set-output`` workflow command has been replaced
  by the ``$GITHUB_OUTPUT`` environment variable
  in the GitHub Actions CI/CD workflow definition.


  *Related issues and pull requests on GitHub:*
  :issue:`940`.



- `codecov-action <https://github.com/codecov/codecov-action>`_
  has been temporarily downgraded to ``v3``
  in the GitHub Actions CI/CD workflow definitions
  in order to fix uploading coverage to Codecov_.
  See `this issue <https://github.com/codecov/codecov-action/issues/1252>`_
  for more details.


  .. _Codecov: https://codecov.io/gh/aio-libs/multidict?flags[]=pytest


  *Related issues and pull requests on GitHub:*
  :issue:`941`.



- In the GitHub Actions CI/CD workflow definition,
  the ``Get pip cache dir`` step has been fixed for
  Windows runners by adding ``shell: bash``.
  See `actions/runner#2224 <https://github.com/actions/runner/issues/2224>`_
  for more details.


  *Related issues and pull requests on GitHub:*
  :issue:`942`.



- Interpolation of the ``pip`` cache keys has been
  fixed by adding missing ``$`` syntax
  in the GitHub Actions CI/CD workflow definition.


  *Related issues and pull requests on GitHub:*
  :issue:`943`.




----


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
