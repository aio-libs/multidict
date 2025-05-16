from collections.abc import Mapping, MutableMapping

from multidict import (
    MultiDict,
    MultiDictProxy,
    MultiMapping,
    MutableMultiMapping,
)


def test_abc_inheritance() -> None:
    assert issubclass(MultiMapping, Mapping)
    assert not issubclass(MultiMapping, MutableMapping)
    assert issubclass(MutableMultiMapping, Mapping)
    assert issubclass(MutableMultiMapping, MutableMapping)


def test_multidict_inheritance(any_md_class: type[MultiDict[str]]) -> None:
    assert issubclass(any_md_class, MultiMapping)
    assert issubclass(any_md_class, MutableMultiMapping)


def test_proxy_inheritance(
    any_md_proxy_class: type[MultiDictProxy[str]],
) -> None:
    assert issubclass(any_md_proxy_class, MultiMapping)
    assert not issubclass(any_md_proxy_class, MutableMultiMapping)


def test_generic_type_in_runtime() -> None:
    MultiMapping[str]
    MutableMultiMapping[str]
