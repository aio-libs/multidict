import gc
from typing import Any

import objgraph  # type: ignore[import-untyped]

from multidict import MultiDict


class NoLeakDict(dict[str, Any]):
    """A subclassed dict to make it easier to test for leaks."""


md: MultiDict[str] = MultiDict()
for _ in range(100):
    md.update(NoLeakDict())
del md
gc.collect()

leaked = len(objgraph.by_type("NoLeakDict"))
if leaked:
    print(f"Memory leak detected: {leaked} instances of NoLeakDict not collected by GC")
exit(1 if leaked else 0)
