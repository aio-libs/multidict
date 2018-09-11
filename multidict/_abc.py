import abc
import typing

from typing import (Mapping, MutableMapping, List, Union, Iterable,
                    Iterator, TypeVar, Tuple, Dict, Optional)


_T = TypeVar('_T')


# Note: type defs are slightly different from __init__.pyi version The
# correct one (and checked by mypy) is the later.  Type checks here
# exists for sake of consistency and allowing to instantiate
# MultiMapiing[_T] in inline python code


class MultiMapping(Mapping[str, _T]):

    @abc.abstractmethod
    def getall(self, key: str, default: Optional[_T]=None) -> List[_T]:
        raise KeyError

    @abc.abstractmethod
    def getone(self, key: str, default: Optional[_T]=None) -> _T:
        raise KeyError


_Arg = Union[Mapping[str, _T],
             Dict[str, _T],
             MultiMapping[_T],
             Iterable[Tuple[str, _T]]]


class MutableMultiMapping(MultiMapping[_T], MutableMapping[str, _T]):

    @abc.abstractmethod
    def add(self, key: str, value: _T) -> None:
        raise NotImplementedError

    @abc.abstractmethod
    def extend(self, *args: _Arg[_T], **kwargs: _T) -> None:
        raise NotImplementedError

    @abc.abstractmethod
    def popone(self, key: str, default: Optional[_T]=None) -> _T:
        raise KeyError

    @abc.abstractmethod
    def popall(self, key: str, default: Optional[_T]=None) -> List[_T]:
        raise KeyError
