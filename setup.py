import os
import platform
import sys

from setuptools import Extension, setup

NO_EXTENSIONS = bool(os.environ.get("MULTIDICT_NO_EXTENSIONS"))
DEBUG_BUILD = bool(os.environ.get("MULTIDICT_DEBUG_BUILD"))
ASAN_BUILD = bool(os.environ.get("MULTIDICT_ASAN_BUILD"))
TSAN_BUILD = bool(os.environ.get("MULTIDICT_TSAN_BUILD"))
NO_FREELIST = bool(os.environ.get("MULTIDICT_NO_FREELIST"))

if sys.implementation.name != "cpython":
    NO_EXTENSIONS = True

CFLAGS = ["-O0", "-g3", "-UNDEBUG"] if DEBUG_BUILD else ["-O3", "-DNDEBUG"]
LDFLAGS = []

if NO_FREELIST:
    # Stops the C extension reusing freed blocks from its module-state
    # pools. A pooled block never reaches free(), so AddressSanitizer
    # can neither poison it nor report a use-after-free on it; an ASan
    # run wants a pass with this set to keep that coverage.
    CFLAGS.append("-DMULTIDICT_NO_FREELIST")

if platform.system() != "Windows":
    CFLAGS.extend(
        [
            "-std=c11",
            "-Wall",
            "-Wsign-compare",
            "-Wconversion",
            "-fno-strict-aliasing",
            "-Wno-conversion",
            "-Werror",
        ]
    )
    if DEBUG_BUILD and (ASAN_BUILD or TSAN_BUILD):
        # Sanitizers are opt-in on top of MULTIDICT_DEBUG_BUILD, not implied
        # by it: plain MULTIDICT_DEBUG_BUILD=1 is relied on across CI (and by
        # contributors) to just build with -O0/-UNDEBUG and run normally,
        # with no LD_PRELOAD of a sanitizer runtime. ThreadSanitizer also
        # can't be combined with Address/UndefinedBehaviorSanitizer in the
        # same binary, so MULTIDICT_TSAN_BUILD selects a dedicated
        # thread-safety build instead of layering on top of the other two.
        SANITIZE_FLAGS = [
            "-fsanitize=thread" if TSAN_BUILD else "-fsanitize=address,undefined",
            "-fno-sanitize-recover=all",
            "-fno-omit-frame-pointer",
        ]
        CFLAGS.extend(SANITIZE_FLAGS)
        LDFLAGS.extend(SANITIZE_FLAGS)

extensions = [
    Extension(
        "multidict._multidict",
        ["multidict/_multidict.c"],
        extra_compile_args=CFLAGS,
        extra_link_args=LDFLAGS,
    ),
    # Exercises the public C API capsule from tests; not for normal use.
    Extension(
        "multidict._testcapi",
        ["multidict/_testcapi.c"],
        extra_compile_args=CFLAGS,
        extra_link_args=LDFLAGS,
    ),
]


if not NO_EXTENSIONS:
    try:
        from Cython.Build import cythonize
    except ImportError:
        cythonize = None

    if cythonize is not None:
        # Only built when Cython happens to be available at build time
        # (deliberately, via `pip install Cython` + `--no-build-isolation`
        # -- see AGENTS.md). Never a real dependency: absent from ordinary
        # installs and from every release wheel, which build in isolation
        # without it.
        extensions += cythonize(
            [
                Extension(
                    "multidict._testcyapi",
                    ["multidict/_testcyapi.pyx"],
                    extra_compile_args=CFLAGS,
                    extra_link_args=LDFLAGS,
                ),
            ],
            language_level=3,
        )

    print("*********************")
    print("* Accelerated build *")
    print("*********************")
    setup(ext_modules=extensions)
else:
    print("*********************")
    print("* Pure Python build *")
    print("*********************")
    setup()
