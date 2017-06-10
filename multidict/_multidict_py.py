from collections import abc
import sys

_marker = object()


class istr(str):

    """Case insensitive str."""

    __is_istr__ = True

    def __new__(cls, val='',
                encoding=sys.getdefaultencoding(), errors='strict'):
        if getattr(val, '__is_istr__', False):
            # Faster than instance check
            return val
        if isinstance(val, (bytes, bytearray, memoryview)):
            val = str(val, encoding, errors)
        elif isinstance(val, str):
            pass
        else:
            val = str(val)
        val = val.title()
        return str.__new__(cls, val)

    def title(self):
        return self


upstr = istr  # for relaxing backward compatibility problems


class _Base:

    def _title(self, key):
        return key

    def getall(self, key, default=_marker):
        """Return a list of all values matching the key."""
        identity = self._title(key)
        res = [v for i, k, v in self._items if i == identity]
        if res:
            return res
        if not res and default is not _marker:
            return default
        raise KeyError('Key not found: %r' % key)

    def getone(self, key, default=_marker):
        """Get first value matching the key."""
        identity = self._title(key)
        for i, k, v in self._items:
            if i == identity:
                return v
        if default is not _marker:
            return default
        raise KeyError('Key not found: %r' % key)

    # Mapping interface #

    def __getitem__(self, key):
        return self.getone(key)

    def get(self, key, default=None):
        """Get first value matching the key.

        The method is alias for .getone().
        """
        return self.getone(key, default)

    def __iter__(self):
        return iter(self.keys())

    def __len__(self):
        return len(self._items)

    def keys(self):
        """Return a new view of the dictionary's keys."""
        return _KeysView(self._items)

    def items(self):
        """Return a new view of the dictionary's items *(key, value) pairs)."""
        return _ItemsView(self._items)

    def values(self):
        """Return a new view of the dictionary's values."""
        return _ValuesView(self._items)

    def __eq__(self, other):
        if not isinstance(other, abc.Mapping):
            return NotImplemented
        if isinstance(other, _Base):
            lft = self._items
            rht = other._items
            if len(lft) != len(rht):
                return False
            for (i1, k2, v1), (i2, k2, v2) in zip(lft, rht):
                if i1 != i2 or v1 != v2:
                    return False
            return True
        for k, v in self.items():
            nv = other.get(k, _marker)
            if v != nv:
                return False
        return True

    def __contains__(self, key):
        identity = self._title(key)
        for i, k, v in self._items:
            if i == identity:
                return True
        return False

    def __repr__(self):
        body = ', '.join("'{}': {!r}".format(k, v) for k, v in self.items())
        return '<{}({})>'.format(self.__class__.__name__, body)


class _CIBase(_Base):

    def _title(self, key):
        return key.title()


class MultiDictProxy(_Base, abc.Mapping):

    def __init__(self, arg):
        if not isinstance(arg, (MultiDict, MultiDictProxy)):
            raise TypeError(
                'ctor requires MultiDict or MultiDictProxy instance'
                ', not {}'.format(
                    type(arg)))

        self._items = arg._items

    def copy(self):
        """Return a copy of itself."""
        return MultiDict(self.items())


class CIMultiDictProxy(_CIBase, MultiDictProxy):

    def __init__(self, arg):
        if not isinstance(arg, (CIMultiDict, CIMultiDictProxy)):
            raise TypeError(
                'ctor requires CIMultiDict or CIMultiDictProxy instance'
                ', not {}'.format(
                    type(arg)))

        self._items = arg._items

    def _title(self, key):
        return key.title()

    def copy(self):
        """Return a copy of itself."""
        return CIMultiDict(self.items())


class MultiDict(_Base, abc.MutableMapping):

    def __init__(self, *args, **kwargs):
        self._items = []

        self._extend(args, kwargs, self.__class__.__name__, self.add)

    def _title(self, key):
        return key

    def _key(self, key):
        if isinstance(key, str):
            return str(key)
        else:
            raise TypeError("MultiDict keys should be either str "
                            "or subclasses of str")

    def add(self, key, value):
        identity = self._title(key)
        self._items.append((identity, self._key(key), value))

    def copy(self):
        """Return a copy of itself."""
        cls = self.__class__
        return cls(self.items())

    def extend(self, *args, **kwargs):
        """Extend current MultiDict with more values.

        This method must be used instead of update.
        """
        self._extend(args, kwargs, 'extend', self.add)

    def _extend(self, args, kwargs, name, method):
        if len(args) > 1:
            raise TypeError("{} takes at most 1 positional argument"
                            " ({} given)".format(name, len(args)))
        if args:
            arg = args[0]
            if isinstance(args[0], MultiDictProxy):
                items = arg._items
            elif isinstance(args[0], MultiDict):
                items = arg._items
            elif hasattr(arg, 'items'):
                items = [(k, k, v) for k, v in arg.items()]
            else:
                items = []
                for item in arg:
                    if not len(item) == 2:
                        raise TypeError(
                            "{} takes either dict or list of (key, value) "
                            "tuples".format(name))
                    items.append((item[0], item[0], item[1]))

            for identity, key, value in items:
                method(key, value)

        for key, value in kwargs.items():
            method(key, value)

    def clear(self):
        """Remove all items from MultiDict."""
        self._items.clear()

    # Mapping interface #

    def __setitem__(self, key, value):
        self._replace(key, value)

    def __delitem__(self, key):
        items = self._items
        found = False
        for i in range(len(items) - 1, -1, -1):
            if items[i][0] == key:
                del items[i]
                found = True
        if not found:
            raise KeyError(key)

    def setdefault(self, key, default=None):
        """Return value for key, set value to default if key is not present."""
        for i, k, v in self._items:
            if i == key:
                return v
        self.add(key, default)
        return default

    def popone(self, key, default=_marker):
        """Remove specified key and return the corresponding value.

        If key is not found, d is returned if given, otherwise
        KeyError is raised.

        """
        for i in range(len(self._items)):
            if self._items[i][0] == key:
                value = self._items[i][2]
                del self._items[i]
                return value
        if default is _marker:
            raise KeyError(key)
        else:
            return default

    pop = popone

    def popall(self, key, default=_marker):
        """Remove all occurrences of key and return the list of corresponding
        values.

        If key is not found, default is returned if given, otherwise
        KeyError is raised.

        """
        found = False
        identity = self._title(key)
        ret = []
        for i in range(len(self._items)-1, -1, -1):
            item = self._items[i]
            if item[0] == identity:
                ret.append(item[2])
                del self._items[i]
                found = True
        if not found:
            if default is _marker:
                raise KeyError(key)
            else:
                return default
        else:
            ret.reverse()
            return ret

    def popitem(self):
        """Remove and return an arbitrary (key, value) pair."""
        if self._items:
            i = self._items.pop(0)
            return i[1], i[2]
        else:
            raise KeyError("empty multidict")

    def update(self, *args, **kwargs):
        """Update the dictionary from *other*, overwriting existing keys."""
        self._extend(args, kwargs, 'update', self._replace)

    def _replace(self, key, value):
        key = self._key(key)
        identity = self._title(key)
        items = self._items

        for i in range(len(items)-1, -1, -1):
            item = items[i]
            if item[0] == identity:
                items[i] = (identity, key, value)
                # i points to last found item
                rgt = i
                break
        else:
            self._items.append((identity, key, value))
            return

        # remove all precending items
        i = 0
        while i < rgt:
            item = items[i]
            if item[0] == identity:
                del items[i]
                rgt -= 1
            else:
                i += 1


class CIMultiDict(_CIBase, MultiDict):

    def __setitem__(self, key, value):
        super().__setitem__(key.title(), value)

    def __delitem__(self, key):
        super().__delitem__(key.title())

    def _replace(self, key, value):
        super()._replace(key.title(), value)

    def pop(self, key, default=_marker):
        """Remove specified key and return the corresponding value.

        If key is not found, d is returned if given, otherwise
        KeyError is raised.

        """
        key = key.title()
        return super().pop(key, default)

    def setdefault(self, key, default=None):
        """Return value for key, set value to default if key is not present."""
        key = key.title()
        return super().setdefault(key, default)

    def popitem(self):
        """Remove and return an arbitrary (key, value) pair."""
        if self._items:
            identity, key, value = self._items.pop(0)
            return key, value
        else:
            raise KeyError("empty multidict")


class _ViewBase:

    def __init__(self, items):
        self._items = items

    def __len__(self):
        return len(self._items)


class _ItemsView(_ViewBase, abc.ItemsView):

    def __contains__(self, item):
        assert isinstance(item, tuple) or isinstance(item, list)
        assert len(item) == 2
        for i, k, v in self._items:
            if item[0] == k and item[1] == v:
                return True
        return False

    def __iter__(self):
        for i, k, v in self._items:
            yield k, v

    def __repr__(self):
        lst = []
        for item in self._items:
            lst.append("{!r}: {!r}".format(item[1], item[2]))
        body = ', '.join(lst)
        return '{}({})'.format(self.__class__.__name__, body)


class _ValuesView(_ViewBase, abc.ValuesView):

    def __contains__(self, value):
        for item in self._items:
            if item[2] == value:
                return True
        return False

    def __iter__(self):
        for item in self._items:
            yield item[2]

    def __repr__(self):
        lst = []
        for item in self._items:
            lst.append("{!r}".format(item[2]))
        body = ', '.join(lst)
        return '{}({})'.format(self.__class__.__name__, body)


class _KeysView(_ViewBase, abc.KeysView):

    def __contains__(self, key):
        for item in self._items:
            if item[1] == key:
                return True
        return False

    def __iter__(self):
        for item in self._items:
            yield item[1]

    def __repr__(self):
        lst = []
        for item in self._items:
            lst.append("{!r}".format(item[1]))
        body = ', '.join(lst)
        return '{}({})'.format(self.__class__.__name__, body)
