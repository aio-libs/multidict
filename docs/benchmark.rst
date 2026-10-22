.. _benchmarking-reference:

==========
Benchmarks
==========

Introduction
------------

``multidict`` is measured in two different ways, and the difference matters
when you read the numbers below or produce your own.

Callgrind counts the instructions an operation executes. The count does not
depend on scheduling, CPU frequency, or how busy the machine is, so a
regression shows up as a changed integer instead of a distribution you have to
argue about. Every number published on this page is an instruction count.

:doc:`pyperf:index` measures elapsed time. That is what users actually feel,
but it needs a tuned machine to be reproducible.

.. list-table:: Choosing a measurement mode
   :header-rows: 1
   :widths: 24 38 38

   * - Property
     - Callgrind instruction counts
     - ``pyperf`` wall clock
   * - Reproducible
     - Yes, bit for bit
     - Only on a tuned machine
   * - Needs a quiet machine
     - No
     - Yes
   * - Measures
     - Instructions executed
     - Elapsed time
   * - Blind to
     - Cache misses, branch prediction, memory latency, lock contention
     - Nothing, but it is noisy
   * - Use it for
     - The tables below, and for spotting a regression
     - Confirming that an instruction-count win is a real time win

Instruction counts are a proxy. A change that removes instructions but worsens
locality still looks like a win, so confirm anything significant with
``pyperf`` on a tuned machine before claiming it.

What is measured
----------------

The rows are the operations :class:`dict` and :class:`~multidict.MultiDict`
both have. Each is measured on a mapping of 200 :class:`str` keys whose values
equal their keys, except ``d.update(other)``, which merges a 100-item
:class:`dict` whose keys are all already present, so it measures replacement
rather than insertion.

Three rows use a different size, because what they cost is dominated by an
allocation rather than by the entries: ``cls()`` builds an empty mapping, and
``cls(items)``, 20 items and ``d.copy()``, 20 items use a request's worth of
headers. An allocation is a fixed cost, so at 200 entries it is divided across
them and all but disappears; ``d.items()`` and ``iter(d)`` are listed for the
same reason, since they allocate one object each however large the mapping is.

Operations that destroy the mapping, such as ``d.pop(key)`` and ``d.clear()``,
rebuild it before each measured round. The rebuild happens outside the
instrumented region, so it is not counted.

Every operation is also measured a second time with the operation itself
removed from the loop, and that baseline is subtracted. Without the
subtraction, the driving :keyword:`for` loop, worth a few hundred instructions
per iteration, would compress every ratio toward 1. The subtraction does not
remove the attribute lookup and call of method-based operations such as
``d.get(key)``, because a caller pays those too.

Operations :class:`dict` has no counterpart for, including ``add()``,
``getall()`` and everything keyed by :class:`~multidict.istr`, are excluded
from the tables. So is re-initializing a populated mapping with
``d.__init__(...)``, which replaces a multidict's contents but only updates a
:class:`dict`. They are still available to both runners; pass
``--include-multidict-only`` to the Callgrind driver, or use
``benchmarks/benchmark.py`` directly.

Results
-------

.. BEGIN GENERATED TABLES

.. code-block:: text

   multidict   7.1.1.dev0 (edfc84d0)
   CPython     3.14.7, GIL and free-threaded builds
   valgrind    3.26.0, callgrind, client-request bracketing
   CPU         Intel(R) Core(TM) Ultra 7 155H
   platform    Linux-6.17.0-29-generic-x86_64-with-glibc2.43
   collected   2026-10-22

.. list-table:: Instructions per operation, ``CPython 3.14.7``, GIL build
   :header-rows: 1
   :widths: 32 14 16 18 20

   * - Operation
     - ``dict``
     - ``MultiDict``
     - ``CIMultiDict``
     - ``CIMultiDict`` vs ``dict``
   * - ``cls(items)``
     - 91,506
     - 42,668
     - 60,025
     - 0.66x
   * - ``cls()``
     - 676
     - 612
     - 753
     - 1.11x
   * - ``cls(items)``, 20 items
     - 9,347
     - 4,679
     - 5,915
     - 0.63x
   * - ``d.copy()``
     - 9,796
     - 9,923
     - 24,777
     - 2.53x
   * - ``d.copy()``, 20 items
     - 1,464
     - 1,446
     - 1,963
     - 1.34x
   * - ``d[key]``
     - 184
     - 154
     - 196
     - 1.06x
   * - ``d.get(key)``, miss
     - 358
     - 316
     - 346
     - 0.97x
   * - ``key in d``
     - 174
     - 128
     - 173
     - 0.99x
   * - ``d[key] = v``, existing key
     - 252
     - 261
     - 325
     - 1.29x
   * - ``d[key] = v``, new key
     - 389
     - 403
     - 483
     - 1.24x
   * - ``d.setdefault(key, v)``, new key
     - 585
     - 540
     - 623
     - 1.07x
   * - ``del d[key]``
     - 295
     - 248
     - 304
     - 1.03x
   * - ``d.pop(key)``
     - 476
     - 390
     - 450
     - 0.95x
   * - ``d.popitem()``
     - 447
     - 473
     - 1,084
     - 2.42x
   * - ``d.update(other)``, 100 existing keys
     - 30,732
     - 34,352
     - 39,654
     - 1.29x
   * - ``d.clear()``
     - 4,816
     - 4,636
     - 6,436
     - 1.34x
   * - ``for k in d``
     - 53
     - 49
     - 52
     - 0.98x
   * - ``for k, v in d.items()``
     - 88
     - 123
     - 126
     - 1.43x
   * - ``d.items()``
     - 538
     - 321
     - 321
     - 0.60x
   * - ``iter(d)``
     - 727
     - 500
     - 500
     - 0.69x

.. list-table:: Instructions per operation, ``CPython 3.14.7``, free-threaded build
   :header-rows: 1
   :widths: 32 14 16 18 20

   * - Operation
     - ``dict``
     - ``MultiDict``
     - ``CIMultiDict``
     - ``CIMultiDict`` vs ``dict``
   * - ``cls(items)``
     - 114,418
     - 64,472
     - 79,801
     - 0.70x
   * - ``cls()``
     - 669
     - 904
     - 994
     - 1.49x
   * - ``cls(items)``, 20 items
     - 11,735
     - 7,266
     - 8,265
     - 0.70x
   * - ``d.copy()``
     - 12,757
     - 12,168
     - 28,172
     - 2.21x
   * - ``d.copy()``, 20 items
     - 1,827
     - 2,139
     - 2,744
     - 1.50x
   * - ``d[key]``
     - 224
     - 227
     - 266
     - 1.19x
   * - ``d.get(key)``, miss
     - 392
     - 392
     - 395
     - 1.01x
   * - ``key in d``
     - 235
     - 182
     - 217
     - 0.92x
   * - ``d[key] = v``, existing key
     - 344
     - 408
     - 472
     - 1.37x
   * - ``d[key] = v``, new key
     - 524
     - 572
     - 643
     - 1.23x
   * - ``d.setdefault(key, v)``, new key
     - 730
     - 720
     - 781
     - 1.07x
   * - ``del d[key]``
     - 404
     - 348
     - 419
     - 1.04x
   * - ``d.pop(key)``
     - 587
     - 526
     - 593
     - 1.01x
   * - ``d.popitem()``
     - 482
     - 524
     - 1,158
     - 2.40x
   * - ``d.update(other)``, 100 existing keys
     - 38,319
     - 40,604
     - 43,706
     - 1.14x
   * - ``d.clear()``
     - 6,732
     - 5,776
     - 8,176
     - 1.21x
   * - ``for k in d``
     - 55
     - 85
     - 88
     - 1.61x
   * - ``for k, v in d.items()``
     - 114
     - 173
     - 176
     - 1.55x
   * - ``d.items()``
     - 443
     - 514
     - 514
     - 1.16x
   * - ``iter(d)``
     - 647
     - 806
     - 806
     - 1.25x

.. list-table:: Free-threading overhead, ``3.14.7`` free-threaded versus GIL build
   :header-rows: 1
   :widths: 26 15 12 13 24

   * - Class
     - Median
     - Best
     - Worst
     - Worst operation
   * - ``dict``
     - 1.25x
     - 0.82x
     - 1.40x
     - ``d.clear()``
   * - ``MultiDict``
     - 1.42x
     - 1.11x
     - 1.73x
     - ``for k in d``
   * - ``CIMultiDict``
     - 1.33x
     - 1.07x
     - 1.69x
     - ``for k in d``
   * - ``MultiDict`` (Python)
     - 1.24x
     - 0.94x
     - 2.96x
     - ``for k in d``
   * - ``CIMultiDict`` (Python)
     - 1.23x
     - 0.94x
     - 2.09x
     - ``for k in d``

.. list-table:: Pure-Python backend, instructions per operation, ``CPython 3.14.7``, GIL build
   :header-rows: 1
   :widths: 34 20 20 26

   * - Operation
     - ``MultiDict``
     - ``CIMultiDict``
     - ``MultiDict`` vs the C extension
   * - ``cls(items)``
     - 2,521,674
     - 2,786,802
     - 59.10x
   * - ``cls()``
     - 63,748
     - 63,777
     - 104.16x
   * - ``cls(items)``, 20 items
     - 356,792
     - 382,534
     - 76.25x
   * - ``d.copy()``
     - 451,394
     - 456,492
     - 45.49x
   * - ``d.copy()``, 20 items
     - 76,010
     - 80,614
     - 52.57x
   * - ``d[key]``
     - 7,427
     - 8,807
     - 48.29x
   * - ``d.get(key)``, miss
     - 7,665
     - 9,078
     - 24.28x
   * - ``key in d``
     - 7,706
     - 9,086
     - 60.06x
   * - ``d[key] = v``, existing key
     - 22,718
     - 24,150
     - 87.17x
   * - ``d[key] = v``, new key
     - 26,715
     - 27,888
     - 66.34x
   * - ``d.setdefault(key, v)``, new key
     - 26,753
     - 27,925
     - 49.50x
   * - ``del d[key]``
     - 15,778
     - 17,142
     - 63.75x
   * - ``d.pop(key)``
     - 14,526
     - 15,885
     - 37.25x
   * - ``d.popitem()``
     - 10,950
     - 12,896
     - 23.17x
   * - ``d.update(other)``, 100 existing keys
     - 1,682,779
     - 1,815,802
     - 48.99x
   * - ``d.clear()``
     - 114,751
     - 114,720
     - 24.75x
   * - ``for k in d``
     - 2,188
     - 4,074
     - 44.56x
   * - ``for k, v in d.items()``
     - 2,510
     - 4,455
     - 20.46x
   * - ``d.items()``
     - 2,493
     - 2,493
     - 7.77x
   * - ``iter(d)``
     - 8,348
     - 8,348
     - 16.70x

.. END GENERATED TABLES

How to read these numbers
-------------------------

They are instructions, not time. Two operations with the same count can differ
in wall clock if one of them misses cache more often.

They transfer between x86-64 machines running the same builds, because the
count depends on the compiled code and not on the hardware executing it. They
do not transfer between CPython versions: an interpreter change moves every row
at once.

The free-threading table includes :class:`dict` as a control. Part of the
overhead on the free-threaded build is CPython's own, not ``multidict``'s, and
the :class:`dict` row is how you tell the two apart.

Running the benchmarks
----------------------

Use a virtualenv per interpreter build
``````````````````````````````````````

Getting this wrong produces plausible wrong numbers rather than an error, so do
it first.

Create one virtualenv per build, from the resolved interpreter:

.. code-block:: bash

    $ ~/.pyenv/versions/3.14.7/bin/python  -m venv .venv-gil
    $ ~/.pyenv/versions/3.14.7t/bin/python -m venv .venv-ft
    $ for venv in .venv-gil .venv-ft; do
    >     $venv/bin/python -m pip install -e . -r requirements/dev.txt
    > done

Four things to watch:

* **Do not share a virtualenv between checkouts.** Two working trees installing
  into the same environment swap which extension module resolves, and the
  benchmark then measures the other checkout.

* **Do not hand Valgrind a wrapper script.** Valgrind does not follow child
  processes by default, so it measures the wrapper and detaches at its
  ``exec``. The failure signature is a plausible constant instruction count
  that does not change when the round count does. A virtualenv's
  ``bin/python`` is a symbolic link to a real binary and is fine; a ``pyenv``
  shim is not. The driver refuses a wrapper rather than reporting a number.

* **Install** ``requirements/pytest.txt``, which brings ``pytest-codspeed``.
  The children bracket the measured loop with the Valgrind client requests it
  ships; without them they count the whole process, so the unmeasured setup lands
  in every number, destructive operations read too cheap, and the raw counts
  are dominated by interpreter startup. Those numbers cannot be compared with a
  bracketed run, so the driver refuses to collect them unless it is given
  ``--whole-process``, and ``render_tables.py`` refuses to render them. A fresh
  virtualenv with only ``pip install -e .`` is the usual way to hit this.

* **Use the same patch release for both builds.** Otherwise the free-threading
  table attributes unrelated interpreter changes to free-threading.
  ``render_tables.py`` refuses to build it when the two runs disagree.

Confirm each environment picked up the extension you expect:

.. code-block:: bash

    $ .venv-gil/bin/python -c "import multidict._multidict as m; print(m.__file__)"

Instruction counts with Callgrind
`````````````````````````````````

Valgrind must be installed. Run the driver *with* the interpreter you want to
measure:

.. code-block:: bash

    $ .venv-gil/bin/python benchmarks/callgrind_driver.py -o gil.json
    $ .venv-ft/bin/python  benchmarks/callgrind_driver.py -o ft.json

Each cell is four Valgrind child processes, ``{run, noop}`` at two round
counts, reduced with::

    ir_per_op = ((run[r2] - run[r1]) - (noop[r2] - noop[r1])) / ((r2 - r1) * inner)

Taking the difference between two non-zero round counts cancels interpreter
startup exactly; subtracting the second arm cancels the driving loop. The
children set ``PYTHONHASHSEED=0``, because string hash randomization changes
the hash table's probe sequences from process to process and moves the counts
by about 4%, which looks exactly like a real regression.

The children also run from a staging directory of a fixed length under the
temporary directory rather than from the tree. CPython allocates every path it
knows at startup, including the interpreter, the virtualenv, ``sys.path`` and
the path an editable install maps ``multidict`` to, and it caches the listing
of every directory it imports from. Two copies of one commit whose paths differ
in length, or whose trees hold different build products, therefore hand the
measured loop a different heap, and an operation that allocates, such as an
``istr`` lookup that copies its key, can take the allocator's slow path on every
iteration in one tree and never in the other: identical code measured 3% apart
on ``getitem_istr``, and a few dozen instructions apart on ``copy`` and
``update``. The driver first asks a child which modules it imports, then stages
a link to the base interpreter, a copy of the child's own scripts, one import
directory holding a link to each of those modules (and, for ``multidict``, to
each of its imported files only), and a bytecode cache of its own. The children
run with ``-S`` and a reduced environment, so neither ``pyvenv.cfg``, ``.pth``
files, the tree's ``__pycache__`` nor the caller's working directory reaches
them, and the result records what was staged under ``stage`` in its metadata.

The same layout effect remains for anything the change itself allocates at
import time, so treat a delta of a few instructions per operation on an
allocating row with suspicion, and confirm it against a base-to-base control:
the base commit measured from two checkouts should agree exactly.

The free-threaded build allocates through mimalloc, which draws the address of
its first memory reservation from ``getrandom()`` and returns memory to the
system on a timer. Either moves allocating rows between two runs of one tree,
by about 20% on ``getitem_istr``. The children set
``MIMALLOC_ARENA_RESERVE=2GiB``, a reservation mimalloc leaves the kernel to
place, which Valgrind does at the same address every time, and
``MIMALLOC_PURGE_DELAY=-1``.

Because instruction counts do not depend on scheduling, the children run in
parallel by default and the result is correct even on a busy machine. Use
``-j1`` to run them one at a time.

The driver checks itself before reporting: it refuses a wrapper script, it
requires the counts to grow with the round count, and it re-measures one cell
and requires a bit-identical result. ``--self-check`` runs every operation once
without Valgrind and asserts the resulting state, which is the quick way to
check a new operation. A measuring run does the same first, but only for the
cells ``--impl`` and ``--include-multidict-only`` select, so ``--impl
multidict_py`` works on a pure-Python install.

Wall-clock measurements with pyperf
```````````````````````````````````

``requirements/dev.txt`` should be installed first. Please also make sure that
you have :doc:`configured <pyperf:system>` your OS to have reliable results;
see `Preparing a stable environment`_ below.

.. code-block:: bash

    $ python benchmarks/benchmark.py

This runs both classes (:class:`~multidict.MultiDict` and
:class:`~multidict.CIMultiDict`) of both implementations (pure-Python and C),
plus :class:`dict` for comparison. ``--shared-only`` limits it to the
operations :class:`dict` also has.

To run benchmarks for a specific class of specific implementation please use
the ``--impl`` option:

.. code-block:: bash

    $ python benchmarks/benchmark.py --impl multidict_c

Please use ``--help`` to see all available options. Most of the options are
described at :doc:`pyperf's Runner <pyperf:runner>` documentation.

Reported times are per operation and exclude the rebuild that destructive
operations need, but unlike the tables above they still include the driving
loop, so they are larger than the instruction counts imply and the two are not
directly comparable.

Comparing implementations
`````````````````````````

The ``--impl`` option combined with the :ref:`compare_to <pyperf:compare_to_cmd>`
command of :doc:`pyperf:index` gives a good picture of how an implementation
performs:

.. code-block:: bash

    $ python benchmarks/benchmark.py --impl multidict_c -o multidict_c.json
    $ python benchmarks/benchmark.py --impl multidict_py -o multidict_py.json
    $ python -m pyperf compare_to multidict_c.json multidict_py.json

Preparing a stable environment
------------------------------

None of this is needed for the instruction counts; it applies to ``pyperf``
wall-clock runs only.

Start with the tuning ``pyperf`` can do itself, which covers the CPU frequency
governor, turbo, address space randomization and the ``perf`` event rate:

.. code-block:: bash

    $ sudo python -m pyperf system tune
    $ python -m pyperf system show
    $ sudo python -m pyperf system reset    # afterwards

Beyond that:

* **Isolate cores at boot.** Add ``isolcpus=``, ``nohz_full=`` and
  ``rcu_nocbs=`` to the kernel command line for the cores you will measure on.
  ``nohz_full`` matters because the scheduler tick otherwise lands inside
  individual measurements.

* **Pin the workers** with ``taskset -c`` or ``pyperf``'s ``--affinity``
  option.

* **On a hybrid CPU, pin to performance cores only**, one thread per physical
  core. A run that migrates between a performance core and an efficiency core
  produces a bimodal distribution that ``pyperf`` reports as an enormous
  standard deviation, and hardware counters are multiplexed across the two core
  types. List them with ``lscpu -e=CPU,CORE,MAXMHZ`` or read
  ``/sys/devices/cpu_core/cpus``.

* **Disable simultaneous multithreading**, or at least leave the sibling
  thread idle.

* **Pin the clock.** ``cpupower frequency-set -g performance`` and disabling
  turbo through ``/sys/devices/system/cpu/intel_pstate/no_turbo`` stop the
  frequency drifting as the package heats up over a long run.

* **Set** ``PYTHONHASHSEED=0``. Note that ``-I`` and ``-E`` make CPython ignore
  every ``PYTHON*`` variable, so adding either silently drops it.

On a shared, frequency-scaled laptop none of this is enough: within-run spread
of 10% to 30% is normal there, which makes anything smaller than a 30% change
too small to measure. Use the instruction counts instead.

Regenerating the tables
-----------------------

Collect both builds, then render:

.. code-block:: bash

    $ .venv-gil/bin/python benchmarks/callgrind_driver.py -o gil.json
    $ .venv-ft/bin/python  benchmarks/callgrind_driver.py -o ft.json
    $ python benchmarks/render_tables.py gil.json ft.json

That prints the reStructuredText so the numbers can be read before they are
committed. Add ``--write docs/benchmark.rst`` to replace the block between the
two marker comments in this file.

The tables are regenerated once per release rather than in every pull request
that moves performance, since each run shifts every row a little and the churn
buries the rows that actually changed. A pull request reports the operations it
moved in its own description; the published tables are refreshed as a step of
the release procedure, documented in :file:`RELEASE.md`.
