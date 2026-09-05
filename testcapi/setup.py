from setuptools import Extension, setup
import multidict
import os

NO_EXTENSIONS = bool(os.environ.get("MULTIDICT_NO_EXTENSIONS"))

extensions = [
    Extension(
        "testcapi._api",
        ["testcapi/_api.c"],
        include_dirs=multidict.__path__,
        extra_compile_args=["-O0", "-g3", "-UNDEBUG"],
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
