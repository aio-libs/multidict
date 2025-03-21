import sys
from collections.abc import (
    Container,
    ItemsView,
    Iterable,
    KeysView,
    Sequence,
    Set,
)
from typing import Literal, Union

if sys.version_info >= (3, 10):
    from types import NotImplementedType
else:
    from typing import Any as NotImplementedType

if sys.version_info >= (3, 11):
    from typing import assert_never
else:
    from typing_extensions import assert_never


_ViewArg = Union[KeysView[str], ItemsView[str, object]]


def _viewbaseset_and(
    view: _ViewArg, other: object
) -> Union[set[Sequence[object]], NotImplementedType]:
    if not isinstance(other, Iterable):
        return NotImplemented  # type: ignore[no-any-return]
    lft = set(iter(view))
    rgt = set(iter(other))
    return lft & rgt


def _viewbaseset_or(
    view: _ViewArg, other: object
) -> Union[set[Sequence[object]], NotImplementedType]:
    if not isinstance(other, Iterable):
        return NotImplemented  # type: ignore[no-any-return]
    lft = set(iter(view))
    rgt = set(iter(other))
    return lft | rgt


def _viewbaseset_sub(
    view: _ViewArg, other: object
) -> Union[set[Sequence[object]], NotImplementedType]:
    if not isinstance(other, Iterable):
        return NotImplemented  # type: ignore[no-any-return]
    lft = set(iter(view))
    rgt = set(iter(other))
    return lft - rgt


def _viewbaseset_xor(
    view: _ViewArg, other: object
) -> Union[set[Sequence[object]], NotImplementedType]:
    if not isinstance(other, Iterable):
        return NotImplemented  # type: ignore[no-any-return]
    lft = set(iter(view))
    rgt = set(iter(other))
    return lft ^ rgt


def _itemsview_isdisjoint(view: Container[object], other: Iterable[object]) -> bool:
    "Return True if two sets have a null intersection."
    for v in other:
        if v in view:
            return False
    return True


def _keysview_isdisjoint(view: Container[object], other: Iterable[object]) -> bool:
    "Return True if two sets have a null intersection."
    for k in other:
        if k in view:
            return False
    return True
