import gc
import timeit


setitem = """\
dct[key] = 'new value'
"""

getitem = """\
dct[key]
"""

cython_multidict = """\
from multidict import MultiDict, istr
dct = MultiDict()
"""

python_multidict = """\
from multidict._multidict_py import MultiDict, istr
dct = MultiDict()
"""

cython_cimultidict = """\
from multidict import CIMultiDict, istr
dct = CIMultiDict()
"""

python_cimultidict = """\
from multidict._multidict_py import CIMultiDict, istr
dct = CIMultiDict()
"""

fill = """\
for i in range(20):
    dct['key'+str(i)] = str(i)

key = 'key10'
"""

fill_istr = """\
for i in range(20):
    key = istr('key'+str(i))
    dct[key] = str(i)

key = istr('key10')
"""

istr_from_istr = """\
istr(val)
"""

make_istr = """\
val = istr('VaLuE')
"""

print("Cython / Python / x")

t1 = timeit.timeit(setitem, cython_multidict+fill)
gc.collect()
t2 = timeit.timeit(setitem, python_multidict+fill)
gc.collect()

print("MD.setitem str: {:.3f}s {:.3f}s {:.1f}x".format(t1, t2, t2/t1))

t1 = timeit.timeit(setitem, cython_multidict+fill_istr)
gc.collect()
t2 = timeit.timeit(setitem, python_multidict+fill_istr)
gc.collect()

print("MD.setitem istr: {:.3f}s {:.3f}s {:.1f}x".format(t1, t2, t2/t1))


t1 = timeit.timeit(getitem, cython_multidict+fill)
gc.collect()
t2 = timeit.timeit(getitem, python_multidict+fill)
gc.collect()

print("MD.getitem str: {:.3f}s {:.3f}s {:.1f}x".format(t1, t2, t2/t1))


t1 = timeit.timeit(getitem, cython_cimultidict+fill)
gc.collect()
t2 = timeit.timeit(getitem, python_cimultidict+fill)
gc.collect()
print("CI.getitem str: {:.3f}s {:.3f}s {:.1f}x".format(t1, t2, t2/t1))

t1 = timeit.timeit(getitem, cython_cimultidict+fill_istr)
gc.collect()
t2 = timeit.timeit(getitem, python_cimultidict+fill_istr)
gc.collect()
print("CI.getitem istr: {:.3f}s {:.3f}s {:.1f}x".format(t1, t2, t2/t1))

t1 = timeit.timeit(istr_from_istr, cython_cimultidict+make_istr)
gc.collect()
t2 = timeit.timeit(istr_from_istr, python_cimultidict+make_istr)
gc.collect()
print("istr from istr: {:.3f}s {:.3f}s {:.1f}x".format(t1, t2, t2/t1))
