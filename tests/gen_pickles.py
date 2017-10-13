import pickle

from multidict._compat import USE_CYTHON

if USE_CYTHON:
    from multidict._multidict import MultiDict, CIMultiDict  # noqa

from multidict._multidict_py import (MultiDict as PyMultiDict,  # noqa
                                     CIMultiDict as PyCIMultiDict)


def write(name, proto):
    cls = globals()[name]
    d = cls([('a', 1), ('a', 2)])
    with open('{}.pickle.{}'.format(name.lower(), proto), 'wb') as f:
        pickle.dump(d, f, proto)


for proto in range(pickle.HIGHEST_PROTOCOL):
    for name in ('MultiDict', 'CIMultiDict', 'PyMultiDict', 'PyCIMultiDict'):
        if USE_CYTHON or name.startswith('Py'):
            write(name, proto)
