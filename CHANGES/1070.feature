Implemented a custom parser for ``METH_FASTCALL | METH_KEYWORDS`` protocol
-- by :user:`asvetlov`.

The patch re-enables fast call protocol in the :py:mod:`multidict` C Extension.

Speedup is about 25%-30% for the library benchmarks for Python 3.12+.
