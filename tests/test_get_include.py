import os

import multidict


def test_get_include_returns_directory_with_capi_headers() -> None:
    include_dir = multidict.get_include()
    assert os.path.isdir(include_dir)
    assert os.path.isfile(os.path.join(include_dir, "multidict_capi.h"))
    assert os.path.isfile(os.path.join(include_dir, "multidict_capi_struct.h"))
