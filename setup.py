import os
import platform
import sys

from setuptools import Extension, setup

NO_EXTENSIONS = bool(os.environ.get("MULTIDICT_NO_EXTENSIONS"))
DEBUG_BUILD = bool(os.environ.get("MULTIDICT_DEBUG_BUILD"))
TSAN_BUILD = bool(os.environ.get("MULTIDICT_TSAN_BUILD"))

if sys.implementation.name != "cpython":
    NO_EXTENSIONS = True

CFLAGS = ["-O0", "-g3", "-UNDEBUG"] if DEBUG_BUILD else ["-O3", "-DNDEBUG"]
LDFLAGS = []

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
    if DEBUG_BUILD:
        # ThreadSanitizer can't be combined with Address/UndefinedBehaviorSanitizer
        # in the same binary, so MULTIDICT_TSAN_BUILD switches to a dedicated
        # thread-safety build instead of layering on top of the default one.
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
]


if not NO_EXTENSIONS:
    print("*********************")
    print("* Accelerated build *")
    print("*********************")
    setup(ext_modules=extensions)
else:
    print("*********************")
    print("* Pure Python build *")
    print("*********************")
    setup()
