.. _benchmarking-reference:

==========
Benchmarks
==========

Introduction
------------

Benchmarks allow to track performance from release to release and verify
that latest changes haven not affected it drastically. Benchmarks are based on
the `perf <https://perf.readthedocs.io>`_ module.

How to run
----------

`requirements/dev.txt` should be installed before we can proceed
with benchmarks. Please also make sure that you have
`configured <https://perf.readthedocs.io/en/latest/system.html>`_
your OS to have reliable results.

To run benchmarks next command can be executed:

.. code-block:: bash

    $ python benchmarks/benchmark.py

This would run benchmarks for both classes (:class:`MultiDict` and
:class:`CIMultiDict`) of both implementations (`Python` and `Cython`).

To run benchmarks for a specific class of specific implementation
please use `--impl` option:

.. code-block:: bash

    $ python benchmarks/benchmark.py --impl multidict_cython

would run benchmarks only for :class:`MultiDict` implemented in `Cython`.

Please use `--help` to see all available options. Most of the options are
described at `perf's Runner <https://perf.readthedocs.io/en/latest/runner.html>`_
documentation.

How to compare implementations
------------------------------

`--impl` option allows to run benchmarks for a specific implementation of
class. Combined with the
`compare_to <https://perf.readthedocs.io/en/latest/cli.html#compare-to-cmd>`_
command of :mod:`perf` module we can get a good picture of how implementation
performs:

.. code-block:: bash

    $ python benchmarks/benchmark.py --impl multidict_cython -o multidict_cy.json
    $ python benchmarks/benchmark.py --impl multidict_python -o multidict_py.json
    $ python -m perf compare_to multidict_cy.json multidict_py.json
