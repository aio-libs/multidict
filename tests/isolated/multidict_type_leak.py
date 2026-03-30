from multidict import MultiDict, istr
import sys

md = MultiDict([("a", "1"), ("b", "2")])


# Test iterator type leak
def test_iterator_type_leak() -> bool:
    it = iter(md.keys())
    iter_type = type(it)
    del it
    baseline = sys.getrefcount(iter_type)
    for i in range(1000):
        it = iter(md.keys())
        list(it)
        del it
    after = sys.getrefcount(iter_type)
    return after - baseline == 0


def test_view_type_leak() -> bool:
    # Test view type leak
    view_type = type(md.keys())
    baseline = sys.getrefcount(view_type)
    for i in range(1000):
        v = md.keys()
        del v
    after = sys.getrefcount(view_type)
    return after - baseline == 0


# Test istr type leak
def test_istr_type_leak() -> bool:
    baseline = sys.getrefcount(istr)
    for i in range(1000):
        s = istr("hello")
        del s
    after = sys.getrefcount(istr)
    return after - baseline == 0


def main() -> None:
    assert test_iterator_type_leak(), "iterator type leaked"
    assert test_view_type_leak(), "view type leaked"
    assert test_istr_type_leak(), "istr type leaked"


if __name__ == "__main__":
    main()
