import codecs
from itertools import islice
import os
import platform
import re
import sys
from setuptools import setup, Extension
from distutils.errors import (CCompilerError, DistutilsExecError,
                              DistutilsPlatformError)
from distutils.command.build_ext import build_ext


PYPY = platform.python_implementation() == 'PyPy'


# Fallbacks for PyPy: don't use C extensions
extensions = []
cmdclass = {}

if not PYPY:
    try:
        from Cython.Build import cythonize
        USE_CYTHON = True
    except ImportError:
        USE_CYTHON = False

    ext = '.pyx' if USE_CYTHON else '.c'

    if bool(os.environ.get('PROFILE_BUILD')):
        macros = [('CYTHON_TRACE', '1')]
    else:
        macros = []

    CFLAGS = ['-O2']
    # CFLAGS = ['-g']
    if platform.system() != 'Windows':
        CFLAGS.extend(['-std=c99', '-Wall', '-Wsign-compare', '-Wconversion',
                       '-fno-strict-aliasing'])

    extensions = [
        Extension(
            'multidict._multidict',
            [
                'multidict/_multidict' + ext,
                'multidict/_pair_list.c',
                'multidict/_multidict_iter.c',
            ],
            extra_compile_args=CFLAGS
        )
    ]

    if USE_CYTHON:
        if bool(os.environ.get('PROFILE_BUILD')):
            directives = {"linetrace": True}
        else:
            directives = {}
        extensions = cythonize(extensions, compiler_directives=directives)

    extensions.append(Extension('multidict._istr',
                                ['multidict/_istr.c']))

    class BuildFailed(Exception):
        pass

    class ve_build_ext(build_ext):
        # This class allows C extension building to fail.

        def run(self):
            try:
                build_ext.run(self)
            except (DistutilsPlatformError, FileNotFoundError):
                raise BuildFailed()

        def build_extension(self, ext):
            try:
                build_ext.build_extension(self, ext)
            except (DistutilsExecError,
                    DistutilsPlatformError, ValueError):
                raise BuildFailed()

    cmdclass['build_ext'] = ve_build_ext


try:
    from wheel.bdist_wheel import bdist_wheel

    def emit_chunks_of(num, from_):
        res = []
        for i in from_:
            res.append(i)
            if len(res) >= num:
                yield tuple(res)
                res = []
        if len(res):
            yield tuple(res)

    class _bdist_wheel(bdist_wheel):
        user_options = bdist_wheel.user_options + [
            ('plat-tag-chunk-num=', None,
             'macOS platform tags subset size'),
            ('plat-tag-chunk-pos=', None,
             'macOS platform tags chunk position'),
        ]

        @property
        def macos_platforms_range(self):
            return range(6, 14)

        def initialize_options(self):
            super().initialize_options()
            self.plat_tag_chunk_num = None
            self.plat_tag_chunk_pos = None

        def finalize_options(self):
            super().finalize_options()

            try:
                self.plat_tag_chunk_num = int(self.plat_tag_chunk_num)
                if not self.plat_tag_chunk_num:
                    raise ValueError
            except TypeError:
                """None value."""
                pass
            except ValueError:
                """Empty string or 0."""
                raise ValueError(
                    'plat-tag-chunk-num must be a positive number'
                )

            try:
                self.plat_tag_chunk_pos = int(self.plat_tag_chunk_pos)
                if not self.plat_tag_chunk_pos:
                    raise ValueError
            except TypeError:
                """None value."""
                pass
            except ValueError:
                """Empty string or 0."""
                raise ValueError(
                    'plat-tag-chunk-num must be a positive number'
                )

        def get_macos_compatible_tags(self):
            return (
                'macosx_10_{ver}_{arch}'.format(ver=v, arch=a)
                for v in self.macos_platforms_range
                for a in ('intel', 'x86_64')
            )

        def select_macos_tags_chunk(self):
            compatible_platforms = self.get_macos_compatible_tags()

            if self.plat_tag_chunk_num and self.plat_tag_chunk_pos:
                compatible_platforms = islice(
                    emit_chunks_of(
                        self.plat_tag_chunk_num,
                        compatible_platforms,
                    ),
                    self.plat_tag_chunk_pos - 1,
                    self.plat_tag_chunk_pos,
                )
                try:
                    compatible_platforms = next(
                        iter(compatible_platforms)
                    )
                except StopIteration:
                    raise ValueError(
                        'You must select an existing macOS tag chunk.'
                    )

            return tuple(compatible_platforms)

        def get_tag(self):
            tag = super().get_tag()
            if tag[2] != 'macosx_10_6_intel':
                return tag

            compatible_platforms = self.select_macos_tags_chunk()
            new_version_tag = '.'.join(compatible_platforms)

            return tag[:2] + (new_version_tag, )

    cmdclass['bdist_wheel'] = _bdist_wheel
except ImportError:
    """Wheel is not installed."""


with codecs.open(os.path.join(os.path.abspath(os.path.dirname(
        __file__)), 'multidict', '__init__.py'), 'r', 'latin1') as fp:
    try:
        version = re.findall(r"^__version__ = '([^']+)'\r?$",
                             fp.read(), re.M)[0]
    except IndexError:
        raise RuntimeError('Unable to determine version.')


def read(f):
    return open(os.path.join(os.path.dirname(__file__), f)).read().strip()


NEEDS_PYTEST = {'pytest', 'test'}.intersection(sys.argv)
pytest_runner = ['pytest-runner'] if NEEDS_PYTEST else []

tests_require = ['pytest', 'pytest-cov']

name = 'multidict'
appveyor_slug = 'asvetlov/{}'.format(name)  # FIXME: move under aio-libs/* slug
repo_slug = 'aio-libs/{}'.format(name)
repo_url = 'https://github.com/{}'.format(repo_slug)

args = dict(
    name=name,
    version=version,
    description=('multidict implementation'),
    long_description=read('README.rst'),
    classifiers=[
        'License :: OSI Approved :: Apache Software License',
        'Intended Audience :: Developers',
        'Programming Language :: Python',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.4',
        'Programming Language :: Python :: 3.5',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7',
        'Development Status :: 5 - Production/Stable',
    ],
    author='Andrew Svetlov',
    author_email='andrew.svetlov@gmail.com',
    url=repo_url,
    project_urls={
        'Chat: Gitter': 'https://gitter.im/aio-libs/Lobby',
        'CI: AppVeyor': 'https://ci.appveyor.com/project/{}'.format(appveyor_slug),
        'CI: Circle': 'https://circleci.com/gh/{}'.format(repo_slug),
        'CI: Shippable': 'https://app.shippable.com/github/{}'.format(repo_slug),
        'CI: Travis': 'https://travis-ci.com/{}'.format(repo_slug),
        'Coverage: codecov': 'https://codecov.io/github/{}'.format(repo_slug),
        'Docs: RTD': 'https://{}.readthedocs.io'.format(name),
        'GitHub: issues': '{}/issues'.format(repo_url),
        'GitHub: repo': repo_url,
    },
    license='Apache 2',
    packages=['multidict'],
    python_requires='>=3.4.1',
    tests_require=tests_require,
    setup_requires=pytest_runner,
    include_package_data=True,
    ext_modules=extensions,
    cmdclass=cmdclass)

try:
    setup(**args)
except BuildFailed:
    print("************************************************************")
    print("Cannot compile C accelerator module, use pure python version")
    print("************************************************************")
    del args['ext_modules']
    del args['cmdclass']
    setup(**args)
