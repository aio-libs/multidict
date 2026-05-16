# Notes for LLM contributors

Read this before opening a pull request against `aio-libs/multidict`.
Agents keep getting the same things wrong in this repo, so the rules
below are not optional. If you are about to skip a section because it
sounds like boilerplate, that is the section to re-read.

Human-facing contributor docs live under
[CHANGES/README.rst](CHANGES/README.rst); this file is the short
orientation for agents.

## What this project is

`multidict` is the case-insensitive, multi-value mapping used by
`aiohttp`, `yarl`, and the rest of the `aio-libs` stack. It is small,
widely deployed, and performance sensitive. It ships **two parallel
implementations that must stay behaviourally identical**:

- Pure Python: `multidict/_multidict_py.py`
- C extension: `multidict/_multidict.c`, plus headers in
  `multidict/_multilib/`

Useful entry points:

| Path                                  | What                                                            |
| ------------------------------------- | --------------------------------------------------------------- |
| `multidict/__init__.py`               | public surface; chooses C vs. pure-Python impl at import time   |
| `multidict/_abc.py`                   | `MultiMapping`, `MutableMultiMapping`, type protocols           |
| `multidict/_multidict_py.py`          | pure-Python `MultiDict`, `CIMultiDict`, `istr`, proxies         |
| `multidict/_multidict.c`              | C implementation entry points and type definitions              |
| `multidict/_multilib/hashtable.h`     | core hash table (``md_*`` functions, resize, lookup)            |
| `multidict/_multilib/htkeys.h`        | key-storage layout, ``estimate_log2_keysize``                   |
| `multidict/_multilib/istr.h`          | C ``istr`` (case-insensitive str)                               |
| `multidict/_multilib/iter.h`          | views and iterators for the C impl                              |
| `multidict/_multilib/parser.h`        | argument parsing for ``extend`` / ``update`` / constructors     |
| `multidict/_multilib/pythoncapi_compat.h` | vendored upstream; do not edit                              |
| `tests/`                              | pytest suite, parametrised across both backends                 |
| `CHANGES/`                            | towncrier news fragments, one per PR                            |

`MULTIDICT_NO_EXTENSIONS=1` forces the pure-Python build at install
time; the default is the C extension. `MULTIDICT_DEBUG_BUILD=1` builds
the C extension with `-O0 -g3 -UNDEBUG`.

## Pull request rules

These are the rules agents most often violate. Treat them as mandatory.

### 1. Use the aio-libs pull request template

`multidict` follows the standard `aio-libs` PR template. Even though
this repo does not ship its own `.github/PULL_REQUEST_TEMPLATE.md`,
maintainers expect every PR body to follow the structure below.
Do not invent your own `## What / ## Why / ## How / ## Testing`
layout; that is the marker that the PR was written by an agent
without reading the conventions, and PRs that use it have been
closed on sight.

Fill out the template verbatim, like so:

```markdown
<!-- Thank you for your contribution! -->

## What do these changes do?

<short prose describing the change>

## Are there changes in behavior for the user?

<yes or no, plus a sentence if yes>

## Is it a substantial burden for the maintainers to support this?

<no, plus a sentence on why if relevant>

## Related issue number

Fixes #NNNN
<!-- or a bare reference if related but not closing -->

## Checklist

- [x] I think the code is well written
- [x] Unit tests for the changes exist
- [x] Documentation reflects the changes
- [ ] If you provide code modification, please add yourself to `CONTRIBUTORS.txt`
- [x] Add a new news fragment into the `CHANGES/` folder
```

Tick the boxes that actually apply. If a row does not apply (e.g. a
CI-only change with no tests, or no `CONTRIBUTORS.txt` in this repo),
write `N/A` next to it rather than silently leaving it blank.

For real filled-out examples in this repo, see
[#1316](https://github.com/aio-libs/multidict/pull/1316) and
[#1326](https://github.com/aio-libs/multidict/pull/1326).

### 2. Add a CHANGES fragment

Every user-visible or contributor-visible PR needs a towncrier news
fragment in `CHANGES/`, named `<pr_number>.<category>.rst`. Categories
(defined in [CHANGES/README.rst](CHANGES/README.rst) and
[towncrier.toml](towncrier.toml)):

| Category       | When to use                                                     |
| -------------- | --------------------------------------------------------------- |
| `bugfix`       | corrects undesired behaviour                                    |
| `feature`      | new public API or behaviour                                     |
| `deprecation`  | announces a future removal                                      |
| `breaking`     | removes or changes something public in a breaking way           |
| `doc`          | documentation structure or build process                        |
| `packaging`    | downstream-visible packaging or build changes                   |
| `contrib`      | contributor experience (CI, dev env, test invocation)           |
| `misc`         | does not fit any of the above                                   |

Conventions for the fragment body:

- Use the past tense (`Fixed`, `Added`, `Bumped`), since it is read as
  a "what changed since the previous release" digest.
- Use reStructuredText, not Markdown.
- Do not include the issue or PR number in the body; towncrier adds
  it automatically from the filename.
- Sign with `` -- by :user:`github-handle` `` at the end.
- For multiple fragments in the same category, append a sequence
  number: `1326.breaking.rst`, `1326.breaking.1.rst`.

Example (`CHANGES/1310.bugfix.rst` style):

```rst
A segmentation fault that could be triggered when getting an item
is now fixed -- by :user:`Vizonex`.
```

Picking the number for the filename:

- **If there is a linked issue**, name the fragment with the issue
  number (e.g. `CHANGES/1284.bugfix.rst`). Issue numbers are
  stable and known before the PR exists.
- **If there is no linked issue**, you will not know the PR number
  before pushing. Open the PR first to get the number, then add
  the fragment as `CHANGES/<pr_number>.<category>.rst` in a
  follow-up commit on the same branch.
- **If both exist** (linked issue plus a known PR number), keep the
  fragment file at the issue number and add a symlink from
  `CHANGES/<pr_number>.<category>.rst` to the issue-numbered file
  so towncrier picks up the cross-reference:

  ```bash
  ln -s 1284.bugfix.rst CHANGES/1339.bugfix.rst
  git add CHANGES/1339.bugfix.rst
  ```

### 3. Open the PR as a draft

Use `gh pr create --draft`. **A human marks the PR ready, never
the agent.** The PR stays in draft until a human has fully
reviewed the change end-to-end; only then do they flip it to
ready-for-review. The agent must not mark its own PR ready and
must not request reviewers.

### 4. Disclose the agent, keep agent output out of the summary

Disclosure is welcome. Put one plain line naming the agent that
drafted the change, for example:

```
Drafted with <agent name and version>; reviewed by <human handle>.
```

If you need to include additional agent-generated output (a test
plan summary, a code-scan result, a "Quality Report" block, links
to a CI run, etc.), put it in a **footer below the PR summary**,
wrapped in a collapsed `<details>` block so human reviewers can
skip past it:

```markdown
<details>
<summary>Agent output</summary>

...test plan, scan results, etc...

</details>
```

The summary itself must stay short and human-readable; agent
boilerplate goes after it, never inside it.

Beyond that:

- **No `Co-Authored-By:` trailers** for an LLM or any AI tool, in
  commits or in the PR body. Attribution goes to the human who
  reviewed the change.
- No emoji decoration (`🤖`, `✨`, `🚀`) in commit messages, PR
  titles, PR bodies, or news fragments. Project style is plain prose.
- Commit messages and PR prose should read as if a human contributor
  wrote them. Specifically:
  - **No em-dashes (`—`)** and no dashes used as sentence separators
    (`foo - bar`). Use a semicolon or a comma. This is the strongest
    tell for AI-generated prose in this project, and reviewers do
    read for it.
  - No "Let me", "I'll", or first-person narration of what the agent
    did. Describe the change, not the author.
  - No filler sections ("Overview", "Summary of changes", "Key
    takeaways") on top of the template. The template already has
    the right sections.

### 5. Keep the PR body short

A couple of sentences per template section is plenty. If the change
is non-obvious, a short reproducer or a paragraph on root cause is
welcome. Long, multi-section essays with bolded sub-headings are not
the style here.

### 6. Commit hygiene

- One logical change per PR. If a refactor and a bugfix are bundled
  together, split them.
- Pre-commit hooks (ruff, clang-format, mypy, yamllint, the changelog
  filename check, and others under
  [`.pre-commit-config.yaml`](.pre-commit-config.yaml)) rewrite files
  in place and may abort the commit; when that happens, re-stage and
  commit again. Do not pass `--no-verify`.
- The repo does **not** use Conventional Commits as a CI gate. Recent
  landed subjects are short imperative or descriptive prose (e.g.
  `Drop 3.13t support`, `Bump pytest-codspeed from 5.0.1 to 5.0.2`).
  Match that style; do not force `feat:` / `fix:` prefixes onto every
  commit.

## Dual-backend discipline

This is the single biggest source of broken multidict PRs from agents.

When you change behaviour in one implementation, check whether the
other needs the same change:

- A bug fix in `_multidict_py.py` usually needs a matching fix in
  `_multidict.c` / `_multilib/hashtable.h`. If the C path is
  genuinely unaffected (different code path, different invariant),
  say so explicitly in the "What do these changes do?" section.
- A new public API must land in both backends with identical
  signatures, identical behaviour, identical type hints in
  `_abc.py`, and matching docstrings.
- Tests in `tests/` are parametrised across both backends by
  `tests/conftest.py`, so a divergence will surface as a test
  failure on one of the two legs.

If you can only fix one backend in scope, file a follow-up issue
and call it out in the PR body. Do not silently leave the
implementations divergent.

## Tests

Install dev deps and run the suite:

```bash
make install-dev   # installs deps and builds the C extension in place
make test          # pytest -q against whichever build is installed
```

To exercise both backends explicitly:

```bash
# C extension (default)
pip install -e .
pytest -q

# Pure Python
MULTIDICT_NO_EXTENSIONS=1 pip install -e . --force-reinstall --no-deps
pytest -q
```

`make lint` runs the full pre-commit suite across the tree;
`make cov-dev` runs the suite with coverage.

The C extension is compiled with `-Werror -Wsign-compare -Wconversion
-std=c11` (see [setup.py](setup.py)). Casts must be explicit;
signed/unsigned comparisons will fail the build, not just warn.

CI runs across the supported CPython versions plus a wheel build for
manylinux, musllinux, macOS, Windows, iOS, and Android, plus a
pure-Python leg under `MULTIDICT_NO_EXTENSIONS=1`. Do not regress
the benchmarks under `benchmarks/` without flagging the trade-off
in the PR body.

## Code style

- `pyproject.toml` sets `[tool.ruff]` `target-version = "py310"` with
  only the `UP` (pyupgrade) lint group enabled. Do not enable other
  lint groups as a drive-by; that is its own PR.
- Minimum Python is **3.10**. Match the surrounding file's import
  and typing conventions; do not introduce `from __future__ import
  annotations` to files that do not already use it.
- Do not add docstrings or comments that just restate the code.
  Match the existing terse style in `_multidict_py.py`.

## Things not to do

- Do not invent a `## What / ## Why / ## How / ## Testing` PR body;
  use the aio-libs template above.
- Do not skip the `CHANGES/` fragment "because the change is small".
  Even a one-line bugfix needs one.
- Do not add `Co-Authored-By` trailers for LLM tools, in either
  commits or the PR body.
- Do not paste an agent-generated "Quality Report", "Test plan
  summary", or "Generated by <pipeline>" block **inside the PR
  summary**. If you must include it, drop it in a collapsed
  `<details>` footer below the summary so reviewers can skip it.
- Do not use em-dashes or sentence-separating dashes in PR prose or
  commit messages.
- Do not edit `multidict/_multilib/pythoncapi_compat.h`; it is
  vendored upstream.
- Do not commit build artefacts (`*.so`, `__pycache__`,
  `multidict/_multilib/views.h.old`).
- Do not change one backend without checking the other; see
  "Dual-backend discipline" above.
- Do not request reviewers from the agent session; leave the PR as
  a draft and let the maintainer route it.
