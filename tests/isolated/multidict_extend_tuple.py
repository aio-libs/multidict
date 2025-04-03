import gc
from typing import Any

import objgraph  # type: ignore[import-untyped]

from multidict import MultiDict


class NotLeakTuple(tuple[Any, ...]):
    """A subclassed tuple to make it easier to test for leaks."""


md: MultiDict[str] = MultiDict()
for _ in range(10000):
    md.extend(NotLeakTuple())
del md
gc.collect()

leaked = len(objgraph.by_type("NotLeakTuple"))
if leaked:
    print(
        f"Memory leak detected: {leaked} instances of NotLeakTuple not collected by GC"
    )
exit(1 if leaked else 0)
