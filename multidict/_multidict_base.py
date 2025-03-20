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


def _viewbaseset_richcmp(
    view: _ViewArg, other: object, op: Literal[0, 1, 2, 3, 4, 5]
) -> Union[bool, NotImplementedType]:
    if not isinstance(other, Set):
        return NotImplemented  # type: ignore[no-any-return]
    if op == 0:  # <
        return len(view) < len(other) and view <= other
    elif op == 1:  # <=
        if len(view) > len(other):
            return False
        for elem in view:
            if elem not in other:
                return False
        return True
    elif op == 2:  # ==
        return len(view) == len(other) and view <= other
    elif op == 3:  # !=
        return not view == other
    elif op == 4:  # >
        return len(view) > len(other) and view >= other
    elif op == 5:  # >=
        if len(view) < len(other):
            return False
        for elem in other:
            if elem not in view:
                return False
        return True
    else:  # pragma: no cover
        assert_never(op)


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
