# Releasing multidict

This is the maintainer checklist for cutting a release. It is also
where the benchmark tables in
[`docs/benchmark.rst`](docs/benchmark.rst) get refreshed: they are a
per-release chore, not a per-PR one, because regenerating them on
every pull request produces a stream of noisy diffs that nobody can
review.

Contributor-facing rules live in [AGENTS.md](AGENTS.md) and
[CHANGES/README.rst](CHANGES/README.rst).

## Versioning

`multidict/__init__.py`'s `__version__` is the single source of
truth; `setup.cfg` reads it through `version = attr:` and the
release workflow reads it to name the GitHub Release. Between
releases it carries a `.dev0` suffix, so `6.9.2.dev0` means "6.9.2
is not out yet".

The numbering is the usual three-part scheme: a patch release for
bug fixes, a minor release once a new public API or a behaviour
change lands, a major release for removals. Dropping a Python
version is a minor bump.

## 1. Preflight

- Everything intended for the release is merged, and `master` is
  green. The nightly run matters here: it covers the sanitizer,
  Hypothesis and cross-architecture wheel jobs that a quiet day of
  PRs may not have exercised together.
- Read through the `CHANGES/` fragments as a group, the way a user
  will read the rendered section. Fix tense, wording and missing
  `` :user:`handle` `` signatures now; towncrier copies them
  verbatim.
- Build the docs, including the changelog draft:

  ```bash
  make doc doc-spelling
  ```

## 2. Refresh the benchmark tables

The tables publish real instruction counts, so a release must not
ship numbers collected against an older tree. Regenerate them even
when no single PR in the cycle looked like a performance change;
small wins and losses accumulate.

This needs Valgrind and one virtualenv per interpreter build, both
on the same CPython patch release. [`docs/benchmark.rst`](docs/benchmark.rst)
documents the setup and the ways it goes wrong silently.

```bash
.venv-gil/bin/python benchmarks/callgrind_driver.py -o gil.json
.venv-ft/bin/python  benchmarks/callgrind_driver.py -o ft.json
python benchmarks/render_tables.py gil.json ft.json --write docs/benchmark.rst
```

Then:

- Check the prose that quotes those numbers, since the generator
  does not touch it: the performance paragraphs in
  [`README.rst`](README.rst) and [`docs/index.rst`](docs/index.rst)
  cite dict-relative ratios that go stale the same way the tables
  do.
- Land this as its own pull request with a `doc` fragment, before
  the release PR. Say in the body which rows moved and by how much;
  that summary is the only record of where the cycle's performance
  went.

## 3. Open the release pull request

Two changes, one commit, titled `Release X.Y.Z`:

```bash
python -Im towncrier build --draft --version X.Y.Z   # preview only
python -Im towncrier build --version X.Y.Z --yes     # writes CHANGES.rst,
                                                     # removes the fragments
sed -i 's/^__version__ = .*/__version__ = "X.Y.Z"/' multidict/__init__.py
```

Check the result before pushing:

- `CHANGES.rst` gained a section headed by the bare version, then a
  blank line, then `*(YYYY-MM-DD)*`. The release workflow greps for
  exactly that shape to pull the release notes out, so a hand-edit
  that reflows the heading breaks the GitHub Release body.
- The `CHANGES/` directory holds nothing but `README.rst` and
  `.TEMPLATE.rst`.
- `make doc-spelling` passes on the rendered `CHANGES.rst`.

The PR body follows the normal template; mark the `CHANGES/`
fragment row `N/A`, since towncrier has just consumed them.

## 4. Tag the merged commit

Tags are annotated, and the tag is what triggers publication. Tag
the release commit on `master` (or on the maintenance branch, whose
name matches `[0-9].[0-9]+`), never a local commit that is not
upstream yet:

```bash
git fetch upstream
git tag -a vX.Y.Z -m "Release X.Y.Z" <release-commit-sha>
git push upstream vX.Y.Z
```

Pushing the tag starts the CI/CD run in
[`.github/workflows/ci-cd.yml`](.github/workflows/ci-cd.yml). It runs
the full test and wheel matrix first, then builds every wheel
including the QEMU-emulated architectures that regular runs skip, so
expect 60 to 90 minutes before anything is publishable.

The `deploy` job at the end of that run does not start on its own.
It runs in the `pypi` environment, which requires a reviewer, so it
sits in a pending state until someone approves it from the run's page
in the GitHub UI. Nothing is published before that click; use the
wait to watch the matrix, and approve once it is green. Once
approved, `deploy` creates the GitHub Release from the `CHANGES.rst`
section matching the tag, publishes to PyPI through trusted
publishing, and signs the artifacts with Sigstore.

## 5. Verify

- The PyPI page lists the sdist, the pure-Python wheel and the
  binary wheels for every supported platform.
- The GitHub Release exists, its body is the changelog section, and
  the `.sigstore.json` signatures are attached.
- `pip install --no-binary :all: multidict` and a plain
  `pip install multidict` both work in a throwaway virtualenv, so
  that the sdist and a wheel are each proven installable.

If the `deploy` job fails partway, re-run it rather than tagging
again. It is written to be idempotent: the GitHub Release is only
created when it does not already exist, PyPI uploads skip what is
already published, and asset uploads skip files already attached.

A tag that never reached PyPI at all is a different case. Fixing
whatever broke and folding the fragments merged since into the
existing changelog section, with the date updated, is how 6.9.1 was
released; the alternative is burning the version number.

## 6. Reopen the development cycle

One more pull request, titled `Bump to X.Y.Z.dev0`, moving
`__version__` to the next patch release plus `.dev0`. It needs no
changelog fragment.
