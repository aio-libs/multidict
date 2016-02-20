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

print("Cython setitem str: {:.3f} sec".format(
    timeit.timeit(setitem, cython_multidict+fill)))

print("Python setitem str: {:.3f} sec".format(
    timeit.timeit(setitem, python_multidict+fill)))


print("Cython getitem str: {:.3f} sec".format(
    timeit.timeit(getitem, cython_multidict+fill)))

print("Python getitem str: {:.3f} sec".format(
    timeit.timeit(getitem, python_multidict+fill)))


print("Cython getitem upstr: {:.3f} sec".format(
    timeit.timeit(getitem, cython_cimultidict+fill)))

print("Python getitem upstr: {:.3f} sec".format(
    timeit.timeit(getitem, python_cimultidict+fill)))
