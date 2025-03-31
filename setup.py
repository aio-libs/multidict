import os
import platform
import sys

from setuptools import Extension, setup

NO_EXTENSIONS = bool(os.environ.get("MULTIDICT_NO_EXTENSIONS"))

if sys.implementation.name != "cpython":
    NO_EXTENSIONS = True

CFLAGS = ["-O3"]
# CFLAGS = ["-O0", "-g"]
if platform.system() != "Windows":
    CFLAGS.extend(
        [
            "-std=c99",
            "-Wall",
            "-Wsign-compare",
            "-Wconversion",
            "-fno-strict-aliasing",
            "-pedantic",
        ]
    )

extensions = [
    Extension(
        "multidict._multidict",
        ["multidict/_multidict.c"],
        extra_compile_args=CFLAGS,
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
