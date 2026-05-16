import os
import platform
import sys

from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext

NO_EXTENSIONS = bool(os.environ.get("MULTIDICT_NO_EXTENSIONS"))
DEBUG_BUILD = bool(os.environ.get("MULTIDICT_DEBUG_BUILD"))


class BuildExt(build_ext):
    def build_extensions(self) -> None:
        if self.parallel is None:
            self.parallel = os.cpu_count() or 1
        super().build_extensions()


if sys.implementation.name != "cpython":
    NO_EXTENSIONS = True

CFLAGS = ["-O0", "-g3", "-UNDEBUG"] if DEBUG_BUILD else ["-O3", "-DNDEBUG"]

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
    setup(ext_modules=extensions, cmdclass={"build_ext": BuildExt})
else:
    print("*********************")
    print("* Pure Python build *")
    print("*********************")
    setup()
