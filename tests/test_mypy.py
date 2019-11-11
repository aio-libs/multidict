import multidict


def test_classes_not_abstract() -> None:
    d1 = multidict.MultiDict({"a": "b"})  # type: multidict.MultiDict[str]
    d2 = multidict.CIMultiDict({"a": "b"})  # type: multidict.CIMultiDict[str]

    d3 = multidict.MultiDictProxy(d1)
    d4 = multidict.CIMultiDictProxy(d2)

    d1.getone("a")
    d2.getall("a")
    d3.getone("a")
    d4.getall("a")
