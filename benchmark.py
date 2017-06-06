import gc
import timeit


setitem = """\
dct[key] = 'new value'
"""

getitem = """\
dct[key]
"""

cython_multidict = """\
from multidict import MultiDict
dct = MultiDict()
"""

python_multidict = """\
from multidict._multidict_py import MultiDict
dct = MultiDict()
"""

cython_cimultidict = """\
from multidict import CIMultiDict, upstr
dct = CIMultiDict()
"""

python_cimultidict = """\
from multidict._multidict_py import CIMultiDict, upstr
dct = CIMultiDict()
"""

fill = """\
for i in range(20):
    dct['key'+str(i)] = str(i)

key = 'key10'
"""

fill_upstr = """\
for i in range(20):
    key = upstr('key'+str(i))
    dct[key] = str(i)

key = upstr('key10')
"""

upstr_from_upstr = """\
upstr(val)
"""

make_upstr = """\
val = upstr('VaLuE')
"""

print("Cython / Python / x")

t1 = timeit.timeit(setitem, cython_multidict+fill)
gc.collect()
t2 = timeit.timeit(setitem, python_multidict+fill)
gc.collect()

print("MD.setitem str: {:.3f}s {:3f}s {:1f}x".format(t1, t2, t2/t1))


t1 = timeit.timeit(getitem, cython_multidict+fill)
gc.collect()
t2 = timeit.timeit(getitem, python_multidict+fill)
gc.collect()

print("MD.getitem str: {:.3f}s {:3f}s {:1f}x".format(t1, t2, t2/t1))


t1 = timeit.timeit(getitem, cython_cimultidict+fill)
gc.collect()
t2 = timeit.timeit(getitem, python_cimultidict+fill)
gc.collect()
print("CI.getitem str: {:.3f}s {:3f}s {:1f}x".format(t1, t2, t2/t1))

t1 = timeit.timeit(getitem, cython_cimultidict+fill_upstr)
gc.collect()
t2 = timeit.timeit(getitem, python_cimultidict+fill_upstr)
gc.collect()
print("CI.getitem istr: {:.3f}s {:3f}s {:1f}x".format(t1, t2, t2/t1))

t1 = timeit.timeit(upstr_from_upstr, cython_cimultidict+make_upstr)
gc.collect()
t2 = timeit.timeit(upstr_from_upstr, python_cimultidict+make_upstr)
gc.collect()
print("istr from istr: {:.3f}s {:3f}s {:1f}x".format(t1, t2, t2/t1))
