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
from the tables. They are still available to both runners; pass
``--include-multidict-only`` to the Callgrind driver, or use
``benchmarks/benchmark.py`` directly.

Results
-------

.. BEGIN GENERATED TABLES

.. code-block:: text

   multidict   6.9.2.dev0 (6e6ac72)
   CPython     3.14.7, GIL and free-threaded builds
   valgrind    3.26.0, callgrind, client-request bracketing
   CPU         Intel(R) Core(TM) Ultra 7 155H
   platform    Linux-6.17.0-29-generic-x86_64-with-glibc2.43
   collected   2026-09-24

.. list-table:: Instructions per operation, ``CPython 3.14.7``, GIL build
   :header-rows: 1
   :widths: 32 14 16 18 20

   * - Operation
     - ``dict``
     - ``MultiDict``
     - ``CIMultiDict``
     - ``CIMultiDict`` vs ``dict``
   * - ``cls(items)``
     - 91,427
     - 44,450
     - 62,530
     - 0.68x
   * - ``cls()``
     - 569
     - 609
     - 749
     - 1.32x
   * - ``cls(items)``, 20 items
     - 9,257
     - 4,163
     - 5,983
     - 0.65x
   * - ``d.copy()``
     - 9,810
     - 25,462
     - 25,859
     - 2.64x
   * - ``d.copy()``, 20 items
     - 1,452
     - 1,903
     - 2,300
     - 1.58x
   * - ``d[key]``
     - 184
     - 148
     - 238
     - 1.29x
   * - ``d.get(key)``, miss
     - 358
     - 314
     - 582
     - 1.62x
   * - ``key in d``
     - 174
     - 134
     - 225
     - 1.29x
   * - ``d[key] = v``, existing key
     - 252
     - 314
     - 404
     - 1.60x
   * - ``d[key] = v``, new key
     - 388
     - 388
     - 478
     - 1.23x
   * - ``d.setdefault(key, v)``, new key
     - 585
     - 505
     - 784
     - 1.34x
   * - ``del d[key]``
     - 295
     - 236
     - 326
     - 1.10x
   * - ``d.pop(key)``
     - 476
     - 388
     - 672
     - 1.41x
   * - ``d.popitem()``
     - 447
     - 1,092
     - 2,395
     - 5.36x
   * - ``d.update(other)``, 100 existing keys
     - 30,716
     - 37,529
     - 46,393
     - 1.51x
   * - ``d.clear()``
     - 4,821
     - 7,212
     - 7,469
     - 1.55x
   * - ``for k in d``
     - 53
     - 62
     - 65
     - 1.22x
   * - ``for k, v in d.items()``
     - 88
     - 378
     - 382
     - 4.35x
   * - ``d.items()``
     - 538
     - 326
     - 583
     - 1.08x
   * - ``iter(d)``
     - 727
     - 496
     - 496
     - 0.68x

.. list-table:: Instructions per operation, ``CPython 3.14.7``, free-threaded build
   :header-rows: 1
   :widths: 32 14 16 18 20

   * - Operation
     - ``dict``
     - ``MultiDict``
     - ``CIMultiDict``
     - ``CIMultiDict`` vs ``dict``
   * - ``cls(items)``
     - 114,378
     - 66,632
     - 83,861
     - 0.73x
   * - ``cls()``
     - 604
     - 852
     - 941
     - 1.56x
   * - ``cls(items)``, 20 items
     - 11,674
     - 6,855
     - 8,544
     - 0.73x
   * - ``d.copy()``
     - 12,756
     - 29,819
     - 30,160
     - 2.36x
   * - ``d.copy()``, 20 items
     - 1,830
     - 2,757
     - 3,097
     - 1.69x
   * - ``d[key]``
     - 224
     - 291
     - 378
     - 1.69x
   * - ``d.get(key)``, miss
     - 392
     - 409
     - 660
     - 1.69x
   * - ``key in d``
     - 235
     - 253
     - 340
     - 1.45x
   * - ``d[key] = v``, existing key
     - 344
     - 515
     - 602
     - 1.75x
   * - ``d[key] = v``, new key
     - 525
     - 612
     - 699
     - 1.33x
   * - ``d.setdefault(key, v)``, new key
     - 730
     - 707
     - 970
     - 1.33x
   * - ``del d[key]``
     - 404
     - 396
     - 483
     - 1.19x
   * - ``d.pop(key)``
     - 587
     - 512
     - 775
     - 1.32x
   * - ``d.popitem()``
     - 482
     - 1,245
     - 2,577
     - 5.34x
   * - ``d.update(other)``, 100 existing keys
     - 38,319
     - 48,951
     - 57,823
     - 1.51x
   * - ``d.clear()``
     - 6,732
     - 9,749
     - 10,001
     - 1.49x
   * - ``for k in d``
     - 55
     - 100
     - 102
     - 1.88x
   * - ``for k, v in d.items()``
     - 114
     - 450
     - 453
     - 3.99x
   * - ``d.items()``
     - 446
     - 518
     - 769
     - 1.73x
   * - ``iter(d)``
     - 647
     - 782
     - 782
     - 1.21x

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
     - 0.83x
     - 1.40x
     - ``d.clear()``
   * - ``MultiDict``
     - 1.47x
     - 1.14x
     - 1.96x
     - ``d[key]``
   * - ``CIMultiDict``
     - 1.34x
     - 1.08x
     - 1.59x
     - ``d[key]``
   * - ``MultiDict`` (Python)
     - 1.22x
     - 0.94x
     - 2.96x
     - ``for k in d``
   * - ``CIMultiDict`` (Python)
     - 1.21x
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
     - 2,488,662
     - 2,729,937
     - 55.99x
   * - ``cls()``
     - 64,605
     - 64,946
     - 106.08x
   * - ``cls(items)``, 20 items
     - 330,310
     - 353,961
     - 79.34x
   * - ``d.copy()``
     - 450,224
     - 456,750
     - 17.68x
   * - ``d.copy()``, 20 items
     - 75,510
     - 82,026
     - 39.68x
   * - ``d[key]``
     - 7,483
     - 8,737
     - 50.46x
   * - ``d.get(key)``, miss
     - 7,741
     - 8,977
     - 24.68x
   * - ``key in d``
     - 7,762
     - 9,016
     - 57.89x
   * - ``d[key] = v``, existing key
     - 26,228
     - 27,539
     - 83.40x
   * - ``d[key] = v``, new key
     - 27,120
     - 28,153
     - 69.88x
   * - ``d.setdefault(key, v)``, new key
     - 34,833
     - 37,029
     - 69.03x
   * - ``del d[key]``
     - 19,684
     - 20,897
     - 83.23x
   * - ``d.pop(key)``
     - 15,495
     - 16,732
     - 39.96x
   * - ``d.popitem()``
     - 11,915
     - 13,851
     - 10.91x
   * - ``d.update(other)``, 100 existing keys
     - 2,050,778
     - 2,167,967
     - 54.65x
   * - ``d.clear()``
     - 114,191
     - 114,183
     - 15.83x
   * - ``for k in d``
     - 2,190
     - 4,066
     - 35.27x
   * - ``for k, v in d.items()``
     - 2,510
     - 4,445
     - 6.64x
   * - ``d.items()``
     - 2,494
     - 2,494
     - 7.65x
   * - ``iter(d)``
     - 9,143
     - 9,152
     - 18.43x

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

Three things to watch:

* **Do not share a virtualenv between checkouts.** Two working trees installing
  into the same environment swap which extension module resolves, and the
  benchmark then measures the other checkout.

* **Do not hand Valgrind a wrapper script.** Valgrind does not follow child
  processes by default, so it measures the wrapper and detaches at its
  ``exec``. The failure signature is a plausible constant instruction count
  that does not change when the round count does. A virtualenv's
  ``bin/python`` is a symbolic link to a real binary and is fine; a ``pyenv``
  shim is not. The driver refuses a wrapper rather than reporting a number.

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

Because instruction counts do not depend on scheduling, the children run in
parallel by default and the result is correct even on a busy machine. Use
``-j1`` to run them one at a time.

The driver checks itself before reporting: it refuses a wrapper script, it
requires the counts to grow with the round count, and it re-measures one cell
and requires a bit-identical result. ``--self-check`` runs every operation once
without Valgrind and asserts the resulting state, which is the quick way to
check a new operation.

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
