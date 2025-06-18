from Cython.Build import cythonize
from setuptools import Extension, setup
import pathlib
import multidict
import shutil

# to compile run 
# python tools/setup_cython_test.py build_ext --inplace 
# NOTE: do not run in the tools directory directly

if __name__ == "__main__":
    setup(
        ext_modules=cythonize(
            Extension(
                "tests._multidict_cython",
                sources=["tests/_multidict_cython.pyx"]
            )
        ),
        include_dirs=[multidict.get_include()]
    )
