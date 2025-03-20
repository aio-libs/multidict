"""codspeed benchmarks for multidict."""

from typing import Dict, Type, Union

from pytest_codspeed import BenchmarkFixture

from multidict import (
    CIMultiDict,
    CIMultiDictProxy,
    MultiDict,
    MultiDictProxy,
    istr,
)

# Note that this benchmark should not be refactored to use pytest.mark.parametrize
# since each benchmark name should be unique.

_SENTINEL = object()


def test_multidict_insert_str(
    benchmark: BenchmarkFixture, any_multidict_class: Type[MultiDict[str]]
) -> None:
    md = any_multidict_class()
    items = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md[i] = i


def test_cimultidict_insert_istr(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: Type[CIMultiDict[istr]],
) -> None:
    md = case_insensitive_multidict_class()
    items = [istr(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md[i] = i


def test_multidict_add_str(
    benchmark: BenchmarkFixture, any_multidict_class: Type[MultiDict[str]]
) -> None:
    base_md = any_multidict_class()
    items = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            for i in items:
                md.add(i, i)


def test_cimultidict_add_istr(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: Type[CIMultiDict[istr]],
) -> None:
    base_md = case_insensitive_multidict_class()
    items = [istr(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for j in range(100):
            md = base_md.copy()
            for i in items:
                md.add(i, i)


def test_multidict_pop_str(
    benchmark: BenchmarkFixture, any_multidict_class: Type[MultiDict[str]]
) -> None:
    md_base = any_multidict_class((str(i), str(i)) for i in range(100))
    items = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        md = md_base.copy()
        for i in items:
            md.pop(i)


def test_cimultidict_pop_istr(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: Type[CIMultiDict[istr]],
) -> None:
    md_base = case_insensitive_multidict_class((istr(i), istr(i)) for i in range(100))
    items = [istr(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        md = md_base.copy()
        for i in items:
            md.pop(i)


def test_multidict_popitem_str(
    benchmark: BenchmarkFixture, any_multidict_class: Type[MultiDict[str]]
) -> None:
    md_base = any_multidict_class((str(i), str(i)) for i in range(100))

    @benchmark
    def _run() -> None:
        md = md_base.copy()
        for _ in range(100):
            md.popitem()


def test_multidict_clear_str(
    benchmark: BenchmarkFixture, any_multidict_class: Type[MultiDict[str]]
) -> None:
    md = any_multidict_class((str(i), str(i)) for i in range(100))

    @benchmark
    def _run() -> None:
        md.clear()


def test_multidict_update_str(
    benchmark: BenchmarkFixture, any_multidict_class: Type[MultiDict[str]]
) -> None:
    md = any_multidict_class((str(i), str(i)) for i in range(100))
    items = {str(i): str(i) for i in range(100, 200)}

    @benchmark
    def _run() -> None:
        md.update(items)


def test_cimultidict_update_istr(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: Type[CIMultiDict[istr]],
) -> None:
    md = case_insensitive_multidict_class((istr(i), istr(i)) for i in range(100))
    items: Dict[Union[str, istr], istr] = {istr(i): istr(i) for i in range(100, 200)}

    @benchmark
    def _run() -> None:
        md.update(items)


def test_multidict_extend_str(
    benchmark: BenchmarkFixture, any_multidict_class: Type[MultiDict[str]]
) -> None:
    base_md = any_multidict_class((str(i), str(i)) for i in range(100))
    items = {str(i): str(i) for i in range(200)}

    @benchmark
    def _run() -> None:
        for j in range(100):
            md = base_md.copy()
            md.extend(items)


def test_cimultidict_extend_istr(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: Type[CIMultiDict[istr]],
) -> None:
    base_md = case_insensitive_multidict_class((istr(i), istr(i)) for i in range(100))
    items = {istr(i): istr(i) for i in range(200)}

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            md.extend(items)


def test_multidict_delitem_str(
    benchmark: BenchmarkFixture, any_multidict_class: Type[MultiDict[str]]
) -> None:
    md_base = any_multidict_class((str(i), str(i)) for i in range(100))
    items = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        md = md_base.copy()
        for i in items:
            del md[i]


def test_cimultidict_delitem_istr(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: Type[CIMultiDict[istr]],
) -> None:
    md_base = case_insensitive_multidict_class((istr(i), istr(i)) for i in range(100))
    items = [istr(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        md = md_base.copy()
        for i in items:
            del md[i]


def test_multidict_getall_str_hit(
    benchmark: BenchmarkFixture, any_multidict_class: Type[MultiDict[str]]
) -> None:
    md = any_multidict_class(("all", str(i)) for i in range(100))

    @benchmark
    def _run() -> None:
        md.getall("all")


def test_multidict_getall_str_miss(
    benchmark: BenchmarkFixture, any_multidict_class: Type[MultiDict[str]]
) -> None:
    md = any_multidict_class(("all", str(i)) for i in range(100))

    @benchmark
    def _run() -> None:
        md.getall("miss", ())


def test_cimultidict_getall_istr_hit(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: Type[CIMultiDict[istr]],
) -> None:
    all_istr = istr("all")
    md = case_insensitive_multidict_class((all_istr, istr(i)) for i in range(100))

    @benchmark
    def _run() -> None:
        md.getall(all_istr)


def test_cimultidict_getall_istr_miss(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: Type[CIMultiDict[istr]],
) -> None:
    all_istr = istr("all")
    miss_istr = istr("miss")
    md = case_insensitive_multidict_class((all_istr, istr(i)) for i in range(100))

    @benchmark
    def _run() -> None:
        md.getall(miss_istr, ())


def test_multidict_fetch(
    benchmark: BenchmarkFixture, any_multidict_class: Type[MultiDict[str]]
) -> None:
    md = any_multidict_class((str(i), str(i)) for i in range(100))
    items = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md[i]


def test_cimultidict_fetch_istr(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: Type[CIMultiDict[istr]],
) -> None:
    md = case_insensitive_multidict_class((istr(i), istr(i)) for i in range(100))
    items = [istr(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md[i]


def test_multidict_get_hit(
    benchmark: BenchmarkFixture, any_multidict_class: Type[MultiDict[str]]
) -> None:
    md = any_multidict_class((str(i), str(i)) for i in range(100))
    items = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md.get(i)


def test_multidict_get_miss(
    benchmark: BenchmarkFixture, any_multidict_class: Type[MultiDict[str]]
) -> None:
    md = any_multidict_class((str(i), str(i)) for i in range(100))
    items = [str(i) for i in range(100, 200)]

    @benchmark
    def _run() -> None:
        for i in items:
            md.get(i)


def test_cimultidict_get_istr_hit(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: Type[CIMultiDict[istr]],
) -> None:
    md = case_insensitive_multidict_class((istr(i), istr(i)) for i in range(100))
    items = [istr(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md.get(i)


def test_cimultidict_get_istr_miss(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: Type[CIMultiDict[istr]],
) -> None:
    md = case_insensitive_multidict_class((istr(i), istr(i)) for i in range(100))
    items = [istr(i) for i in range(100, 200)]

    @benchmark
    def _run() -> None:
        for i in items:
            md.get(i)


def test_multidict_get_hit_with_default(
    benchmark: BenchmarkFixture, any_multidict_class: Type[MultiDict[str]]
) -> None:
    md = any_multidict_class((str(i), str(i)) for i in range(100))
    items = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md.get(i, _SENTINEL)


def test_cimultidict_get_istr_hit_with_default(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: Type[CIMultiDict[istr]],
) -> None:
    md = case_insensitive_multidict_class((istr(i), istr(i)) for i in range(100))
    items = [istr(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md.get(i, _SENTINEL)


def test_cimultidict_get_istr_with_default_miss(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: Type[CIMultiDict[istr]],
) -> None:
    md = case_insensitive_multidict_class((istr(i), istr(i)) for i in range(100))
    items = [istr(i) for i in range(100, 200)]

    @benchmark
    def _run() -> None:
        for i in items:
            md.get(i, _SENTINEL)


def test_multidict_repr(
    benchmark: BenchmarkFixture, any_multidict_class: Type[MultiDict[str]]
) -> None:
    items = [str(i) for i in range(100)]
    md = any_multidict_class([(i, i) for i in items])

    @benchmark
    def _run() -> None:
        repr(md)


def test_create_empty_multidict(
    benchmark: BenchmarkFixture, any_multidict_class: Type[MultiDict[str]]
) -> None:
    @benchmark
    def _run() -> None:
        any_multidict_class()


def test_create_multidict_with_items(
    benchmark: BenchmarkFixture, any_multidict_class: Type[MultiDict[str]]
) -> None:
    items = [(str(i), str(i)) for i in range(100)]

    @benchmark
    def _run() -> None:
        any_multidict_class(items)


def test_create_cimultidict_with_items_istr(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: Type[CIMultiDict[istr]],
) -> None:
    items = [(istr(i), istr(i)) for i in range(100)]

    @benchmark
    def _run() -> None:
        case_insensitive_multidict_class(items)


def test_create_empty_multidictproxy(
    benchmark: BenchmarkFixture, any_multidict_class: Type[MultiDict[str]]
) -> None:
    md = any_multidict_class()

    @benchmark
    def _run() -> None:
        MultiDictProxy(md)


def test_create_multidictproxy(
    benchmark: BenchmarkFixture, any_multidict_class: Type[MultiDict[str]]
) -> None:
    items = [(str(i), str(i)) for i in range(100)]
    md = any_multidict_class(items)

    @benchmark
    def _run() -> None:
        MultiDictProxy(md)


def test_create_empty_CIMultiDictProxy(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: Type[CIMultiDict[istr]],
) -> None:
    md = case_insensitive_multidict_class()

    @benchmark
    def _run() -> None:
        CIMultiDictProxy(md)


def test_create_cimultidictproxy(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: Type[CIMultiDict[istr]],
) -> None:
    items = [(istr(i), istr(i)) for i in range(100)]
    md = case_insensitive_multidict_class(items)

    @benchmark
    def _run() -> None:
        CIMultiDictProxy(md)


def test_create_from_existing_cimultidict(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: Type[CIMultiDict[istr]],
) -> None:
    existing = case_insensitive_multidict_class((istr(i), istr(i)) for i in range(5))

    @benchmark
    def _run() -> None:
        case_insensitive_multidict_class(existing)


def test_copy_from_existing_cimultidict(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: Type[CIMultiDict[istr]],
) -> None:
    existing = case_insensitive_multidict_class((istr(i), istr(i)) for i in range(5))

    @benchmark
    def _run() -> None:
        existing.copy()
