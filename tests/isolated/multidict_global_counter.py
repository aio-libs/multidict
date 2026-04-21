import threading
import multidict
from multidict import MultiDict
import sysconfig

FREETHREADED = bool(sysconfig.get_config_var("Py_GIL_DISABLED"))
if not FREETHREADED:
    raise SystemExit(0)

md = MultiDict()
N, M = 3, 100
baseline = multidict.getversion(md)


def worker(tid):
    for i in range(M):
        md[f"k{tid}_{i}"] = i


if __name__ == "__main__":
    threads = [threading.Thread(target=worker, args=(tid,)) for tid in range(N)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()

    observed = multidict.getversion(md) - baseline
    expected = N * M
    assert expected == observed, (
        f"expected delta: {expected}"
        f"   observed: {observed}   "
        f"lost: {expected - observed}"
    )
