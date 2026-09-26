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

   multidict   6.9.2.dev0 (d66879b)
   CPython     3.14.7, GIL and free-threaded builds
   valgrind    3.26.0, callgrind, client-request bracketing
   CPU         Intel(R) Core(TM) Ultra 7 155H
   platform    Linux-6.17.0-29-generic-x86_64-with-glibc2.43
   collected   2026-09-26

.. list-table:: Instructions per operation, ``CPython 3.14.7``, GIL build
   :header-rows: 1
   :widths: 32 14 16 18 20

   * - Operation
     - ``dict``
     - ``MultiDict``
     - ``CIMultiDict``
     - ``CIMultiDict`` vs ``dict``
   * - ``cls(items)``
     - 91,283
     - 44,860
     - 55,740
     - 0.61x
   * - ``cls()``
     - 570
     - 615
     - 755
     - 1.33x
   * - ``cls(items)``, 20 items
     - 9,253
     - 4,229
     - 5,329
     - 0.58x
   * - ``d.copy()``
     - 9,815
     - 25,450
     - 25,590
     - 2.61x
   * - ``d.copy()``, 20 items
     - 1,447
     - 1,909
     - 2,049
     - 1.42x
   * - ``d[key]``
     - 184
     - 148
     - 201
     - 1.09x
   * - ``d.get(key)``, miss
     - 358
     - 314
     - 356
     - 0.99x
   * - ``key in d``
     - 174
     - 134
     - 188
     - 1.08x
   * - ``d[key] = v``, existing key
     - 252
     - 330
     - 381
     - 1.51x
   * - ``d[key] = v``, new key
     - 389
     - 403
     - 455
     - 1.17x
   * - ``d.setdefault(key, v)``, new key
     - 585
     - 512
     - 564
     - 0.96x
   * - ``del d[key]``
     - 295
     - 251
     - 304
     - 1.03x
   * - ``d.pop(key)``
     - 476
     - 391
     - 444
     - 0.93x
   * - ``d.popitem()``
     - 447
     - 503
     - 1,510
     - 3.38x
   * - ``d.update(other)``, 100 existing keys
     - 30,716
     - 36,860
     - 42,800
     - 1.39x
   * - ``d.clear()``
     - 4,821
     - 7,222
     - 7,222
     - 1.50x
   * - ``for k in d``
     - 53
     - 60
     - 64
     - 1.20x
   * - ``for k, v in d.items()``
     - 88
     - 130
     - 134
     - 1.52x
   * - ``d.items()``
     - 536
     - 326
     - 326
     - 0.61x
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
     - 114,347
     - 69,893
     - 80,321
     - 0.70x
   * - ``cls()``
     - 588
     - 878
     - 966
     - 1.64x
   * - ``cls(items)``, 20 items
     - 11,657
     - 7,136
     - 8,143
     - 0.70x
   * - ``d.copy()``
     - 12,756
     - 29,070
     - 29,158
     - 2.29x
   * - ``d.copy()``, 20 items
     - 1,830
     - 2,764
     - 2,852
     - 1.56x
   * - ``d[key]``
     - 224
     - 234
     - 286
     - 1.28x
   * - ``d.get(key)``, miss
     - 392
     - 371
     - 412
     - 1.05x
   * - ``key in d``
     - 235
     - 195
     - 247
     - 1.05x
   * - ``d[key] = v``, existing key
     - 344
     - 505
     - 556
     - 1.62x
   * - ``d[key] = v``, new key
     - 524
     - 610
     - 661
     - 1.26x
   * - ``d.setdefault(key, v)``, new key
     - 730
     - 730
     - 779
     - 1.07x
   * - ``del d[key]``
     - 404
     - 409
     - 457
     - 1.13x
   * - ``d.pop(key)``
     - 587
     - 538
     - 586
     - 1.00x
   * - ``d.popitem()``
     - 482
     - 665
     - 1,714
     - 3.55x
   * - ``d.update(other)``, 100 existing keys
     - 38,339
     - 44,676
     - 50,300
     - 1.31x
   * - ``d.clear()``
     - 6,732
     - 8,978
     - 8,978
     - 1.33x
   * - ``for k in d``
     - 55
     - 102
     - 102
     - 1.88x
   * - ``for k, v in d.items()``
     - 114
     - 189
     - 190
     - 1.67x
   * - ``d.items()``
     - 443
     - 513
     - 513
     - 1.16x
   * - ``iter(d)``
     - 644
     - 784
     - 784
     - 1.22x

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
     - 1.45x
     - 1.14x
     - 1.69x
     - ``for k in d``
   * - ``CIMultiDict``
     - 1.41x
     - 1.14x
     - 1.60x
     - ``for k in d``
   * - ``MultiDict`` (Python)
     - 1.22x
     - 0.94x
     - 2.96x
     - ``for k in d``
   * - ``CIMultiDict`` (Python)
     - 1.21x
     - 0.94x
     - 2.08x
     - ``for k in d``

.. list-table:: Pure-Python backend, instructions per operation, ``CPython 3.14.7``, GIL build
   :header-rows: 1
   :widths: 34 20 20 26

   * - Operation
     - ``MultiDict``
     - ``CIMultiDict``
     - ``MultiDict`` vs the C extension
   * - ``cls(items)``
     - 2,476,221
     - 2,779,357
     - 55.20x
   * - ``cls()``
     - 64,665
     - 65,004
     - 105.15x
   * - ``cls(items)``, 20 items
     - 329,129
     - 358,945
     - 77.83x
   * - ``d.copy()``
     - 450,183
     - 456,734
     - 17.69x
   * - ``d.copy()``, 20 items
     - 75,583
     - 82,049
     - 39.59x
   * - ``d[key]``
     - 7,414
     - 8,979
     - 50.00x
   * - ``d.get(key)``, miss
     - 7,670
     - 9,224
     - 24.46x
   * - ``key in d``
     - 7,694
     - 9,258
     - 57.37x
   * - ``d[key] = v``, existing key
     - 26,143
     - 27,758
     - 79.32x
   * - ``d[key] = v``, new key
     - 27,034
     - 28,387
     - 67.07x
   * - ``d.setdefault(key, v)``, new key
     - 34,659
     - 37,476
     - 67.65x
   * - ``del d[key]``
     - 19,636
     - 21,144
     - 78.29x
   * - ``d.pop(key)``
     - 15,432
     - 16,979
     - 39.49x
   * - ``d.popitem()``
     - 11,919
     - 13,856
     - 23.72x
   * - ``d.update(other)``, 100 existing keys
     - 2,045,032
     - 2,192,872
     - 55.48x
   * - ``d.clear()``
     - 114,191
     - 114,183
     - 15.81x
   * - ``for k in d``
     - 2,190
     - 4,071
     - 36.45x
   * - ``for k, v in d.items()``
     - 2,510
     - 4,450
     - 19.35x
   * - ``d.items()``
     - 2,501
     - 2,501
     - 7.67x
   * - ``iter(d)``
     - 9,164
     - 9,162
     - 18.33x

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
