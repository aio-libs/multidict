import os
import subprocess
import sys
import sysconfig
import textwrap

FREETHREADED = bool(sysconfig.get_config_var("Py_GIL_DISABLED"))


if __name__ == "__main__" and FREETHREADED:
    child = textwrap.dedent("""
        import signal, threading, time
        signal.alarm(20)
        from multidict import MultiDict
        md = MultiDict([(f"k{i}", i) for i in range(50)])
        errors = []
        stop = threading.Event()
        def repr_loop():
            while not stop.is_set():
                try: repr(md)
                except Exception as e: errors.append(type(e).__name__)
        def mutate_loop():
            i = 0
            while not stop.is_set(): md.add(f"m{i}", i); i += 1
        ts = [threading.Thread(target=repr_loop, daemon=True) for _ in range(2)]
        ts += [threading.Thread(target=mutate_loop, daemon=True) for _ in range(2)]
        for t in ts: t.start()
        time.sleep(0.3); stop.set(); time.sleep(0.05)
        print(f"repr errors: {len(errors)}")
    """)
    subprocess.run(
        [sys.executable, "-c", child],
        env={**os.environ, "PYTHON_GIL": "0"},
        capture_output=True,
        timeout=60,
        check=True,
    )
