# Some simple testing tasks (sorry, UNIX only).

all: test

.install-deps: $(shell find requirements -type f)
	pip install -U -r requirements/dev.txt
	touch .install-deps

flake: .install-deps
#	python setup.py check -rms
	flake8 multidict
	if python -c "import sys; sys.exit(sys.version_info < (3,5))"; then \
            flake8 tests; \
        fi


.develop: .install-deps $(shell find multidict -type f)
	rm -f multidict/*.so
	rm -f multidict/_multidict.c
	pip install -e .
	touch .develop

rmcache:
	rm -rf tests/__pycache__


mypy: .develop
	if python -c "import sys; sys.exit(sys.implementation.name != 'cpython')"; then \
	    mypy multidict tests; \
	fi


test: flake .develop rmcache mypy
	pytest -q ./tests/

vtest: flake .develop rmcache mypy
	pytest -s -v ./tests/

cov cover coverage:
	tox

profile-dev-base: .install-deps
	rm -f .develop
	rm -f multidict/*.so
	rm -f multidict/_multidict.c
	PROFILE_BUILD=x pip install -e .
	touch .develop


cov-dev: profile-dev-base rmcache mypy
	pytest --cov=multidict --cov-report=term --cov-report=html tests 
	@echo "open file://`pwd`/htmlcov/index.html"

cov-dev-full: profile-dev-base rmcache mypy
	AIOHTTPMULTIDICT_NO_EXTENSIONS=1 pytest --cov=multidict --cov-append tests 
	pytest --cov=multidict --cov-report=term --cov-report=html tests 
	@echo "open file://`pwd`/htmlcov/index.html"

clean:
	rm -rf `find . -name __pycache__`
	rm -f `find . -type f -name '*.py[co]' `
	rm -f `find . -type f -name '*~' `
	rm -f `find . -type f -name '.*~' `
	rm -f `find . -type f -name '@*' `
	rm -f `find . -type f -name '#*#' `
	rm -f `find . -type f -name '*.orig' `
	rm -f `find . -type f -name '*.rej' `
	rm -f .coverage
	rm -rf coverage
	rm -rf build
	rm -rf cover
	rm -rf htmlcov
	make -C docs clean SPHINXBUILD=false
	python3 setup.py clean
	rm -f multidict/_multidict.html
	rm -f multidict/_multidict.c
	rm -f multidict/_multidict.*.so
	rm -f multidict/_multidict.*.pyd
	rm -f multidict/_istr.*.so
	rm -f multidict/_istr.*.pyd
	rm -rf .tox
	rm -f .install-deps
	rm -f .develop

doc:
	make -C docs html
	@echo "open file://`pwd`/docs/_build/html/index.html"

doc-spelling:
	make -C docs spelling

install:
	pip install -U pip
	pip install -Ur requirements/dev.txt

wheel_x64:
	docker pull quay.io/pypa/manylinux1_x86_64
	docker run --rm -v `pwd`:/io quay.io/pypa/manylinux1_x86_64 /io/build-wheels.sh

.PHONY: all build venv flake test vtest testloop cov clean doc
