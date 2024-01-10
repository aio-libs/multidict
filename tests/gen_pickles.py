import pickle

import multidict


def write(tag, cls, proto):
    d = cls([("a", 1), ("a", 2)])
    file_basename = f"{cls.__name__.lower()}-{tag}"
    with open(f"{file_basename}.pickle.{proto}", "wb") as f:
        pickle.dump(d, f, proto)


def generate():
    _impl_map = {
        "c-extension": multidict._multidict,
        "pure-python": multidict._multidict_py,
    }
    for proto in range(pickle.HIGHEST_PROTOCOL + 1):
        for tag, impl in _impl_map.items():
            for cls in impl.CIMultiDict, impl.MultiDict:
                write(tag, cls, proto)


if __name__ == "__main__":
    generate()
