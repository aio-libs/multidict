import codecs
import os
import re
import sys
from setuptools import setup, Extension
from distutils.errors import (CCompilerError, DistutilsExecError,
                              DistutilsPlatformError)
from distutils.command.build_ext import build_ext


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


extensions = [Extension('multidict._multidict',
                        ['multidict/_multidict' + ext],
                        # extra_compile_args=["-g"],
                        # extra_link_args=["-g"],
)]


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
        except (CCompilerError, DistutilsExecError,
                DistutilsPlatformError, ValueError):
            raise BuildFailed()


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

tests_require = ['pytest']

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
        'CI: Travis': 'https://travis-ci.org/{}'.format(repo_slug),
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
    cmdclass=dict(build_ext=ve_build_ext))

try:
    setup(**args)
except BuildFailed:
    print("************************************************************")
    print("Cannot compile C accelerator module, use pure python version")
    print("************************************************************")
    del args['ext_modules']
    del args['cmdclass']
    setup(**args)
