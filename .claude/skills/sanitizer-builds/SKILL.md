---
name: sanitizer-builds
description: Build and test the multidict C extension under AddressSanitizer, UndefinedBehaviorSanitizer or ThreadSanitizer (MULTIDICT_ASAN_BUILD, MULTIDICT_TSAN_BUILD), including the required LD_PRELOAD, PYTHONMALLOC and suppression setup.
---

# Sanitizer builds

Sanitizers are opt-in on top of `MULTIDICT_DEBUG_BUILD=1`, not implied
by it: plain `MULTIDICT_DEBUG_BUILD=1` is relied on across CI (and by
contributors) to just build with `-O0`/`-UNDEBUG` and run normally,
with no sanitizer runtime preloaded. Add `MULTIDICT_ASAN_BUILD=1` to
also compile and link the C extension with AddressSanitizer and
UndefinedBehaviorSanitizer (skipped on Windows, where these flags
aren't supported by MSVC). Because the extension is then loaded into
a normal CPython that wasn't itself built with ASan, the runtime has
to be preloaded ahead of everything else:

```bash
ASAN_SO=$(cc -print-file-name=libasan.so)
MULTIDICT_DEBUG_BUILD=1 MULTIDICT_ASAN_BUILD=1 \
    pip install -e . --force-reinstall --no-deps
LD_PRELOAD="$ASAN_SO" ASAN_OPTIONS=detect_leaks=0 PYTHONMALLOC=malloc \
    python -m pytest tests -q -k "not test_leak"
```

`PYTHONMALLOC=malloc` routes every allocation through libc `malloc`,
which ASan intercepts. Without it, pymalloc serves small blocks
(512 bytes or less, which covers most hash tables) from its own
arenas; ASan sees the arena as one live allocation, so a freed block
is never flagged and use-after-free goes unreported. Free-threaded
builds do not support it: they abort at startup with
`PYTHONMALLOC: unknown allocator`, since only the mimalloc allocators
are available there. Run ASan on a GIL build.

Do not add `-I` (or `-E`) to that command. Both make Python ignore
`PYTHON*` environment variables, so `PYTHONMALLOC=malloc` is silently
dropped and small allocations (most hash tables) come from pymalloc
arenas, where ASan cannot see use-after-free.

Run the suite a second time with `MULTIDICT_NO_FREELIST=1` added to
the install. The extension keeps bounded pools of freed hash tables and
object shells in its module state, and a pooled block never reaches
`free()`, so ASan can neither poison it nor report a use-after-free on
it. That flag makes every pool a miss, which puts those paths back under
ASan's redzones and quarantine. Deselect
`test_freed_blocks_are_reused` on that run, the way `test_leaks.py` is
deselected above: it asserts that a pool hands a block back out, which
is the very thing the flag turns off.

`detect_leaks=0` and excluding `test_leaks.py` are required: CPython
itself retains allocations at shutdown (interned strings, caches)
that LeakSanitizer reports as leaks, and `test_leaks.py` asserts on
process RSS growth, which ASan's redzones/quarantine inflate well
past the test's threshold regardless of any real multidict
behaviour. Neither is a multidict bug; both are just sanitizer
overhead interacting with checks that assume an uninstrumented
process.

ThreadSanitizer can't be linked into the same binary as ASan/UBSan,
and running it against a normal CPython produces false positives
from the interpreter's own internals (its locks aren't all built
from primitives TSan recognizes unless the interpreter itself is
TSan-instrumented). Use `MULTIDICT_TSAN_BUILD=1` instead of the
default sanitizer set, together with a free-threaded CPython built
with `--with-thread-sanitizer` (a normal `--disable-gil` build is
not enough):

```bash
CC=clang CXX=clang++ PYTHON_CONFIGURE_OPTS="--with-thread-sanitizer" \
    PYTHON_BUILD_FREE_THREADING=1 \
    python-build 3.14.7t ~/.pyenv/versions/3.14.7t-tsan   # one-time, slow

TSAN_PY=~/.pyenv/versions/3.14.7t-tsan/bin/python3.14t
CC=clang CXX=clang++ MULTIDICT_DEBUG_BUILD=1 MULTIDICT_TSAN_BUILD=1 \
    $TSAN_PY -m pip install -e . --force-reinstall --no-deps
TSAN_OPTIONS="halt_on_error=0:suppressions=tools/tsan_suppressions.txt" \
    $TSAN_PY -m pytest tests -q --no-cov -k "not test_leak"
```

`--no-cov` matters: `pytest.ini` turns coverage on by default, and its
tracer makes a TSan run about eight times slower. That pushes slow
tests past `faulthandler_timeout`, and pure-Python stress tests past
their own time limits. [`tools/tsan_suppressions.txt`](tools/tsan_suppressions.txt)
holds the reports that are benign by design and outside multidict,
so far only pytest's faulthandler watchdog; add an entry only with the
same justification in a comment, never to hide a race in multidict.

A TSan run is much slower than normal (single stress tests can take
30-50s instead of well under a second); don't be surprised if it
takes minutes to get through the suite.

CI runs across the supported CPython versions plus a wheel build for
manylinux, musllinux, macOS, Windows, iOS, and Android, plus a
pure-Python leg under `MULTIDICT_NO_EXTENSIONS=1`. Do not regress
the benchmarks under `benchmarks/` without flagging the trade-off
in the PR body.
