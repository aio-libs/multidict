import os
import subprocess
import sys
import sysconfig
import textwrap

FREETHREADED = bool(sysconfig.get_config_var("Py_GIL_DISABLED"))

if __name__ == "__main__" and FREETHREADED:
    child = textwrap.dedent("""
        import threading
        from multidict import MultiDict
        md: MultiDict[int] = MultiDict()
        def worker(tid: int):
            for i in range(800):
                md.add(f"k{tid}_{i}", i)
        ts = [threading.Thread(target=worker, args=(tid,), daemon=True) for tid in range(4)]
        for t in ts: 
            t.start()
        for t in ts: 
            t.join()
        print(f"missing: {4*800 - len(md)}")
    """)
    subprocess.run(
        [sys.executable, "-c", child],
        env={**os.environ, "PYTHON_GIL": "0"},
        capture_output=True,
        timeout=60,
        check=True,
    )
