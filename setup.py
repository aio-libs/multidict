import os
import sys

from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext

NO_EXTENSIONS = bool(os.environ.get("MULTIDICT_NO_EXTENSIONS"))
DEBUG_BUILD = bool(os.environ.get("MULTIDICT_DEBUG_BUILD"))

if sys.implementation.name != "cpython":
    NO_EXTENSIONS = True

BASE_CFLAGS = ["O0", "g3", "UNDEBUG"] if DEBUG_BUILD else ["O3", "DNDEBUG"]

UNIX_CFLAGS = [
    "-std=c11",
    "-Wall",
    "-Wsign-compare",
    "-Wconversion",
    "-fno-strict-aliasing",
    "-Wno-conversion",
    "-Werror",
]

MSVC_CFLAGS = ["/std:c11", "/experimental:c11atomics"]


class BuildExt(build_ext):
    def build_extensions(self):
        if self.compiler.compiler_type == "msvc":
            for ext in self.extensions:
                ext.extra_compile_args.extend(MSVC_CFLAGS)
                for flag in BASE_CFLAGS:
                    # XXX: MSVC Doesn't have a /O3 flag only O2 is possible...
                    ext.extra_compile_args.append("/O2" if flag == "O3" else f"/{flag}")
        else:
            for ext in self.extensions:
                ext.extra_compile_args.extend(UNIX_CFLAGS)
                for flag in BASE_CFLAGS:
                    ext.extra_compile_args.append(f"-{flag}")
        super().build_extensions()


extensions = [
    Extension(
        "multidict._multidict",
        ["multidict/_multidict.c"],
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
