import abc
from collections.abc import Iterable, Mapping, MutableMapping
from typing import Protocol, TypeVar, Union, overload

_V = TypeVar("_V")
_V_co = TypeVar("_V_co", covariant=True)
_T = TypeVar("_T")


class SupportsKeysAndGetItem(Protocol[_V_co]):
    def keys(self) -> Iterable[str]: ...
    def __getitem__(self, key: str, /) -> _V_co: ...


MDArg = Union[SupportsKeysAndGetItem[_V], Iterable[tuple[str, _V]], None]


class MultiMapping(Mapping[str, _V_co]):
    @overload
    def getall(self, key: str) -> list[_V_co]:
        ...
    @overload
    def getall(self, key: str, default: _T) -> Union[list[_V_co], _T]:
        ...
    @abc.abstractmethod
    def getall(self, key: str, default: _T = ...) -> Union[list[_V_co], _T]:
        """Return all values for key."""

    @overload
    def getone(self, key: str) -> _V_co:
        ...
    @overload
    def getone(self, key: str, default: _T) -> Union[_V_co, _T]:
        ...
    @abc.abstractmethod
    def getone(self, key: str, default: _T = ...) -> Union[_V_co, _T]:
        """Return first value for key."""


class MutableMultiMapping(MultiMapping[_V], MutableMapping[str, _V]):
    @abc.abstractmethod
    def add(self, key: str, value: _V) -> None:
        """Add value to list."""

    @abc.abstractmethod
    def extend(self, arg: MDArg[_V] = None, /, **kwargs: _V) -> None:
        """Add everything from arg and kwargs to the mapping."""

    @overload
    def popone(self, key: str) -> _V:
        ...
    @overload
    def popone(self, key: str, default: _T) -> Union[_V, _T]:
        ...
    @abc.abstractmethod
    def popone(self, key: str, default: _T = ...) -> Union[_V, _T]:
        """Remove specified key and return the corresponding value."""

    @overload
    def popall(self, key: str) -> list[_V]:
        ...
    @overload
    def popall(self, key: str, default: _T) -> Union[list[_V], _T]:
        ...
    @abc.abstractmethod
    def popall(self, key: str, default: _T = ...) -> Union[list[_V], _T]:
        """Remove all occurrences of key and return the list of corresponding values."""
