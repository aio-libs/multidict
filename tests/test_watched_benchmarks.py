"""codspeed benchmarks for mutating a watched multidict.

Each one mirrors a mutation benchmark in test_multidict_benchmarks.py, with
a no-op watcher attached, so the pair shows what watching adds on top of
the operation itself. A copy is not watched, so every copy is watched
inside the timed loop.
"""

from collections.abc import Callable, Iterator

import pytest
from pytest_codspeed import BenchmarkFixture

from multidict import MultiDict

_multidict = pytest.importorskip("multidict._multidict")
_testcapi = pytest.importorskip("multidict._testcapi")

pytestmark = pytest.mark.capi

Watch = Callable[[MultiDict[str]], None]


@pytest.fixture(scope="module")
def watch() -> Iterator[Watch]:
    watcher_id = _testcapi.md_add_noop_watcher()
    watch_noop = _testcapi.md_watch_noop

    def _watch(md: MultiDict[str]) -> None:
        watch_noop(watcher_id, md)

    yield _watch
    _testcapi.md_clear_watcher(watcher_id)


# The IDs follow test_multidict_benchmarks.py's `cs`/`ci` plus implementation
# scheme; watching is C-only, so the implementation is always `c`.
@pytest.fixture(
    scope="module", params=("MultiDict", "CIMultiDict"), ids=("cs-c", "ci-c")
)
def watched_class(request: pytest.FixtureRequest) -> type[MultiDict[str]]:
    return getattr(_multidict, request.param)  # type: ignore[no-any-return]


def test_watched_insert_str(
    benchmark: BenchmarkFixture, watched_class: type[MultiDict[str]], watch: Watch
) -> None:
    base_md = watched_class()
    items = [str(i) for i in range(200)]

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            watch(md)
            for i in items:
                md[i] = i


def test_watched_add_str(
    benchmark: BenchmarkFixture, watched_class: type[MultiDict[str]], watch: Watch
) -> None:
    base_md = watched_class()
    items = [str(i) for i in range(200)]

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            watch(md)
            for i in items:
                md.add(i, i)


def test_watched_add_same_key(
    benchmark: BenchmarkFixture, watched_class: type[MultiDict[str]], watch: Watch
) -> None:
    base_md = watched_class()
    values = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            watch(md)
            for v in values:
                md.add("key", v)


def test_watched_setitem_str(
    benchmark: BenchmarkFixture, watched_class: type[MultiDict[str]], watch: Watch
) -> None:
    md_base = watched_class((str(i), str(i)) for i in range(100))
    items = [(str(i), str(i) + " new") for i in range(100)]

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = md_base.copy()
            watch(md)
            for key, val in items:
                md[key] = val


def test_watched_pop_str(
    benchmark: BenchmarkFixture, watched_class: type[MultiDict[str]], watch: Watch
) -> None:
    md_base = watched_class((str(i), str(i)) for i in range(400))
    items = [str(i) for i in range(100, 300)]

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = md_base.copy()
            watch(md)
            for i in items:
                md.pop(i)


def test_watched_popitem_str(
    benchmark: BenchmarkFixture, watched_class: type[MultiDict[str]], watch: Watch
) -> None:
    md_base = watched_class((str(i), str(i)) for i in range(200))

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = md_base.copy()
            watch(md)
            for _ in range(200):
                md.popitem()


def test_watched_delitem_str(
    benchmark: BenchmarkFixture, watched_class: type[MultiDict[str]], watch: Watch
) -> None:
    md_base = watched_class((str(i), str(i)) for i in range(200))
    items = [str(i) for i in range(200)]

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = md_base.copy()
            watch(md)
            for i in items:
                del md[i]


def test_watched_clear_str(
    benchmark: BenchmarkFixture, watched_class: type[MultiDict[str]], watch: Watch
) -> None:
    md_base = watched_class((str(i), str(i)) for i in range(100))

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = md_base.copy()
            watch(md)
            md.clear()


def test_watched_update_str(
    benchmark: BenchmarkFixture, watched_class: type[MultiDict[str]], watch: Watch
) -> None:
    base_md = watched_class((str(i), str(i)) for i in range(150))
    items = {str(i): str(i) for i in range(100, 200)}

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            watch(md)
            md.update(items)


def test_watched_update_str_with_duplicates(
    benchmark: BenchmarkFixture, watched_class: type[MultiDict[str]], watch: Watch
) -> None:
    base_md = watched_class((str(i % 50), str(i)) for i in range(150))
    items = [(str(i % 75), str(i)) for i in range(100)]

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            watch(md)
            md.update(items)


def test_watched_extend_str(
    benchmark: BenchmarkFixture, watched_class: type[MultiDict[str]], watch: Watch
) -> None:
    base_md = watched_class((str(i), str(i)) for i in range(100))
    items = {str(i): str(i) for i in range(200)}

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            watch(md)
            md.extend(items)


def test_watched_merge_str(
    benchmark: BenchmarkFixture, watched_class: type[MultiDict[str]], watch: Watch
) -> None:
    base_md = watched_class((str(i), str(i)) for i in range(150))
    items = {str(i): str(i) for i in range(100, 200)}

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            watch(md)
            md.merge(items)
