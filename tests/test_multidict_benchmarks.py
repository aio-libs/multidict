"""codspeed benchmarks for multidict."""

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

_HEADER_NAMES = (
    "Accept",
    "Accept-Encoding",
    "Accept-Language",
    "Cache-Control",
    "Connection",
    "Content-Length",
    "Content-Type",
    "Cookie",
    "Host",
    "If-None-Match",
    "Origin",
    "Referer",
    "Sec-Fetch-Mode",
    "Server",
    "Set-Cookie",
    "Transfer-Encoding",
    "Upgrade",
    "User-Agent",
    "X-Forwarded-For",
    "X-Request-ID",
)

# A plain ``str`` key makes CIMultiDict compute the identity on every call,
# the step ``istr`` skips via its cached canonical form.  The two spellings
# are kept separate because lowering an already-lowercase key and lowering a
# mixed-case one are different amounts of work.
_MIXED_CASE_KEYS = [f"{name}-{i}" for i in range(10) for name in _HEADER_NAMES]
_LOWER_CASE_KEYS = [key.lower() for key in _MIXED_CASE_KEYS]
_NON_ASCII_KEYS = [f"Ключ-{i}" for i in range(200)]


def test_multidict_insert_str(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    base_md = any_multidict_class()
    items = [str(i) for i in range(200)]

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            for i in items:
                md[i] = i


def test_cimultidict_insert_istr(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    base_md = case_insensitive_multidict_class()
    items = [case_insensitive_str_class(i) for i in range(200)]

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            for i in items:
                md[i] = i


def test_multidict_add_str(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    base_md = any_multidict_class()
    items = [str(i) for i in range(200)]

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            for i in items:
                md.add(i, i)


def test_cimultidict_add_istr(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    base_md = case_insensitive_multidict_class()
    items = [case_insensitive_str_class(i) for i in range(200)]

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            for i in items:
                md.add(i, i)


def test_multidict_add_same_key(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    base_md = any_multidict_class()
    values = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            for v in values:
                md.add("key", v)


def test_multidict_pop_str(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    md_base = any_multidict_class((str(i), str(i)) for i in range(400))
    items = [str(i) for i in range(100, 300)]

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = md_base.copy()
            for i in items:
                md.pop(i)


def test_cimultidict_pop_istr(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    md_base = case_insensitive_multidict_class(
        (case_insensitive_str_class(i), case_insensitive_str_class(i))
        for i in range(400)
    )
    items = [case_insensitive_str_class(i) for i in range(100, 300)]

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = md_base.copy()
            for i in items:
                md.pop(i)


def test_multidict_popitem_str(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    md_base = any_multidict_class((str(i), str(i)) for i in range(200))

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = md_base.copy()
            for _ in range(200):
                md.popitem()


def test_multidict_clear_str(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    md_base = any_multidict_class((str(i), str(i)) for i in range(100))

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = md_base.copy()
            md.clear()


def test_multidict_update_str(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    base_md = any_multidict_class((str(i), str(i)) for i in range(150))
    items = {str(i): str(i) for i in range(100, 200)}

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            md.update(items)


def test_cimultidict_update_istr(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    base_md = case_insensitive_multidict_class(
        (case_insensitive_str_class(i), case_insensitive_str_class(i))
        for i in range(150)
    )
    items: dict[str | istr, istr] = {
        case_insensitive_str_class(i): case_insensitive_str_class(i)
        for i in range(100, 200)
    }

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            md.update(items)


def test_multidict_update_str_with_kwargs(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    base_md = any_multidict_class((str(i), str(i)) for i in range(150))
    items = {str(i): str(i) for i in range(100, 200)}
    kwargs = {str(i): str(i) for i in range(200, 300)}

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            md.update(items, **kwargs)


def test_cimultidict_update_istr_with_kwargs(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    base_md = case_insensitive_multidict_class(
        (case_insensitive_str_class(i), case_insensitive_str_class(i))
        for i in range(150)
    )
    items: dict[str | istr, istr] = {
        case_insensitive_str_class(i): case_insensitive_str_class(i)
        for i in range(100, 200)
    }
    kwargs = {str(i): case_insensitive_str_class(i) for i in range(200, 300)}

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            md.update(items, **kwargs)


def test_multidict_extend_str(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    base_md = any_multidict_class((str(i), str(i)) for i in range(100))
    items = {str(i): str(i) for i in range(200)}

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            md.extend(items)


def test_cimultidict_extend_istr(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    base_md = case_insensitive_multidict_class(
        (case_insensitive_str_class(i), case_insensitive_str_class(i))
        for i in range(100)
    )
    items = {
        case_insensitive_str_class(i): case_insensitive_str_class(i) for i in range(200)
    }

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            md.extend(items)


def test_multidict_extend_str_with_kwargs(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    base_md = any_multidict_class((str(i), str(i)) for i in range(100))
    items = {str(i): str(i) for i in range(200)}
    kwargs = {str(i): str(i) for i in range(200, 300)}

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            md.extend(items, **kwargs)


def test_cimultidict_extend_istr_with_kwargs(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    base_md = case_insensitive_multidict_class(
        (case_insensitive_str_class(i), case_insensitive_str_class(i))
        for i in range(100)
    )
    items = {
        case_insensitive_str_class(i): case_insensitive_str_class(i) for i in range(200)
    }
    kwargs = {str(i): case_insensitive_str_class(i) for i in range(200, 300)}

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            md.extend(items, **kwargs)


def test_multidict_delitem_str(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    md_base = any_multidict_class((str(i), str(i)) for i in range(200))
    items = [str(i) for i in range(200)]

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = md_base.copy()
            for i in items:
                del md[i]


def test_cimultidict_delitem_istr(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    md_base = case_insensitive_multidict_class(
        (case_insensitive_str_class(i), case_insensitive_str_class(i))
        for i in range(200)
    )
    items = [case_insensitive_str_class(i) for i in range(200)]

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = md_base.copy()
            for i in items:
                del md[i]


def test_multidict_setitem_str(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    md_base = any_multidict_class((str(i), str(i)) for i in range(100))
    items = [(str(i), str(i) + " new") for i in range(100)]

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = md_base.copy()
            for key, val in items:
                md[key] = val


def test_cimultidict_setitem_istr(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    md_base = case_insensitive_multidict_class(
        (case_insensitive_str_class(i), case_insensitive_str_class(i))
        for i in range(100)
    )
    items = [
        (case_insensitive_str_class(i), case_insensitive_str_class(str(i) + " new"))
        for i in range(100)
    ]

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = md_base.copy()
            for key, val in items:
                md[key] = val


def test_multidict_getall_str_hit(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    md = any_multidict_class(
        (f"key{j}", str(f"{i}-{j}")) for i in range(8) for j in range(128)
    )

    keys = [f"key{j}" for j in range(128)]

    @benchmark
    def _run() -> None:
        for i in range(8):
            for key in keys:
                md.getall(key)


def test_multidict_getall_str_hit_nonascii(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    md = any_multidict_class(
        (f"ключ{j}", str(f"{i}-{j}")) for i in range(8) for j in range(128)
    )

    keys = [f"ключ{j}" for j in range(128)]

    @benchmark
    def _run() -> None:
        for i in range(8):
            for key in keys:
                md.getall(key)


def test_multidict_getall_str_miss(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    md = any_multidict_class(
        (f"key{j}", str(f"{i}-{j}")) for i in range(8) for j in range(128)
    )

    key = "key-miss"

    @benchmark
    def _run() -> None:
        for i in range(1024):
            md.getall(key, ())


def test_cimultidict_getall_istr_hit(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    md = case_insensitive_multidict_class(
        (f"key{j}", case_insensitive_str_class(f"{i}-{j}"))
        for i in range(8)
        for j in range(128)
    )

    keys = [case_insensitive_str_class(f"key{j}") for j in range(128)]

    @benchmark
    def _run() -> None:
        for i in range(8):
            for key in keys:
                md.getall(key)


def test_cimultidict_getall_istr_hit_nonascii(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    md = case_insensitive_multidict_class(
        (f"ключ{j}", case_insensitive_str_class(f"{i}-{j}"))
        for i in range(8)
        for j in range(128)
    )

    keys = [case_insensitive_str_class(f"ключ{j}") for j in range(128)]

    @benchmark
    def _run() -> None:
        for i in range(8):
            for key in keys:
                md.getall(key)


def test_cimultidict_getall_istr_miss(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    md = case_insensitive_multidict_class(
        (case_insensitive_str_class(f"key{j}"), case_insensitive_str_class(f"{i}-{j}"))
        for i in range(8)
        for j in range(128)
    )

    key = case_insensitive_str_class("key-miss")

    @benchmark
    def _run() -> None:
        for i in range(1024):
            md.getall(key, ())


def test_multidict_fetch(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    md = any_multidict_class((str(i), str(i)) for i in range(100))
    items = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md[i]


def test_cimultidict_fetch_istr(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    md = case_insensitive_multidict_class(
        (case_insensitive_str_class(i), case_insensitive_str_class(i))
        for i in range(100)
    )
    items = [case_insensitive_str_class(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md[i]


def test_multidict_get_hit(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    md = any_multidict_class((str(i), str(i)) for i in range(100))
    items = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md.get(i)


def test_multidict_get_miss(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    md = any_multidict_class((str(i), str(i)) for i in range(100))
    items = [str(i) for i in range(100, 200)]

    @benchmark
    def _run() -> None:
        for i in items:
            md.get(i)


def test_cimultidict_get_istr_hit(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    md = case_insensitive_multidict_class(
        (case_insensitive_str_class(i), case_insensitive_str_class(i))
        for i in range(100)
    )
    items = [case_insensitive_str_class(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md.get(i)


def test_cimultidict_get_istr_miss(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    md = case_insensitive_multidict_class(
        (case_insensitive_str_class(i), case_insensitive_str_class(i))
        for i in range(100)
    )
    items = [case_insensitive_str_class(i) for i in range(100, 200)]

    @benchmark
    def _run() -> None:
        for i in items:
            md.get(i)


def test_multidict_get_hit_with_default(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    md = any_multidict_class((str(i), str(i)) for i in range(100))
    items = [str(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md.get(i, _SENTINEL)


def test_cimultidict_get_istr_hit_with_default(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    md = case_insensitive_multidict_class(
        (case_insensitive_str_class(i), case_insensitive_str_class(i))
        for i in range(100)
    )
    items = [case_insensitive_str_class(i) for i in range(100)]

    @benchmark
    def _run() -> None:
        for i in items:
            md.get(i, _SENTINEL)


def test_cimultidict_get_istr_with_default_miss(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    md = case_insensitive_multidict_class(
        (case_insensitive_str_class(i), case_insensitive_str_class(i))
        for i in range(100)
    )
    items = [case_insensitive_str_class(i) for i in range(100, 200)]

    @benchmark
    def _run() -> None:
        for i in items:
            md.get(i, _SENTINEL)


def test_multidict_repr(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    items = [str(i) for i in range(100)]
    md = any_multidict_class([(i, i) for i in items])

    @benchmark
    def _run() -> None:
        for _ in range(100):
            repr(md)


def test_create_empty_multidict(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    @benchmark
    def _run() -> None:
        any_multidict_class()


def test_create_multidict_with_items(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    items = [(str(i), str(i)) for i in range(100)]

    @benchmark
    def _run() -> None:
        any_multidict_class(items)


def test_create_multidict_with_items_same_key(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    items = [("key", str(i)) for i in range(1000)]

    @benchmark
    def _run() -> None:
        any_multidict_class(items)


def test_create_multidict_with_many_items(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    items = [(str(i), str(i)) for i in range(5000)]

    @benchmark
    def _run() -> None:
        any_multidict_class(items)


def test_create_cimultidict_with_items_istr(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    items = [
        (case_insensitive_str_class(i), case_insensitive_str_class(i))
        for i in range(100)
    ]

    @benchmark
    def _run() -> None:
        case_insensitive_multidict_class(items)


def test_create_multidict_with_dict(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    dct = {str(i): str(i) for i in range(100)}

    @benchmark
    def _run() -> None:
        any_multidict_class(dct)


def test_create_cimultidict_with_dict_istr(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    dct = {
        case_insensitive_str_class(i): case_insensitive_str_class(i) for i in range(100)
    }

    @benchmark
    def _run() -> None:
        case_insensitive_multidict_class(dct)


def test_create_multidict_with_items_with_kwargs(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    items = [(str(i), str(i)) for i in range(100)]
    kwargs = {str(i): str(i) for i in range(100)}

    @benchmark
    def _run() -> None:
        any_multidict_class(items, **kwargs)


def test_create_cimultidict_with_items_istr_with_kwargs(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    items = [
        (case_insensitive_str_class(i), case_insensitive_str_class(i))
        for i in range(100)
    ]
    kwargs = {str(i): case_insensitive_str_class(i) for i in range(100)}

    @benchmark
    def _run() -> None:
        case_insensitive_multidict_class(items, **kwargs)


def test_create_empty_multidictproxy(benchmark: BenchmarkFixture) -> None:
    md: MultiDict[str] = MultiDict()

    @benchmark
    def _run() -> None:
        MultiDictProxy(md)


def test_create_multidictproxy(benchmark: BenchmarkFixture) -> None:
    items = [(str(i), str(i)) for i in range(100)]
    md: MultiDict[str] = MultiDict(items)

    @benchmark
    def _run() -> None:
        MultiDictProxy(md)


def test_create_empty_cimultidictproxy(
    benchmark: BenchmarkFixture,
) -> None:
    md: CIMultiDict[istr] = CIMultiDict()

    @benchmark
    def _run() -> None:
        CIMultiDictProxy(md)


def test_create_cimultidictproxy(
    benchmark: BenchmarkFixture,
    case_insensitive_str_class: type[istr],
) -> None:
    items = [
        (case_insensitive_str_class(i), case_insensitive_str_class(i))
        for i in range(100)
    ]
    md = CIMultiDict(items)

    @benchmark
    def _run() -> None:
        CIMultiDictProxy(md)


def test_create_from_existing_cimultidict(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    existing = case_insensitive_multidict_class(
        (case_insensitive_str_class(i), case_insensitive_str_class(i)) for i in range(5)
    )

    @benchmark
    def _run() -> None:
        case_insensitive_multidict_class(existing)


def test_copy_from_existing_cimultidict(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    existing = case_insensitive_multidict_class(
        (case_insensitive_str_class(i), case_insensitive_str_class(i)) for i in range(5)
    )

    @benchmark
    def _run() -> None:
        existing.copy()


def test_iterate_multidict(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    items = [(str(i), str(i)) for i in range(100)]
    md = any_multidict_class(items)

    @benchmark
    def _run() -> None:
        for _ in md:
            pass


def test_iterate_multidict_keys(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    items = [(str(i), str(i)) for i in range(100)]
    md = any_multidict_class(items)

    @benchmark
    def _run() -> None:
        for _ in md.keys():
            pass


def test_iterate_multidict_values(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    items = [(str(i), str(i)) for i in range(100)]
    md = any_multidict_class(items)

    @benchmark
    def _run() -> None:
        for _ in md.values():
            pass


def test_iterate_multidict_items(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    items = [(str(i), str(i)) for i in range(100)]
    md = any_multidict_class(items)

    @benchmark
    def _run() -> None:
        for _, _ in md.items():
            pass


def test_multidict_getall_str_hit_large_table(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    md = any_multidict_class(
        (f"key{j}", f"{i}-{j}") for i in range(8) for j in range(8192)
    )

    keys = [f"key{j}" for j in range(128)]

    @benchmark
    def _run() -> None:
        for key in keys:
            md.getall(key)


def test_multidict_getall_str_hit_large_table_many_values(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    md = any_multidict_class(
        (f"key{j}", f"{i}-{j}") for i in range(16) for j in range(4096)
    )

    keys = [f"key{j}" for j in range(128)]

    @benchmark
    def _run() -> None:
        for key in keys:
            md.getall(key)


def test_multidict_getall_str_hit_many_values(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    md = any_multidict_class(
        (f"key{j}", f"{i}-{j}") for i in range(64) for j in range(16)
    )

    keys = [f"key{j}" for j in range(16)]

    @benchmark
    def _run() -> None:
        for i in range(8):
            for key in keys:
                md.getall(key)


def test_cimultidict_getall_istr_hit_many_values(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    md = case_insensitive_multidict_class(
        (f"key{j}", case_insensitive_str_class(f"{i}-{j}"))
        for i in range(64)
        for j in range(16)
    )

    keys = [case_insensitive_str_class(f"key{j}") for j in range(16)]

    @benchmark
    def _run() -> None:
        for i in range(8):
            for key in keys:
                md.getall(key)


def test_multidict_update_str_with_duplicates(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    base_md = any_multidict_class((str(i % 50), str(i)) for i in range(150))
    items = [(str(i % 75), str(i)) for i in range(100)]

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            md.update(items)


def test_cimultidict_update_istr_with_duplicates(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[istr]],
    case_insensitive_str_class: type[istr],
) -> None:
    base_md = case_insensitive_multidict_class(
        (case_insensitive_str_class(i % 50), case_insensitive_str_class(i))
        for i in range(150)
    )
    items = [
        (case_insensitive_str_class(i % 75), case_insensitive_str_class(i))
        for i in range(100)
    ]

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            md.update(items)


def test_multidict_merge_str(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    base_md = any_multidict_class((str(i), str(i)) for i in range(150))
    items = {str(i): str(i) for i in range(100, 200)}

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            md.merge(items)


def test_multidict_to_dict_str(
    benchmark: BenchmarkFixture, any_multidict_class: type[MultiDict[str]]
) -> None:
    md = any_multidict_class((str(i % 50), str(i)) for i in range(150))

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md.to_dict()


def test_cimultidict_insert_str_mixed_case(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[str]],
) -> None:
    base_md = case_insensitive_multidict_class()

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            for i in _MIXED_CASE_KEYS:
                md[i] = i


def test_cimultidict_insert_str_lower_case(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[str]],
) -> None:
    base_md = case_insensitive_multidict_class()

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            for i in _LOWER_CASE_KEYS:
                md[i] = i


def test_cimultidict_add_str_mixed_case(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[str]],
) -> None:
    base_md = case_insensitive_multidict_class()

    @benchmark
    def _run() -> None:
        for _ in range(100):
            md = base_md.copy()
            for i in _MIXED_CASE_KEYS:
                md.add(i, i)


def test_cimultidict_fetch_str_mixed_case(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[str]],
) -> None:
    md = case_insensitive_multidict_class((i, i) for i in _MIXED_CASE_KEYS)

    @benchmark
    def _run() -> None:
        for _ in range(8):
            for i in _MIXED_CASE_KEYS:
                md[i]


def test_cimultidict_fetch_str_lower_case(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[str]],
) -> None:
    md = case_insensitive_multidict_class((i, i) for i in _LOWER_CASE_KEYS)

    @benchmark
    def _run() -> None:
        for _ in range(8):
            for i in _LOWER_CASE_KEYS:
                md[i]


def test_cimultidict_fetch_str_non_ascii(
    benchmark: BenchmarkFixture,
    case_insensitive_multidict_class: type[CIMultiDict[str]],
) -> None:
    md = case_insensitive_multidict_class((i, i) for i in _NON_ASCII_KEYS)

    @benchmark
    def _run() -> None:
        for _ in range(8):
            for i in _NON_ASCII_KEYS:
                md[i]
