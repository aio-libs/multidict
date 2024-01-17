import pickle
from pathlib import Path

import pytest

here = Path(__file__).resolve().parent


def test_pickle(any_multidict_class, pickle_protocol):
    d = any_multidict_class([("a", 1), ("a", 2)])
    pbytes = pickle.dumps(d, pickle_protocol)
    obj = pickle.loads(pbytes)
    assert d == obj
    assert isinstance(obj, any_multidict_class)


def test_pickle_proxy(any_multidict_class, any_multidict_proxy_class):
    d = any_multidict_class([("a", 1), ("a", 2)])
    proxy = any_multidict_proxy_class(d)
    with pytest.raises(TypeError):
        pickle.dumps(proxy)


def test_load_from_file(any_multidict_class, multidict_implementation, in_memory_pickle_classes, pickle_protocol):
    multidict_class_name = any_multidict_class.__name__
    pickle_file_basename = "-".join(
        (
            multidict_class_name.lower(),
            multidict_implementation.tag,
        )
    )
    d = any_multidict_class([("a", 1), ("a", 2)])
    fname = f"{pickle_file_basename}.pickle.{pickle_protocol}"
    obj = pickle.loads(in_memory_pickle_classes[fname])
    assert d == obj
    assert isinstance(obj, any_multidict_class)
