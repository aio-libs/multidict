import os
import pickle
from pathlib import Path

import pytest

WRITE_PICKLE_FILES = bool(os.environ.get("WRITE_PICKLE_FILES"))


def test_unpickle(any_multidict_class, dict_data, pickled_data):
    expected = any_multidict_class(dict_data)
    actual = pickle.loads(pickled_data)
    assert actual == expected
    assert isinstance(actual, any_multidict_class)


def test_pickle_proxy(any_multidict_class, any_multidict_proxy_class, dict_data):
    d = any_multidict_class(dict_data)
    proxy = any_multidict_proxy_class(d)
    with pytest.raises(TypeError):
        pickle.dumps(proxy)


def test_pickle_format_stability(pickled_data, pickle_file_path, pickle_protocol):
    if pickle_protocol == 0:
        # TODO: consider updating pickle files
        pytest.skip(reason="Format for pickle protocol 0 is changed, it's a known fact")
    expected = pickle_file_path.read_bytes()
    assert pickled_data == expected


def test_pickle_backward_compatibility(
    any_multidict_class,
    dict_data,
    pickle_file_path,
):
    expected = any_multidict_class(dict_data)
    with pickle_file_path.open("rb") as f:
        actual = pickle.load(f)

    assert actual == expected
    assert isinstance(actual, any_multidict_class)


@pytest.mark.skipif(
    not WRITE_PICKLE_FILES,
    reason="This is a helper that writes pickle test files",
)
def test_write_pickle_file(pickled_data: bytes, pickle_file_path: Path) -> None:
    pickle_file_path.write_bytes(pickled_data)
