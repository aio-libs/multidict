"""codspeed benchmarks for multidict."""

from typing import Dict, Union

from pytest_codspeed import BenchmarkFixture  # type: ignore[import-untyped]

from multidict import CIMultiDict, MultiDict, istr

# Note that this benchmark should not be refactored to use pytest.mark.parametrize
# since each benchmark name should be unique.


def test_multidict_insert_str_performance(benchmark: BenchmarkFixture) -> None:
    md: MultiDict[str] = MultiDict()
    items = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md[i] = i


def test_cimultidict_insert_str_performance(benchmark: BenchmarkFixture) -> None:
    md: CIMultiDict[str] = CIMultiDict()
    items = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md[i] = i


def test_cimultidict_insert_istr_performance(benchmark: BenchmarkFixture) -> None:
    md: CIMultiDict[istr] = CIMultiDict()
    items = [istr(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md[i] = i


def test_multidict_add_str_performance(benchmark: BenchmarkFixture) -> None:
    md: MultiDict[str] = MultiDict()
    items = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md.add(i, i)


def test_cimultidict_add_str_performance(benchmark: BenchmarkFixture) -> None:
    md: CIMultiDict[str] = CIMultiDict()
    items = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md.add(i, i)


def test_cimultidict_add_istr_performance(benchmark: BenchmarkFixture) -> None:
    md: CIMultiDict[istr] = CIMultiDict()
    items = [istr(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md.add(i, i)


def test_multidict_pop_str_performance(benchmark: BenchmarkFixture) -> None:
    md_base: MultiDict[str] = MultiDict((str(i), str(i)) for i in range(100))
    items = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        md = md_base.copy()
        for i in items:
            md.pop(i)


def test_cimultidict_pop_str_performance(benchmark: BenchmarkFixture) -> None:
    md_base: CIMultiDict[str] = CIMultiDict((str(i), str(i)) for i in range(100))
    items = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        md = md_base.copy()
        for i in items:
            md.pop(i)


def test_cimultidict_pop_istr_performance(benchmark: BenchmarkFixture) -> None:
    md_base: CIMultiDict[istr] = CIMultiDict((istr(i), istr(i)) for i in range(100))
    items = [istr(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        md = md_base.copy()
        for i in items:
            md.pop(i)


def test_multidict_popitem_str_performance(benchmark: BenchmarkFixture) -> None:
    md_base: MultiDict[str] = MultiDict((str(i), str(i)) for i in range(100))

    @benchmark
    def _run() -> None:
        md = md_base.copy()
        for _ in range(100):
            md.popitem()


def test_cimultidict_popitem_str_performance(benchmark: BenchmarkFixture) -> None:
    md_base: MultiDict[str] = MultiDict((str(i), str(i)) for i in range(100))

    @benchmark
    def _run() -> None:
        md = md_base.copy()
        for _ in range(100):
            md.popitem()


def test_multidict_clear_str_performance(benchmark: BenchmarkFixture) -> None:
    md: MultiDict[str] = MultiDict((str(i), str(i)) for i in range(100))

    @benchmark
    def _run() -> None:
        md.clear()


def test_cimultidict_clear_str_performance(benchmark: BenchmarkFixture) -> None:
    md: CIMultiDict[str] = CIMultiDict((str(i), str(i)) for i in range(100))

    @benchmark
    def _run() -> None:
        md.clear()


def test_multidict_update_str_performance(benchmark: BenchmarkFixture) -> None:
    md: MultiDict[str] = MultiDict((str(i), str(i)) for i in range(100))
    items = {str(i): str(i) for i in range(100, 200)}

    @benchmark
    def _run() -> None:
        md.update(items)


def test_cimultidict_update_str_performance(benchmark: BenchmarkFixture) -> None:
    md: CIMultiDict[str] = CIMultiDict((str(i), str(i)) for i in range(100))
    items = {str(i): str(i) for i in range(100, 200)}

    @benchmark
    def _run() -> None:
        md.update(items)


def test_cimultidict_update_istr_performance(benchmark: BenchmarkFixture) -> None:
    md: CIMultiDict[istr] = CIMultiDict((istr(i), istr(i)) for i in range(100))
    items: Dict[Union[str, istr], istr] = {istr(i): istr(i) for i in range(100, 200)}

    @benchmark
    def _run() -> None:
        md.update(items)


def test_multidict_extend_str_performance(benchmark: BenchmarkFixture) -> None:
    md: CIMultiDict[str] = CIMultiDict((str(i), str(i)) for i in range(100))
    items = {str(i): str(i) for i in range(200)}

    @benchmark
    def _run() -> None:
        md.extend(items)


def test_cimultidict_extend_str_performance(benchmark: BenchmarkFixture) -> None:
    md: CIMultiDict[str] = CIMultiDict((str(i), str(i)) for i in range(100))
    items = {str(i): str(i) for i in range(200)}

    @benchmark
    def _run() -> None:
        md.extend(items)


def test_cimultidict_extend_istr_performance(benchmark: BenchmarkFixture) -> None:
    md: CIMultiDict[istr] = CIMultiDict((istr(i), istr(i)) for i in range(100))
    items = {istr(i): istr(i) for i in range(200)}

    @benchmark
    def _run() -> None:
        md.extend(items)


def test_multidict_delitem_str_performance(benchmark: BenchmarkFixture) -> None:
    md_base: MultiDict[str] = MultiDict((str(i), str(i)) for i in range(100))
    items = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        md = md_base.copy()
        for i in items:
            del md[i]


def test_cimultidict_delitem_str_performance(benchmark: BenchmarkFixture) -> None:
    md_base: CIMultiDict[str] = CIMultiDict((str(i), str(i)) for i in range(100))
    items = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        md = md_base.copy()
        for i in items:
            del md[i]


def test_cimultidict_delitem_istr_performance(benchmark: BenchmarkFixture) -> None:
    md_base: CIMultiDict[istr] = CIMultiDict((istr(i), istr(i)) for i in range(100))
    items = [istr(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        md = md_base.copy()
        for i in items:
            del md[i]


def test_multidict_getall_str_hit_performance(benchmark: BenchmarkFixture) -> None:
    md: MultiDict[str] = MultiDict(("all", str(i)) for i in range(100))

    @benchmark
    def _run() -> None:
        md.getall("all")


def test_cimultidict_getall_str_hit_performance(benchmark: BenchmarkFixture) -> None:
    md: CIMultiDict[str] = CIMultiDict(("all", str(i)) for i in range(100))

    @benchmark
    def _run() -> None:
        md.getall("all")


def test_cimultidict_getall_istr_hit_performance(benchmark: BenchmarkFixture) -> None:
    all_istr = istr("all")
    md: CIMultiDict[istr] = CIMultiDict((all_istr, istr(i)) for i in range(100))

    @benchmark
    def _run() -> None:
        md.getall(all_istr)


def test_multidict_fetch_performance(benchmark: BenchmarkFixture) -> None:
    md: MultiDict[str] = MultiDict((str(i), str(i)) for i in range(100))
    items = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md[i]


def test_cimultidict_fetch_str_performance(benchmark: BenchmarkFixture) -> None:
    md: CIMultiDict[str] = CIMultiDict((str(i), str(i)) for i in range(100))
    items = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md[i]


def test_cimultidict_fetch_istr_performance(benchmark: BenchmarkFixture) -> None:
    md: CIMultiDict[istr] = CIMultiDict((istr(i), istr(i)) for i in range(100))
    items = [istr(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md[i]


def test_multidict_get_hit_performance(benchmark: BenchmarkFixture) -> None:
    md: MultiDict[str] = MultiDict((str(i), str(i)) for i in range(100))
    items = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md.get(i)


def test_multidict_get_miss_performance(benchmark: BenchmarkFixture) -> None:
    md: MultiDict[str] = MultiDict((str(i), str(i)) for i in range(100))
    items = [str(i) for i in range(100, 200)]

    @benchmark
    def _run() -> None:
        for i in items:
            md.get(i)


def test_cimultidict_get_hit_performance(benchmark: BenchmarkFixture) -> None:
    md: CIMultiDict[str] = CIMultiDict((str(i), str(i)) for i in range(100))
    items = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md.get(i)


def test_cimultidict_get_miss_performance(benchmark: BenchmarkFixture) -> None:
    md: CIMultiDict[str] = CIMultiDict((str(i), str(i)) for i in range(100))
    items = [str(i) for i in range(100, 200)]

    @benchmark
    def _run() -> None:
        for i in items:
            md.get(i)


def test_cimultidict_get_istr_hit_performance(benchmark: BenchmarkFixture) -> None:
    md: CIMultiDict[istr] = CIMultiDict((istr(i), istr(i)) for i in range(100))
    items = [istr(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md.get(i)


def test_cimultidict_get_istr_miss_performance(benchmark: BenchmarkFixture) -> None:
    md: CIMultiDict[istr] = CIMultiDict((istr(i), istr(i)) for i in range(100))
    items = [istr(i) for i in range(100, 200)]

    @benchmark
    def _run() -> None:
        for i in items:
            md.get(i)
