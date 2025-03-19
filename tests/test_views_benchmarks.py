"""codspeed benchmarks for multidict views."""

from typing import Dict, Union

from pytest_codspeed import BenchmarkFixture

from multidict import MultiDict


def test_keys_view_equals(benchmark: BenchmarkFixture) -> None:
    md1: MultiDict[str] = MultiDict({str(i): str(i) for i in range(100)})
    md2: MultiDict[str] = MultiDict({str(i): str(i) for i in range(100)})

    @benchmark
    def _run() -> None:
        assert md1.keys() == md2.keys()


def test_keys_view_not_equals(benchmark: BenchmarkFixture) -> None:
    md1: MultiDict[str] = MultiDict({str(i): str(i) for i in range(100)})
    md2: MultiDict[str] = MultiDict({str(i): str(i) for i in range(20, 120)})

    @benchmark
    def _run() -> None:
        assert md1.keys() != md2.keys()


def test_keys_view_more(benchmark: BenchmarkFixture) -> None:
    md: MultiDict[str] = MultiDict({str(i): str(i) for i in range(100)})
    s = {str(i) for i in range(50)}

    @benchmark
    def _run() -> None:
        assert md.keys() > s


def test_keys_view_more_or_equal(benchmark: BenchmarkFixture) -> None:
    md: MultiDict[str] = MultiDict({str(i): str(i) for i in range(100)})
    s = {str(i) for i in range(100)}

    @benchmark
    def _run() -> None:
        assert md.keys() >= s


def test_keys_view_less(benchmark: BenchmarkFixture) -> None:
    md: MultiDict[str] = MultiDict({str(i): str(i) for i in range(100)})
    s = {str(i) for i in range(150)}

    @benchmark
    def _run() -> None:
        assert md.keys() < s


def test_keys_view_less_or_equal(benchmark: BenchmarkFixture) -> None:
    md: MultiDict[str] = MultiDict({str(i): str(i) for i in range(100)})
    s = {str(i) for i in range(100)}

    @benchmark
    def _run() -> None:
        assert md.keys() <= s

def test_keys_view_and(benchmark: BenchmarkFixture) -> None:
    md1: MultiDict[str] = MultiDict({str(i): str(i) for i in range(100)})
    md2: MultiDict[str] = MultiDict({str(i): str(i) for i in range(50, 150)})

    @benchmark
    def _run() -> None:
        assert len(md1.keys() & md2.keys()) == 50


def test_keys_view_or(benchmark: BenchmarkFixture) -> None:
    md1: MultiDict[str] = MultiDict({str(i): str(i) for i in range(100)})
    md2: MultiDict[str] = MultiDict({str(i): str(i) for i in range(50, 150)})

    @benchmark
    def _run() -> None:
        assert len(md1.keys() | md2.keys()) == 150


def test_keys_view_sub(benchmark: BenchmarkFixture) -> None:
    md1: MultiDict[str] = MultiDict({str(i): str(i) for i in range(100)})
    md2: MultiDict[str] = MultiDict({str(i): str(i) for i in range(50, 150)})

    @benchmark
    def _run() -> None:
        assert len(md1.keys() - md2.keys()) == 50


def test_keys_view_xor(benchmark: BenchmarkFixture) -> None:
    md1: MultiDict[str] = MultiDict({str(i): str(i) for i in range(100)})
    md2: MultiDict[str] = MultiDict({str(i): str(i) for i in range(50, 150)})

    @benchmark
    def _run() -> None:
        assert len(md1.keys() ^ md2.keys()) == 100


def test_keys_view_is_disjoint(benchmark: BenchmarkFixture) -> None:
    md1: MultiDict[str] = MultiDict({str(i): str(i) for i in range(100)})
    md2: MultiDict[str] = MultiDict({str(i): str(i) for i in range(100, 200)})

    @benchmark
    def _run() -> None:
        assert md1.keys().isdisjoint(md2.keys())


def test_keys_view_repr(benchmark: BenchmarkFixture) -> None:
    md: MultiDict[str] = MultiDict({str(i): str(i) for i in range(100)})

    @benchmark
    def _run() -> None:
        repr(md.keys())


def test_items_view_repr(benchmark: BenchmarkFixture) -> None:
    md: MultiDict[str] = MultiDict({str(i): str(i) for i in range(100)})

    @benchmark
    def _run() -> None:
        repr(md.items())


def test_values_view_repr(benchmark: BenchmarkFixture) -> None:
    md: MultiDict[str] = MultiDict({str(i): str(i) for i in range(100)})

    @benchmark
    def _run() -> None:
        repr(md.values())
