import functools
import json
import os
import pathlib
import platform
import sys

from distutils import log
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

extensions = [
    Extension(
        "multidict._multidict",
        ["multidict/_multidict.c"],
        extra_compile_args=CFLAGS,
    ),
]


from setuptools.command.build_ext import build_ext


class TraceableBinaryExtensionCmd(build_ext):
    def _ext_mod_path_for(self, ext) -> list[str]:
        if not DEBUG_BUILD:
            raise LookupError

        fullname = self.get_ext_fullname(ext.name)
        return fullname.split('.')

    def _ext_tracing_file_for(self, ext) -> Path:
        if not DEBUG_BUILD:
            raise LookupError

        modpath = self._ext_mod_path_for(ext)

        # NOTE: `.o` file is unnecessary for gcovr to function
        return pathlib.Path(*modpath).with_suffix('.gcno')

    def _ext_tracing_data_dir_for(self, ext) -> tuple[str]:
        if not DEBUG_BUILD:
            raise LookupError

        modpath = self._ext_mod_path_for(ext)
        package = '.'.join(modpath[:-1])
        build_py = self.get_finalized_command('build_py')  # ??
        package_dir = pathlib.Path(build_py.get_package_dir(package))
        return package_dir / '__tracing-data__'

    @functools.cached_property
    def _ext_tracing_data_dir_map(self) -> dict[str, tuple[str]]:
        extensions_with_tracing_data = self.extensions if DEBUG_BUILD else ()

        return {
            ext.name: self._ext_tracing_data_dir_for(ext)
            for ext in extensions_with_tracing_data
        }

    def _extra_ext_data_files_for(self, ext) -> tuple[str]:
        if not DEBUG_BUILD:
            return ()

        tracing_data_in_package_dir = self._ext_tracing_data_dir_map[ext.name]
        tracing_data_file_in_tmp_dir = self._ext_tracing_file_for(ext)
        tracing_data_file_in_package_dir = tracing_data_in_package_dir / tracing_data_file_in_tmp_dir
        build_meta_json_path = tracing_data_in_package_dir / 'build-metadata.json'

        return (build_meta_json_path, tracing_data_file_in_package_dir)

    @functools.cached_property
    def _extra_ext_data_files_map(self) -> dict[str, tuple[str]]:
        extensions_with_tracing_data = self.extensions if DEBUG_BUILD else ()

        return {
            ext.name: self._extra_ext_data_files_for(ext)
            for ext in extensions_with_tracing_data
        }

    @functools.cached_property
    def _extra_wheel_data_files(self) -> tuple[pathlib.Path]:
        extensions_with_tracing_data = self.extensions if DEBUG_BUILD else ()

        return tuple(
            relative_path
            for ext in extensions_with_tracing_data
            for relative_path in self._extra_ext_data_files_map[ext.name]
        )

    def get_outputs(self) -> list[str]:
        """Return absolute file paths to be included in the wheel."""
        base_outputs = super().get_outputs()

        build_dir_path = pathlib.Path(self.build_lib)
        tracing_outputs = (
            build_dir_path / relative_path
            for relative_path in self._extra_wheel_data_files
        )

        # NOTE: Files returned here end up in wheels and then `site-packages/`.
        # NOTE: Editable installs rely on the tracing files to be copied into
        # NOTE: the source checkout, which is happening in `run()`.
        return [*base_outputs, *tracing_outputs]

    def build_extension(self, ext):
        super().build_extension(ext)

        if not DEBUG_BUILD:
            log.info('Not a debug build. Skipping tracing data...')
            return

        log.info('Copying tracing data into the build directory')
        tracing_data_file_in_tmp_dir = self._ext_tracing_file_for(ext)
        tracing_data_in_package_dir = self._ext_tracing_data_dir_map[ext.name]
        tracing_data_file_in_package_dir = tracing_data_in_package_dir / tracing_data_file_in_tmp_dir

        tracing_data_in_build_dir = pathlib.Path(self.build_lib) / tracing_data_in_package_dir

        tracing_data_file_in_build_dir_absolute = tracing_data_in_build_dir / tracing_data_file_in_tmp_dir

        tracing_data_file_in_build_dir_absolute.parent.mkdir(exist_ok=True, parents=True)
        # NOTE: `gcc` writes `.gcno` files next to `.o` which is the temporary
        # NOTE: directory for us (`build_temp`). This copies it over into the
        # NOTE: regular build directory (`build_lib`) producing the layout we
        # NOTE: expect in wheels / site-packages / source checkout. It's later
        # NOTE: copied over to those places along with the shared libraries.
        self.copy_file(
            pathlib.Path(self.build_temp) / tracing_data_file_in_tmp_dir,
            tracing_data_file_in_build_dir_absolute,
            level=self.verbose,
        )

        # NOTE: `gcc` writes an absolute path to `.gcno` files into the shared
        # NOTE: library at build time. It may point to an arbitrary temporary
        # NOTE: directory that we don't care for. When pytest imports this
        # NOTE: file, the C-extension attempts writing a `.gcda` file in the
        # NOTE: same directory. We want it to be written in the source checkout
        # NOTE: next to the tracing file. For this, we record information about
        # NOTE: the desired location relative to the project root, bundle it
        # NOTE: in the wheel and the tests will read from it, and set the
        # NOTE: respective environment variables so the coverage data ends up
        # NOTE: where we expect it to. `gcovr` will also work better with
        # NOTE: predictable data locations.
        # NOTE:
        # NOTE: The contributors won't have to set the env vars manually
        # NOTE: $ GCOV_PREFIX=multidict/__tracing-data__/ \
        # NOTE:   GCOV_PREFIX_STRIP=2 \
        # NOTE:     python -Im pytest
        build_meta_path = tracing_data_in_build_dir / 'build-metadata.json'
        tmp_path_length = len(pathlib.Path(self.build_temp).resolve().parents)
        build_meta_path.write_text(
            json.dumps(
                {
                    'GCOV_PREFIX': str(tracing_data_in_package_dir),
                    'GCOV_PREFIX_STRIP': str(tmp_path_length),
                },
            ),
            encoding='utf-8',
        )

    def run(self):
        super().run()

        if not self.inplace:
            log.info('Not an editable install. Skipping tracing data...')

        if not DEBUG_BUILD:
            log.info('Not a debug build. Skipping tracing data...')
            return

        log.info(
            'Editable install in debug mode. Copying tracing data in-tree...',
        )

        # NOTE: Editable installs usually expect data in-tree. This handles
        # NOTE: the cases of `python setup.py build_ext --inplace` and
        # NOTE: `pip install -e .`
        # NOTE: Normal wheel builds include files returned by `get_outputs()`.

        build_dir_path = pathlib.Path(self.build_lib)
        for relative_path in self._extra_wheel_data_files:
            relative_path.parent.mkdir(exist_ok=True, parents=True)
            self.copy_file(
                build_dir_path / relative_path,
                relative_path,
                level=self.verbose,
            )

        for ext in self.extensions:
            tracing_data_in_pkg_dir = self._ext_tracing_data_dir_map[ext.name]
            (tracing_data_in_pkg_dir / '.gitignore').write_text('*\n')


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
