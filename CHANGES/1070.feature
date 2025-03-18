Implement a custom parser for ``METH_FASTCALL | METH_KEYWORDS`` protocol
-- by :user:`asvetlov`.

The patch enables fast call protocol for ``multidict`` C Extension again.

Speedup is about 25%-30% for the library benchmarks for Python 3.12+.