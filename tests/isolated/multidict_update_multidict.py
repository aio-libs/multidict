import gc
import sys

import objgraph  # type: ignore[import-untyped]

from multidict import MultiDict

md: MultiDict[str] = MultiDict()
for _ in range(100):
    md.extend(MultiDict())
del md
gc.collect()
leaked = len(objgraph.by_type("MultiDict"))
if leaked:
    print(f"Memory leak detected: {leaked} instances of MultiDict not collected by GC")
sys.exit(1 if leaked else 0)
