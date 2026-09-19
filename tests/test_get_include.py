from pathlib import Path

import multidict


def test_get_include_returns_directory_with_capi_headers() -> None:
    include_dir = Path(multidict.get_include())
    assert include_dir.is_dir()
    assert (include_dir / "multidict_capi.h").is_file()
    assert (include_dir / "multidict_capi_struct.h").is_file()
