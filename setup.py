import os
import platform
import re
import subprocess
import sys

from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext

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


def _gcc_major(compiler):
    """GCC's major version, or None for clang, MSVC and anything unknown."""
    cc = getattr(compiler, "compiler_so", None)
    if not cc:
        return None
    try:
        out = subprocess.run(
            [*cc, "-dM", "-E", "-x", "c", os.devnull],
            capture_output=True,
            check=True,
            text=True,
        ).stdout
    except (OSError, subprocess.CalledProcessError):
        return None
    gnuc = re.search(r"^#define __GNUC__ (\d+)$", out, re.M)
    if gnuc is None or re.search(r"^#define __clang__ ", out, re.M):
        return None
    return int(gnuc.group(1))


class BuildExt(build_ext):
    def build_extensions(self):
        major = _gcc_major(self.compiler)
        if major is not None and major < 10:
            # GCC 9 flags _PyLong_CompactValue() in CPython 3.12+'s own
            # headers under -Wsign-conversion.
            for ext in self.extensions:
                ext.extra_compile_args = [
                    f for f in ext.extra_compile_args if f != "-Wconversion"
                ]
        super().build_extensions()


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
                    # Cython's generated code includes CPython's internal
                    # headers, which are not -Wconversion clean.
                    extra_compile_args=[f for f in CFLAGS if f != "-Wconversion"],
                    extra_link_args=LDFLAGS,
                ),
            ],
            language_level=3,
        )

    print("*********************")
    print("* Accelerated build *")
    print("*********************")
    setup(ext_modules=extensions, cmdclass={"build_ext": BuildExt})
else:
    print("*********************")
    print("* Pure Python build *")
    print("*********************")
    setup()
