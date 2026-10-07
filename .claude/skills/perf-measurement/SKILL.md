---
name: perf-measurement
description: Measure a multidict performance change for a PR with the callgrind driver (before/after per GIL and free-threaded build), and the rules for docs/benchmark.rst comparison tables and benchmarks/operations.py.
---

# Measuring a performance change

[`docs/benchmark.rst`](docs/benchmark.rst) publishes per-operation
instruction counts for `dict`, `MultiDict` and `CIMultiDict` on both
the GIL and the free-threaded build. They are real measurements, not
illustrations, but regenerating them in every PR that touches
performance is too noisy to review: every row shifts a little on every
run, so the table churns on changes that did not move it. Do not
regenerate them in a feature or bugfix PR. They are refreshed once per
release; see [RELEASE.md](RELEASE.md).

What a performance change owes a reviewer instead is a measurement in
the PR body: which operations moved, by how much, and how you measured
it. Collect a before/after pair per interpreter build the change can
reach, reinstalling the extension into each virtualenv in between.
Anything touching atomics, locking or the free-threaded paths means
both builds:

```bash
.venv-gil/bin/python benchmarks/callgrind_driver.py -o gil-before.json
.venv-ft/bin/python  benchmarks/callgrind_driver.py -o ft-before.json
# apply the change, then per venv:
#     <venv>/bin/pip install -e . --force-reinstall --no-deps
.venv-gil/bin/python benchmarks/callgrind_driver.py -o gil-after.json
.venv-ft/bin/python  benchmarks/callgrind_driver.py -o ft-after.json
```

One run measures every operation in the table; the driver has no flag
to pick a single one, so narrow the report rather than the run and
quote the rows that moved. `--impl` restricts it to one
implementation, and `--include-multidict-only` adds the operations
`dict` has no counterpart for. Compare a GIL run against a GIL run and
a free-threaded run against a free-threaded run; the two builds are
separate baselines, and one is not a control for the other.

The measurement is deterministic, so it does not need a quiet machine;
it does need Valgrind, `requirements/pytest.txt` (for the
`pytest-codspeed` client requests; the driver refuses to run without
them unless given `--whole-process`, whose numbers are not comparable)
and one virtualenv per interpreter build, both on the same CPython
patch release. Before and after trees may live at different paths; the
driver runs its children from a fixed-length staging directory so
neither the path length nor stray build products shift the heap, but a
few instructions of delta on an allocating row still deserves a
base-to-base control.
`docs/benchmark.rst` has the setup and
the traps. Adding or renaming a benchmarked operation means editing
`benchmarks/operations.py`, which is the single registry all three
entry points read; run `python benchmarks/callgrind_driver.py
--self-check` afterwards. Adding or renaming an operation does change
the table's shape rather than just its numbers, so that is the one case
where a regular PR regenerates it.
