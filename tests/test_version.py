import pytest

from multidict._compat import USE_CYTHON

if USE_CYTHON:
    from multidict._multidict import MultiDict, CIMultiDict, getversion

from multidict._multidict_py import (MultiDict as _MultiDict,  # noqa: E402
                                     CIMultiDict as _CIMultiDict,
                                     getversion as _getversion)


class VersionMixin:
    cls = NotImplemented

    def getver(self, md):
        raise NotImplementedError

    def test_getversion_bad_param(self):
        with pytest.raises(TypeError):
            self.getver(1)

    def test_ctor(self):
        m1 = self.cls()
        v1 = self.getver(m1)
        m2 = self.cls()
        v2 = self.getver(m2)
        assert v1 != v2

    def test_add(self):
        m = self.cls()
        v = self.getver(m)
        m.add('key', 'val')
        assert self.getver(m) > v

    def test_delitem(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        del m['key']
        assert self.getver(m) > v

    def test_delitem_not_found(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        with pytest.raises(KeyError):
            del m['notfound']
        assert self.getver(m) == v

    def test_setitem(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        m['key'] = 'val2'
        assert self.getver(m) > v

    def test_setitem_not_found(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        m['notfound'] = 'val2'
        assert self.getver(m) > v

    def test_clear(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        m.clear()
        assert self.getver(m) > v

    def test_setdefault(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        m.setdefault('key2', 'val2')
        assert self.getver(m) > v

    def test_popone(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        m.popone('key')
        assert self.getver(m) > v

    def test_popone_default(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        m.popone('key2', 'default')
        assert self.getver(m) == v

    def test_popone_key_error(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        with pytest.raises(KeyError):
            m.popone('key2')
        assert self.getver(m) == v

    def test_pop(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        m.pop('key')
        assert self.getver(m) > v

    def test_pop_default(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        m.pop('key2', 'default')
        assert self.getver(m) == v

    def test_pop_key_error(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        with pytest.raises(KeyError):
            m.pop('key2')
        assert self.getver(m) == v

    def test_popall(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        m.popall('key')
        assert self.getver(m) > v

    def test_popall_default(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        m.popall('key2', 'default')
        assert self.getver(m) == v

    def test_popall_key_error(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        with pytest.raises(KeyError):
            m.popall('key2')
        assert self.getver(m) == v

    def test_popitem(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        m.popitem()
        assert self.getver(m) > v

    def test_popitem_key_error(self):
        m = self.cls()
        v = self.getver(m)
        with pytest.raises(KeyError):
            m.popitem()
        assert self.getver(m) == v


if USE_CYTHON:
    class TestMultiDict(VersionMixin):

        cls = MultiDict

        def getver(self, md):
            return getversion(md)


if USE_CYTHON:
    class TestCIMultiDict(VersionMixin):

        cls = CIMultiDict

        def getver(self, md):
            return getversion(md)


class TestPyMultiDict(VersionMixin):

    cls = _MultiDict

    def getver(self, md):
        return _getversion(md)


class TestPyCIMultiDict(VersionMixin):

    cls = _CIMultiDict

    def getver(self, md):
        return _getversion(md)
