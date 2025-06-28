import os
import platform
import sys

from setuptools import Extension, setup

NO_EXTENSIONS = bool(os.environ.get("MULTIDICT_NO_EXTENSIONS"))
DEBUG_BUILD = bool(os.environ.get("MULTIDICT_DEBUG_BUILD"))

if sys.implementation.name != "cpython":
    NO_EXTENSIONS = True

CFLAGS = ["-O0", "-g3", "-UNDEBUG"] if DEBUG_BUILD else ["-O3", "-DNDEBUG"]
# https://gcc.gnu.org/onlinedocs/gcc/Gcov-Data-Files.html
# `-ftest-coverage` -> `.gcno` is in the same place as `.o` -> `{self.build_temp}/multidict`
# `-fprofile-dir` -> change the location of the `.gcda` file
#
# https://gcc.gnu.org/onlinedocs/gcc/Invoking-Gcov.html
# `-fkeep-inline-functions`
# `-fkeep-static-functions`
# NOTE: Moving `_multidict.gcno` and `_multidict.o` under `multidict/` before running `pytest`, and `_multidict.gcda` after, worked. Prior to running gcovr.
if DEBUG_BUILD:
    CFLAGS.extend(['--coverage', '-coverage', '-fprofile-arcs', '-ftest-coverage', '-fPIC'])
    # CFLAGS.extend(['-fprofile-dir=./multidict/'])

LDFLAGS = ['--coverage', '-coverage', '-lgcov'] if DEBUG_BUILD else []

# CFLAGS = ["-O2"]
# CFLAGS.extend(['--coverage', '-g', '-O0'])
# CFLAGS = ['-g']

if platform.system() != "Windows" and False:
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

os.environ['CC'] = 'ccache gcc'
os.environ['CFLAGS'] = ' '.join(CFLAGS)
# os.environ['LDFLAGS'] = ' '.join(LDFLAGS)

extensions = [
    Extension(
        "multidict._multidict",
        ["multidict/_multidict.c"],
        extra_compile_args=CFLAGS,
        # extra_link_args=LDFLAGS,
    ),
]


from setuptools.command.build_ext import build_ext


class TraceableBinaryExtensionCmd(build_ext):
    # def initialize_options(self):  # ??
    # def finalize_options(self):  # ??
    #     super().finalize_options()
    #     # self._pkg_dir = self.get_finalized_command('build_py').get_package_dir('multidict')
    #     # self.build_temp = f'{self.build_lib}/{self._pkg_dir}/__debug-symbols__'
    #     # self.build_temp = self.build_lib
    #     # breakpoint()

    def run(self):
        super().run()
        from distutils import log
        log.info(f'{self.build_lib=}')
        log.info(f'{self.build_temp=}')
        # self.copy_file(f'{self.build_temp}/multidict/_multidict.o', f'multidict/_multidict.o', level=self.verbose)  # `.o` file seems unnecessary for gcovr to function

        for ext in self.extensions:
            fullname = self.get_ext_fullname(ext.name)
            # breakpoint()
            filename = self.get_ext_filename(fullname)
            modpath = fullname.split('.')
            package = '.'.join(modpath[:-1])
            build_py = self.get_finalized_command('build_py')  # ??
            package_dir = build_py.get_package_dir(package)
            inplace_file = os.path.join(package_dir, os.path.basename(filename))
            regular_file = os.path.join(self.build_lib, filename)
            tracing_data_file_in_tmp_dir = os.path.join(*modpath) + '.gcno'
            # f'{self.build_temp}/{package_dir}/'
            tracing_data_in_tmp_dir = f'{self.build_temp}/{package_dir}/'
            tracing_data_in_package_dir = f'{package_dir}/__tracing-data__/'
            # breakpoint()

            os.makedirs(os.path.join(tracing_data_in_package_dir, os.path.dirname(tracing_data_file_in_tmp_dir)), exist_ok=True)
            with open(os.path.join(tracing_data_in_package_dir, '.gitignore'), 'w') as gitignore_fd:
                gitignore_fd.writelines(('*', ))

            self.copy_file(
                # os.path.join(tracing_data_in_tmp_dir, '_multidict.gcno'),  # `.o` file is unnecessary for gcovr to function
                os.path.join(self.build_temp, tracing_data_file_in_tmp_dir),  # `.o` file is unnecessary for gcovr to function
                os.path.join(tracing_data_in_package_dir, tracing_data_file_in_tmp_dir),
                level=self.verbose,
            )
        # GCOV_PREFIX=multidict/ GCOV_PREFIX_STRIP=4 some-venv/bin/python -Im pytest tests/test_istr.py
        # GCOV_PREFIX_STRIP=2 some-venv/bin/python -Im pytest tests/test_istr.py
        # GCOV_PREFIX=ext/ GCOV_PREFIX_STRIP=3 some-venv/bin/python -Im pytest tests/test_istr.py
        # GCOV_PREFIX=__tracing-data__/ GCOV_PREFIX_STRIP=2 some-venv/bin/python -Im pytest tests/test_istr.py
        # GCOV_PREFIX=multidict/__tracing-data__/ GCOV_PREFIX_STRIP=3 some-venv/bin/python -Im pytest
        # *FINAL*: GCOV_PREFIX=multidict/__tracing-data__/multidict/ GCOV_PREFIX_STRIP=3 some-venv/bin/python -Im pytest


if not NO_EXTENSIONS:
    print("*********************")
    print("* Accelerated build *")
    print("*********************")
    setup(
        cmdclass={'build_ext': TraceableBinaryExtensionCmd},
        ext_modules=extensions,
    )
else:
    print("*********************")
    print("* Pure Python build *")
    print("*********************")
    setup()
