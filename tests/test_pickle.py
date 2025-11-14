import pickle
from pathlib import Path
import pytest

from multidict import MultiDict, MultiDictProxy, istr

here = Path(__file__).resolve().parent


def test_pickle(
    any_multidict_class: type[MultiDict[int]], pickle_protocol: int
) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    pbytes = pickle.dumps(d, pickle_protocol)
    obj = pickle.loads(pbytes)
    assert d == obj
    assert isinstance(obj, any_multidict_class)


def test_pickle_proxy(
    any_multidict_class: type[MultiDict[int]],
    any_multidict_proxy_class: type[MultiDictProxy[int]],
) -> None:
    d = any_multidict_class([("a", 1), ("a", 2)])
    proxy = any_multidict_proxy_class(d)
    with pytest.raises(TypeError):
        pickle.dumps(proxy)


def test_pickle_istr(
    case_insensitive_str_class: type[istr], pickle_protocol: int
) -> None:
    s = case_insensitive_str_class("str")
    pbytes = pickle.dumps(s, pickle_protocol)
    obj = pickle.loads(pbytes)
    assert s == obj
    assert isinstance(obj, case_insensitive_str_class)


def test_load_from_file(
    # any_multidict_class: type[MultiDict[int]],
    # multidict_implementation: "MultidictImplementation",
    # pickle_protocol: int,
    pickle_protocol_multidict: tuple[bytes, type[MultiDict[int]]],
) -> None:
    buf, any_multidict_class = pickle_protocol_multidict
    d = any_multidict_class([("a", 1), ("a", 2)])
    obj = pickle.loads(buf)
    assert d == obj
    assert isinstance(obj, any_multidict_class)


def test_load_istr_from_file(pickle_protocol_istr: tuple[bytes, type[istr]]) -> None:
    buf, case_incasive_str = pickle_protocol_istr
    d = case_incasive_str("str")
    obj = pickle.loads(buf)
    assert d == obj
    assert isinstance(obj, case_incasive_str)
