from multidict._multidict import istr
from multidict._multidict_py import istr as _istr
import gc
import psutil


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
        p = psutil.Process()
        info = p.memory_info()
        for _ in range(10000):
            self._create_strs()

        gc.collect()
        info2 = p.memory_info()
        rss_diff = info2.rss - info.rss
        assert rss_diff == 0


class TestIStr(IStrMixin):
    cls = istr
