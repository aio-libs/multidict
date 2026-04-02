from multidict import MultiDict, istr
import sys

md = MultiDict([("a", "1"), ("b", "2")])

if __name__ == "__main__":
    # XXX: Code Coverage misbehaves so do this instead
    it = iter(md.keys())
    iter_type = type(it)
    del it
    baseline = sys.getrefcount(iter_type)
    for i in range(1000):
        it = iter(md.keys())
        list(it)
        del it
    after = sys.getrefcount(iter_type)

    # NOTE: On Freethreaded Mode this value can be negative
    assert (after - baseline) <= 0, "iterator type leaked"

    # Test view type leak
    view_type = type(md.keys())
    baseline = sys.getrefcount(view_type)
    for i in range(1000):
        v = md.keys()
        del v
    after = sys.getrefcount(view_type)
    assert (after - baseline) <= 0, "view type leaked"

    baseline = sys.getrefcount(istr)
    for i in range(1000):
        s = istr("hello")
        del s
    after = sys.getrefcount(istr)
    assert (after - baseline) <= 0, "istr type leaked"
