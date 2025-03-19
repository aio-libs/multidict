Don't create Python exception if key is not found but default value if provided.

The PR affects ``MultiDict.getone()``, ``MultiDict.getall()``,
``MultiDict.get()``, ``MultiDict.pop()``, ``MultiDict.popone()``, and
``MultiDict.popall()`` methods if the key is messed *and* default is provided.

Additionally, comparison multidicts with straight dictionaries becomes slightly faster on
Python 3.13+.

The speedup gain is about 60% for mentioned cases.
