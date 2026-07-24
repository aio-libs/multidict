import gc
import sys
import sysconfig
from collections.abc import Callable

from multidict import (
    CIMultiDict,
    CIMultiDictProxy,
    MultiDict,
    MultiDictProxy,
    istr,
)

# sys.getrefcount is not meaningful under the free-threaded build:
# refcounts are biased per-thread and types may be immortalized, so
# the simple baseline/after comparison below does not apply.
FREETHREADED = bool(sysconfig.get_config_var("Py_GIL_DISABLED"))


if __name__ == "__main__":
    if FREETHREADED:
        raise SystemExit(0)

    md = MultiDict([("a", "1"), ("b", "2")])

    # Iterator type leak
    iter_type = type(iter(md.keys()))
    gc.collect()
    baseline = sys.getrefcount(iter_type)
    for _ in range(1000):
        _it = iter(md.keys())
        list(_it)
        del _it
    gc.collect()
    after = sys.getrefcount(iter_type)
    assert after == baseline, f"iterator type leaked: {after - baseline}"

    # View type leak
    view_type = type(md.keys())
    gc.collect()
    baseline = sys.getrefcount(view_type)
    for _ in range(1000):
        _v = md.keys()
        del _v
    gc.collect()
    after = sys.getrefcount(view_type)
    assert after == baseline, f"view type leaked: {after - baseline}"

    # istr type leak
    gc.collect()
    baseline = sys.getrefcount(istr)
    for _ in range(1000):
        _s = istr("hello")
        del _s
    gc.collect()
    after = sys.getrefcount(istr)
    assert after == baseline, f"istr type leaked: {after - baseline}"

    # Container and proxy type leaks.  Heap types keep a reference to their
    # type from every instance; tp_dealloc must release it.  The iterator,
    # view and istr types above were fixed previously, but the four core
    # container/proxy types were missed.
    core_type_factories: dict[str, Callable[[], object]] = {
        "MultiDict": lambda: MultiDict(),
        "CIMultiDict": lambda: CIMultiDict(),
        "MultiDictProxy": lambda: MultiDictProxy(MultiDict()),
        "CIMultiDictProxy": lambda: CIMultiDictProxy(CIMultiDict()),
    }
    for name, make in core_type_factories.items():
        obj_type = type(make())
        gc.collect()
        baseline = sys.getrefcount(obj_type)
        for _ in range(1000):
            _obj = make()
            del _obj
        gc.collect()
        after = sys.getrefcount(obj_type)
        assert after == baseline, f"{name} type leaked: {after - baseline}"
