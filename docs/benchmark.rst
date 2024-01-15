.. _benchmarking-reference:

==========
Benchmarks
==========

Introduction
------------

Benchmarks allow to track performance from release to release and verify
that latest changes haven not affected it drastically. Benchmarks are based on
the :doc:`pyperf:index`.

How to run
----------

``requirements/dev.txt`` should be installed before we can proceed
with benchmarks. Please also make sure that you have :doc:`configured
<pyperf:system>` your OS to have reliable results.

To run benchmarks next command can be executed:

.. code-block:: bash

    $ python benchmarks/benchmark.py

This would run benchmarks for both classes (:class:`~multidict.MultiDict`
and :class:`~multidict.CIMultiDict`) of both implementations (pure-Python
and C).

To run benchmarks for a specific class of specific implementation
please use ``--impl`` option:

.. code-block:: bash

    $ python benchmarks/benchmark.py --impl multidict_c

would run benchmarks only for :class:`~multidict.MultiDict` implemented
in C.

Please use ``--help`` to see all available options. Most of the options are
described at :doc:`perf's Runner <pyperf:runner>` documentation.

How to compare implementations
------------------------------

``--impl`` option allows to run benchmarks for a specific implementation of
class. Combined with the :ref:`compare_to <pyperf:compare_to_cmd>` command of
:doc:`pyperf:index` we can get a good picture of how implementation performs:

.. code-block:: bash

    $ python benchmarks/benchmark.py --impl multidict_c -o multidict_cy.json
    $ python benchmarks/benchmark.py --impl multidict_py -o multidict_py.json
    $ python -m perf compare_to multidict_cy.json multidict_py.json
