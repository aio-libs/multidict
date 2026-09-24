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

   multidict   6.9.2.dev0 (a556b22)
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
     - 91,347
     - 44,479
     - 62,555
     - 0.68x
   * - ``cls()``
     - 569
     - 784
     - 920
     - 1.62x
   * - ``cls(items)``, 20 items
     - 9,257
     - 4,340
     - 6,156
     - 0.66x
   * - ``d.copy()``
     - 9,761
     - 25,650
     - 26,043
     - 2.67x
   * - ``d.copy()``, 20 items
     - 1,452
     - 2,080
     - 2,473
     - 1.70x
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
     - 324
     - 412
     - 1.63x
   * - ``d[key] = v``, new key
     - 389
     - 399
     - 486
     - 1.25x
   * - ``d.setdefault(key, v)``, new key
     - 585
     - 524
     - 802
     - 1.37x
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
     - 2,384
     - 5.33x
   * - ``d.update(other)``, 100 existing keys
     - 30,692
     - 37,521
     - 46,385
     - 1.51x
   * - ``d.clear()``
     - 4,818
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
     - 66,204
     - 83,433
     - 0.73x
   * - ``cls()``
     - 604
     - 820
     - 909
     - 1.51x
   * - ``cls(items)``, 20 items
     - 11,674
     - 6,787
     - 8,476
     - 0.73x
   * - ``d.copy()``
     - 12,756
     - 29,390
     - 29,730
     - 2.33x
   * - ``d.copy()``, 20 items
     - 1,830
     - 2,688
     - 3,028
     - 1.65x
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
     - 613
     - 699
     - 1.33x
   * - ``d.setdefault(key, v)``, new key
     - 730
     - 713
     - 976
     - 1.34x
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
     - 2,579
     - 5.34x
   * - ``d.update(other)``, 100 existing keys
     - 38,287
     - 48,787
     - 57,839
     - 1.51x
   * - ``d.clear()``
     - 6,732
     - 9,350
     - 9,602
     - 1.43x
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
     - 1.43x
     - 1.05x
     - 1.96x
     - ``d[key]``
   * - ``CIMultiDict``
     - 1.30x
     - 0.99x
     - 1.59x
     - ``d[key]``
   * - ``MultiDict`` (Python)
     - 1.21x
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
     - 2,489,377
     - 2,729,994
     - 55.97x
   * - ``cls()``
     - 64,930
     - 64,942
     - 82.82x
   * - ``cls(items)``, 20 items
     - 330,688
     - 353,984
     - 76.20x
   * - ``d.copy()``
     - 450,388
     - 456,931
     - 17.56x
   * - ``d.copy()``, 20 items
     - 75,532
     - 82,026
     - 36.31x
   * - ``d[key]``
     - 7,478
     - 8,732
     - 50.42x
   * - ``d.get(key)``, miss
     - 7,733
     - 8,977
     - 24.66x
   * - ``key in d``
     - 7,756
     - 9,011
     - 57.84x
   * - ``d[key] = v``, existing key
     - 26,204
     - 27,513
     - 80.75x
   * - ``d[key] = v``, new key
     - 27,104
     - 28,130
     - 67.96x
   * - ``d.setdefault(key, v)``, new key
     - 34,798
     - 36,982
     - 66.43x
   * - ``del d[key]``
     - 19,689
     - 20,897
     - 83.25x
   * - ``d.pop(key)``
     - 15,500
     - 16,733
     - 39.97x
   * - ``d.popitem()``
     - 11,915
     - 13,851
     - 10.91x
   * - ``d.update(other)``, 100 existing keys
     - 2,050,857
     - 2,167,616
     - 54.66x
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
     - 9,152
     - 9,154
     - 18.45x

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
two marker comments in this file, and commit the result together with the
change that moved the numbers.
