import sys
from collections.abc import (
    ItemsView,
    Iterable,
    KeysView,
    Sequence,
)
from typing import Union

if sys.version_info >= (3, 10):
    from types import NotImplementedType
else:
    from typing import Any as NotImplementedType


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
