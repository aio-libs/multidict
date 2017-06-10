from multidict._multidict import (MultiDict, CIMultiDict, getversion)
from multidict._multidict_py import (MultiDict as _MultiDict,
                                     CIMultiDict as _CIMultiDict,
                                     getversion as _getversion)

import unittest


class VersionMixin:
    cls = None

    def getver(self, md):
        raise NotImplementedError

    def test_getversion_bad_param(self):
        with self.assertRaises(TypeError):
            self.getver(1)

    def test_ctor(self):
        m1 = self.cls()
        v1 = self.getver(m1)
        m2 = self.cls()
        v2 = self.getver(m2)
        self.assertNotEqual(v1, v2)

    def test_add(self):
        m = self.cls()
        v = self.getver(m)
        m.add('key', 'val')
        self.assertGreater(self.getver(m), v)

    def test_delitem(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        del m['key']
        self.assertGreater(self.getver(m), v)

    def test_delitem_not_found(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        with self.assertRaises(KeyError):
            del m['notfound']
        self.assertEqual(self.getver(m), v)

    def test_setitem(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        m['key'] = 'val2'
        self.assertGreater(self.getver(m), v)

    def test_setitem_not_found(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        m['notfound'] = 'val2'
        self.assertGreater(self.getver(m), v)

    def test_clear(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        m.clear()
        self.assertGreater(self.getver(m), v)

    def test_setdefault(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        m.setdefault('key2', 'val2')
        self.assertGreater(self.getver(m), v)

    def test_popone(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        m.popone('key')
        self.assertGreater(self.getver(m), v)

    def test_popone_default(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        m.popone('key2', 'default')
        self.assertEqual(self.getver(m), v)

    def test_popone_key_error(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        with self.assertRaises(KeyError):
            m.popone('key2')
        self.assertEqual(self.getver(m), v)

    def test_pop(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        m.pop('key')
        self.assertGreater(self.getver(m), v)

    def test_pop_default(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        m.pop('key2', 'default')
        self.assertEqual(self.getver(m), v)

    def test_pop_key_error(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        with self.assertRaises(KeyError):
            m.pop('key2')
        self.assertEqual(self.getver(m), v)

    def test_popall(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        m.popall('key')
        self.assertGreater(self.getver(m), v)

    def test_popall_default(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        m.popall('key2', 'default')
        self.assertEqual(self.getver(m), v)

    def test_popall_key_error(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        with self.assertRaises(KeyError):
            m.popall('key2')
        self.assertEqual(self.getver(m), v)

    def test_popitem(self):
        m = self.cls()
        m.add('key', 'val')
        v = self.getver(m)
        m.popitem()
        self.assertGreater(self.getver(m), v)

    def test_popitem_key_error(self):
        m = self.cls()
        v = self.getver(m)
        with self.assertRaises(KeyError):
            m.popitem()
        self.assertEqual(self.getver(m), v)


class TestMultiDict(unittest.TestCase, VersionMixin):

    cls = MultiDict

    def getver(self, md):
        return getversion(md)


class TestCIMultiDict(unittest.TestCase, VersionMixin):

    cls = CIMultiDict

    def getver(self, md):
        return getversion(md)


class TestPyMultiDict(unittest.TestCase, VersionMixin):

    cls = _MultiDict

    def getver(self, md):
        return _getversion(md)


class TestPyCIMultiDict(unittest.TestCase, VersionMixin):

    cls = _CIMultiDict

    def getver(self, md):
        return _getversion(md)
