import gc

from multidict._compat import USE_CYTHON

if USE_CYTHON:
    from multidict._multidict import istr

from multidict._multidict_py import istr as _istr  # noqa: E402


class IStrMixin:

    cls = None

    def test_ctor(self):
        s = self.cls()
        assert '' == s

    def test_ctor_str(self):
        s = self.cls('a')
        assert 'A' == s

    def test_ctor_str_uppercase(self):
        s = self.cls('A')
        assert 'A' == s

    def test_ctor_istr(self):
        s = self.cls('A')
        s2 = self.cls(s)
        assert 'A' == s
        assert s is s2

    def test_ctor_buffer(self):
        s = self.cls(b'a')
        assert "B'A'" == s

    def test_ctor_repr(self):
        s = self.cls(None)
        assert 'None' == s

    def test_title(self):
        s = self.cls('a')
        assert s is s.title()

    def test_str(self):
        s = self.cls('a')
        s1 = str(s)
        assert s1 == 'A'
        assert type(s1) is str

    def xtest_eq(self):
        s1 = 'Abc'
        s2 = self.cls(s1)
        assert s1 == s2
        assert s1.lower() == s2


class TestPyIStr(IStrMixin):
    cls = _istr

    @staticmethod
    def _create_strs():
        _istr('foobarbaz')
        istr2 = _istr()
        _istr(istr2)

    def test_leak(self):
        gc.collect()
        cnt = len(gc.get_objects())
        for _ in range(10000):
            self._create_strs()

        gc.collect()
        cnt2 = len(gc.get_objects())
        assert abs(cnt - cnt2) < 10  # on PyPy these numbers are not equal


if USE_CYTHON:
    class TestIStr(IStrMixin):
        cls = istr
