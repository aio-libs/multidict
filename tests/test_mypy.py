import multidict


def test_classes_not_abstract() -> None:
    d1 = multidict.MultiDict({'a': 'b'})
    d2 = multidict.CIMultiDict({'a': 'b'})

    d3 = multidict.MultiDictProxy(d1)
    d4 = multidict.CIMultiDictProxy(d2)

    d3.getone('a')
    d4.getall('a')
