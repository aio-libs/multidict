import json
import os
import pathlib
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

# LDFLAGS = ['--coverage', '-coverage', '-lgcov'] if DEBUG_BUILD else []

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
    def run(self):
        super().run()
        from distutils import log
        log.info(f'{self.build_lib=}')
        log.info(f'{self.build_temp=}')

        if not DEBUG_BUILD:
            log.info('Not a debug build. Exiting early...')
            return

        for ext in self.extensions:
            fullname = self.get_ext_fullname(ext.name)
            modpath = fullname.split('.')
            package = '.'.join(modpath[:-1])
            build_py = self.get_finalized_command('build_py')  # ??
            package_dir = pathlib.Path(build_py.get_package_dir(package))
            tracing_data_file_in_tmp_dir = pathlib.Path(*modpath).with_suffix('.gcno')  # `.o` file is unnecessary for gcovr to function
            tracing_data_in_package_dir = package_dir / '__tracing-data__'

            tracing_data_file_in_tmp_dir_absolute = pathlib.Path(self.build_temp) / tracing_data_file_in_tmp_dir
            tracing_data_file_in_package_dir = tracing_data_in_package_dir / tracing_data_file_in_tmp_dir

            tracing_data_file_in_package_dir.parent.mkdir(exist_ok=True, parents=True)
            (tracing_data_in_package_dir / '.gitignore').write_text('*\n')

            log.info(f'GCOV_PREFIX={tracing_data_in_package_dir !s}')
            log.info(f'GCOV_PREFIX_STRIP={len(pathlib.Path(self.build_temp).parents) !s}')
            build_meta_json_path = (tracing_data_in_package_dir / 'build-metadata.json')
            build_meta_json_path.write_text(
                json.dumps(
                    {
                        'GCOV_PREFIX': str(tracing_data_in_package_dir),
                        'GCOV_PREFIX_STRIP': str(len(pathlib.Path(self.build_temp).parents)),
                    },
                ),
                encoding='utf-8',
            )
            # TODO: PWD dir hack — https://maskray.me/blog/2023-04-25-compiler-output-files
            # breakpoint()
            self.copy_file(
                tracing_data_file_in_tmp_dir_absolute,
                tracing_data_file_in_package_dir,
                level=self.verbose,
            )
        # GCOV_PREFIX=multidict/ GCOV_PREFIX_STRIP=4 some-venv/bin/python -Im pytest tests/test_istr.py
        # GCOV_PREFIX_STRIP=2 some-venv/bin/python -Im pytest tests/test_istr.py
        # GCOV_PREFIX=ext/ GCOV_PREFIX_STRIP=3 some-venv/bin/python -Im pytest tests/test_istr.py
        # GCOV_PREFIX=__tracing-data__/ GCOV_PREFIX_STRIP=2 some-venv/bin/python -Im pytest tests/test_istr.py
        # GCOV_PREFIX=multidict/__tracing-data__/ GCOV_PREFIX_STRIP=3 some-venv/bin/python -Im pytest
        # GCOV_PREFIX=multidict/__tracing-data__/multidict/ GCOV_PREFIX_STRIP=3 some-venv/bin/python -Im pytest
        # *FINAL*: GCOV_PREFIX=multidict/__tracing-data__/ GCOV_PREFIX_STRIP=2 some-venv/bin/python -Im pytest


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
