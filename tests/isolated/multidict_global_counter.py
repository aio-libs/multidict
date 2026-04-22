import sysconfig
import threading

import multidict
from multidict import MultiDict

FREETHREADED = bool(sysconfig.get_config_var("Py_GIL_DISABLED"))


md: MultiDict[int] = MultiDict()
N, M = 3, 100
baseline = multidict.getversion(md)  # type: ignore[arg-type]


def worker(tid: int) -> None:
    for i in range(M):
        md[f"k{tid}_{i}"] = i


if (__name__ == "__main__") and FREETHREADED:
    threads = [threading.Thread(target=worker, args=(tid,)) for tid in range(N)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()

    observed = multidict.getversion(md) - baseline  # type: ignore[arg-type]
    expected = N * M
    assert expected == observed, (
        f"expected delta: {expected}"
        f"   observed: {observed}   "
        f"lost: {expected - observed}"
    )
