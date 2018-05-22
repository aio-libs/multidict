from __future__ import absolute_import

import sys
from collections import abc
from collections.abc import Iterable, Set

from cpython.object cimport PyObject_Str, Py_NE, PyObject_RichCompare

from ._abc import MultiMapping, MutableMultiMapping
from ._istr import istr

from ._pair_list cimport *

cdef object _marker = object()

upstr = istr  # for relaxing backward compatibility problems
cdef object _istr = istr

pair_list_init(istr)


def getversion(_Base md):
    return pair_list_version(md._impl)


cdef class _Base:

    cdef object _impl

    def _get_impl(self):
        return self._impl

    cdef str _title(self, s):
        typ = type(s)
        if typ is str:
            return <str>s
        elif typ is _istr:
            return PyObject_Str(s)
        else:
            return str(s)

    def getall(self, key, default=_marker):
        """Return a list of all values matching the key."""
        return self._getall(self._title(key), key, default)

    cdef _getall(self, str identity, key, default):
        try:
            return pair_list_get_all(self._impl, identity, key)
        except KeyError:
            if default is not _marker:
                return default
            else:
                raise

    def getone(self, key, default=_marker):
        """Get first value matching the key."""
        return self._getone(self._title(key), key, default)

    cdef _getone(self, str identity, key, default):
        try:
            return pair_list_get_one(self._impl, identity, key)
        except KeyError:
            if default is not _marker:
                return default
            else:
                raise

    # Mapping interface #

    def __getitem__(self, key):
        return self._getone(self._title(key), key, _marker)

    def get(self, key, default=None):
        """Get first value matching the key.

        The method is alias for .getone().
        """
        return self._getone(self._title(key), key, default)

    def __contains__(self, key):
        return self._contains(key)

    cdef _contains(self, key):
        return pair_list_contains(self._impl, self._title(key))

    def __iter__(self):
        return iter(self.keys())

    def __len__(self):
        return pair_list_len(self._impl)

    cpdef keys(self):
        """Return a new view of the dictionary's keys."""
        return _KeysView.__new__(_KeysView, self)

    def items(self):
        """Return a new view of the dictionary's items *(key, value) pairs)."""
        return _ItemsView.__new__(_ItemsView, self)

    def values(self):
        """Return a new view of the dictionary's values."""
        return _ValuesView.__new__(_ValuesView, self)

    def __repr__(self):
        lst = []
        for k, v in self.items():
            lst.append("'{}': {!r}".format(k, v))
        body = ', '.join(lst)
        return '<{}({})>'.format(self.__class__.__name__, body)

    cdef _eq_to_mapping(self, other):
        cdef PyObject *identity
        cdef PyObject *value
        cdef object ident
        cdef object val
        cdef Py_ssize_t pos
        cdef Py_hash_t h
        if pair_list_len(self._impl) != len(other):
            return False
        pos = 0
        while pair_list_next(self._impl, &pos, &identity, NULL, &value):
            ident = <object>identity
            val = <object>value
            for k, v in other.items():
                if self._title(k) != ident:
                    continue
                if v == val:
                    break
            else:
                return False
        return True

    def __eq__(self, arg):
        cdef Py_ssize_t pos1
        cdef PyObject *identity1
        cdef PyObject *value1
        cdef Py_hash_t h1

        cdef Py_ssize_t pos2
        cdef PyObject *identity2
        cdef PyObject *value2
        cdef Py_hash_t h2

        cdef _Base other

        if isinstance(arg, _Base):
            other = <_Base>arg
            if pair_list_len(self._impl) != pair_list_len(other._impl):
                return False
            pos1 = pos2 = 0
            while (_pair_list_next(self._impl, &pos1, &identity1,
                                   NULL, &value1, &h1) and
                   _pair_list_next(other._impl, &pos2, &identity2,
                                   NULL, &value2, &h2)):
                if h1 != h2:
                    return False
                if PyObject_RichCompare(<object>identity1, <object>identity2, Py_NE):
                    return False
                if PyObject_RichCompare(<object>value1, <object>value2, Py_NE):
                    return False
            return True
        elif isinstance(arg, abc.Mapping):
            return self._eq_to_mapping(arg)
        else:
            return NotImplemented


cdef class MultiDictProxy(_Base):
    _proxy_classes = (MultiDict, MultiDictProxy)
    _base_class = MultiDict

    def __init__(self, arg):
        cdef _Base base
        if not isinstance(arg, self._proxy_classes):
            raise TypeError(
                'ctor requires {} instance'
                ', not {}'.format(
                    ' or '.join(self._proxy_classes),
                    type(arg)))

        base = arg
        self._impl = base._impl

    def __reduce__(self):
        raise TypeError("can't pickle {} objects"
                        .format(self.__class__.__name__))

    def copy(self):
        """Return a copy of itself."""
        return self._base_class(self)

MultiMapping.register(MultiDictProxy)


cdef class CIMultiDictProxy(MultiDictProxy):
    _proxy_classes = (CIMultiDict, CIMultiDictProxy)
    _base_class = CIMultiDict

    cdef str _title(self, s):
        typ = type(s)
        if typ is str:
            return <str>(s.title())
        elif type(s) is _istr:
            return PyObject_Str(s)
        return s.title()


MultiMapping.register(CIMultiDictProxy)


cdef str _str(key):
    typ = type(key)
    if typ is str:
        return <str>key
    if typ is _istr:
        return PyObject_Str(key)
    elif issubclass(typ, str):
        return str(key)
    else:
        raise TypeError("MultiDict keys should be either str "
                        "or subclasses of str")


cdef class MultiDict(_Base):
    """An ordered dictionary that can have multiple values for each key."""

    def __init__(self, *args, **kwargs):
        self._impl = pair_list_new()
        self._extend(args, kwargs, 'MultiDict', True)

    def __reduce__(self):
        return (
            self.__class__,
            (list(self.items()),)
        )

    cdef _extend(self, tuple args, dict kwargs, name, bint do_add):
        cdef object key
        cdef object value
        cdef object arg
        cdef object i

        if len(args) > 1:
            raise TypeError("{} takes at most 1 positional argument"
                            " ({} given)".format(name, len(args)))

        if args:
            arg = args[0]
            if isinstance(arg, _Base):
                if do_add:
                    self._append_items((<_Base>arg)._impl)
                else:
                    self._update_items((<_Base>arg)._impl)
            else:
                if hasattr(arg, 'items'):
                    arg = arg.items()
                if do_add:
                    self._append_items_seq(arg, name)
                else:
                    self._update_items_seq(arg, name)

        for key, value in kwargs.items():
            if do_add:
                self._add(key, value)
            else:
                self._replace(key, value)

    cdef object _update_items(self, object impl):
        pair_list_update(self._impl, impl)

    cdef object _update_items_seq(self, object arg, object name):
        cdef object i
        cdef object identity
        cdef object key
        cdef object value
        cdef Py_hash_t h
        cdef object impl = pair_list_new()
        for i in arg:
            if not len(i) == 2:
                raise TypeError(
                    "{} takes either dict or list of (key, value) "
                    "tuples".format(name))
            key = _str(i[0])
            value = i[1]
            identity = self._title(key)
            h = hash(identity)
            _pair_list_add_with_hash(impl, identity, key, value, h)

        pair_list_update(self._impl, impl)

    cdef object _append_items(self, object impl):
        cdef PyObject *key
        cdef PyObject *val
        cdef Py_ssize_t pos
        pos = 0
        while _pair_list_next(impl, &pos, NULL, &key, &val, NULL):
            self._add(<object>key, <object>val)

    cdef object _append_items_seq(self, object arg, object name):
        cdef object i
        cdef object key
        cdef object value
        for i in arg:
            if not len(i) == 2:
                raise TypeError(
                    "{} takes either dict or list of (key, value) "
                    "tuples".format(name))
            key = i[0]
            value = i[1]
            self._add(key, value)

    cdef _add(self, key, value):
        pair_list_add(self._impl, key, value);

    cdef _replace(self, key, value):
        cdef str identity = self._title(key)
        cdef str k = _str(key)
        cdef Py_hash_t h = hash(identity)
        pair_list_replace(self._impl, identity, k, value, h)

    def add(self, key, value):
        """Add the key and value, not overwriting any previous value."""
        self._add(key, value)

    def copy(self):
        """Return a copy of itself."""
        ret = MultiDict()
        ret._extend((list(self.items()),), {}, 'copy', True)
        return ret

    def extend(self, *args, **kwargs):
        """Extend current MultiDict with more values.

        This method must be used instead of update.
        """
        self._extend(args, kwargs, "extend", True)

    def clear(self):
        """Remove all items from MultiDict"""
        pair_list_clear(self._impl)

    # MutableMapping interface #

    def __setitem__(self, key, value):
        self._replace(key, value)

    def __delitem__(self, key):
        self._remove(key)

    cdef _remove(self, key):
        cdef str identity = self._title(key)
        cdef Py_hash_t h = hash(identity)
        pair_list_del_hash(self._impl, identity, key, h)

    def setdefault(self, key, default=None):
        """Return value for key, set value to default if key is not present."""
        cdef str identity = self._title(key)
        return pair_list_set_default(self._impl, identity, key, default)

    def popone(self, key, default=_marker):
        """Remove the last occurrence of key and return the corresponding
        value.

        If key is not found, default is returned if given, otherwise
        KeyError is raised.

        """
        try:
            return pair_list_pop_one(self._impl, self._title(key), key)
        except KeyError:
            if default is _marker:
                raise
            else:
                return default

    pop = popone

    def popall(self, key, default=_marker):
        """Remove all occurrences of key and return the list of corresponding
        values.

        If key is not found, default is returned if given, otherwise
        KeyError is raised.

        """
        try:
            return pair_list_pop_all(self._impl, self._title(key), key)
        except KeyError:
            if default is _marker:
                raise
            else:
                return default

    def popitem(self):
        """Remove and return an arbitrary (key, value) pair."""
        return pair_list_pop_item(self._impl)

    def update(self, *args, **kwargs):
        """Update the dictionary from *other*, overwriting existing keys."""
        self._extend(args, kwargs, "update", False)


MutableMultiMapping.register(MultiDict)


cdef class CIMultiDict(MultiDict):
    """An ordered dictionary that can have multiple values for each key."""

    def __init__(self, *args, **kwargs):
        self._impl = ci_pair_list_new()
        self._extend(args, kwargs, 'CIMultiDict', True)

    def __reduce__(self):
        return (
            self.__class__,
            (list(self.items()),),
        )

    cdef str _title(self, s):
        typ = type(s)
        if typ is str:
            return <str>(s.title())
        elif type(s) is _istr:
            return PyObject_Str(s)
        return s.title()

    def copy(self):
        """Return a copy of itself."""
        ret = CIMultiDict()
        ret._extend((list(self.items()),), {}, 'copy', True)
        return ret



MutableMultiMapping.register(CIMultiDict)


cdef class _ViewBase:

    cdef _Base _md

    def __cinit__(self, _Base md):
        self._md = md

    def __len__(self):
        return pair_list_len(self._md._impl)


cdef class _ViewBaseSet(_ViewBase):

    def __richcmp__(self, other, op):
        if op == 0:  # <
            if not isinstance(other, Set):
                return NotImplemented
            return len(self) < len(other) and self <= other
        elif op == 1:  # <=
            if not isinstance(other, Set):
                return NotImplemented
            if len(self) > len(other):
                return False
            for elem in self:
                if elem not in other:
                    return False
            return True
        elif op == 2:  # ==
            if not isinstance(other, Set):
                return NotImplemented
            return len(self) == len(other) and self <= other
        elif op == 3:  # !=
            return not self == other
        elif op == 4:  #  >
            if not isinstance(other, Set):
                return NotImplemented
            return len(self) > len(other) and self >= other
        elif op == 5:  # >=
            if not isinstance(other, Set):
                return NotImplemented
            if len(self) < len(other):
                return False
            for elem in other:
                if elem not in self:
                    return False
            return True

    def __and__(self, other):
        if not isinstance(other, Iterable):
            return NotImplemented
        if isinstance(self, _ViewBaseSet):
            self = set(iter(self))
        if isinstance(other, _ViewBaseSet):
            other = set(iter(other))
        if not isinstance(other, Set):
            other = set(iter(other))
        return self & other

    def __or__(self, other):
        if not isinstance(other, Iterable):
            return NotImplemented
        if isinstance(self, _ViewBaseSet):
            self = set(iter(self))
        if isinstance(other, _ViewBaseSet):
            other = set(iter(other))
        if not isinstance(other, Set):
            other = set(iter(other))
        return self | other

    def __sub__(self, other):
        if not isinstance(other, Iterable):
            return NotImplemented
        if isinstance(self, _ViewBaseSet):
            self = set(iter(self))
        if isinstance(other, _ViewBaseSet):
            other = set(iter(other))
        if not isinstance(other, Set):
            other = set(iter(other))
        return self - other

    def __xor__(self, other):
        if not isinstance(other, Iterable):
            return NotImplemented
        if isinstance(self, _ViewBaseSet):
            self = set(iter(self))
        if isinstance(other, _ViewBaseSet):
            other = set(iter(other))
        if not isinstance(other, Set):
            other = set(iter(other))
        return self ^ other


cdef class _ItemsIter:
    cdef object _impl
    cdef Py_ssize_t _current
    cdef uint64_t _version

    def __cinit__(self, object impl):
        self._impl = impl
        self._current = 0
        self._version = pair_list_version(impl)

    def __iter__(self):
        return self

    def __next__(self):
        if self._version != pair_list_version(self._impl):
            raise RuntimeError("Dictionary changed during iteration")
        cdef PyObject *key
        cdef PyObject *value
        if not _pair_list_next(self._impl,
                               &self._current, NULL, &key, &value, NULL):
            raise StopIteration
        return (<object>key, <object>value)


cdef class _ItemsView(_ViewBaseSet):

    def isdisjoint(self, other):
        'Return True if two sets have a null intersection.'
        for v in other:
            if v in self:
                return False
        return True

    def __contains__(self, i):
        cdef str key
        cdef object value
        assert isinstance(i, tuple) or isinstance(i, list)
        assert len(i) == 2
        key = i[0]
        value = i[1]
        for k, v in self:
            if key == k and value == v:
                return True
        return False

    def __iter__(self):
        return _ItemsIter.__new__(_ItemsIter, self._md._impl)

    def __repr__(self):
        lst = []
        for k ,v in self:
            lst.append("{!r}: {!r}".format(k, v))
        body = ', '.join(lst)
        return '{}({})'.format(self.__class__.__name__, body)


abc.ItemsView.register(_ItemsView)


cdef class _ValuesIter:
    cdef object _impl
    cdef Py_ssize_t _current
    cdef uint64_t _version

    def __cinit__(self, object impl):
        self._impl = impl
        self._current = 0
        self._version = pair_list_version(impl)

    def __iter__(self):
        return self

    def __next__(self):
        if self._version != pair_list_version(self._impl):
            raise RuntimeError("Dictionary changed during iteration")
        cdef PyObject *value
        if not _pair_list_next(self._impl,
                              &self._current, NULL, NULL, &value, NULL):
            raise StopIteration
        return <object>value


cdef class _ValuesView(_ViewBase):

    def __contains__(self, value):
        for v in self:
            if v == value:
                return True
        return False

    def __iter__(self):
        return _ValuesIter.__new__(_ValuesIter, self._md._impl)

    def __repr__(self):
        lst = []
        for v in self:
            lst.append("{!r}".format(v))
        body = ', '.join(lst)
        return '{}({})'.format(self.__class__.__name__, body)


abc.ValuesView.register(_ValuesView)


cdef class _KeysIter:
    cdef object _impl
    cdef Py_ssize_t _current
    cdef uint64_t _version

    def __cinit__(self, object impl):
        self._impl = impl
        self._current = 0
        self._version = pair_list_version(impl)

    def __iter__(self):
        return self

    def __next__(self):
        if self._version != pair_list_version(self._impl):
            raise RuntimeError("Dictionary changed during iteration")
        cdef PyObject * key
        if not pair_list_next(self._impl,
                              &self._current, NULL, &key, NULL):
            raise StopIteration
        return <object>(key)


cdef class _KeysView(_ViewBaseSet):

    def isdisjoint(self, other):
        'Return True if two sets have a null intersection.'
        for k in other:
            print('isdisjoint.1', k)
            if k in self:
                print('isdisjoint.2', k)
                return False
        return True

    def __contains__(self, value):
        return self._md._contains(value)

    def __iter__(self):
        return _KeysIter.__new__(_KeysIter, self._md._impl)

    def __repr__(self):
        lst = []
        for k in self:
            lst.append("{!r}".format(k))
        body = ', '.join(lst)
        return '{}({})'.format(self.__class__.__name__, body)


abc.KeysView.register(_KeysView)
