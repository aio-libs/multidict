# Some simple testing tasks (sorry, UNIX only).
.PHONY: all build test vtest cov clean doc


PYXS = $(wildcard multidict/*.pyx)
SRC = multidict tests setup.py

all: test

.install-deps: $(shell find requirements -type f)
	pip install -r requirements/dev.txt
	@touch .install-deps

lint:
	python -Im pre_commit run --all-files --show-diff-on-failure

.develop: .install-deps $(shell find multidict -type f)
	pip install -e .
	@touch .develop

test: .develop
	@pytest -q

vtest: .develop
	@pytest -s -v

cov-dev: .develop
	@pytest --cov-report=html
	@echo "open file://`pwd`/htmlcov/index.html"

cov-ci-run: .develop
	@echo "Regular run"
	@pytest --cov-report=html

cov-dev-full: cov-ci-run
	@echo "open file://`pwd`/htmlcov/index.html"

doc:
	@make -C docs html SPHINXOPTS="-W -n --keep-going -E"
	@echo "open file://`pwd`/docs/_build/html/index.html"

doc-spelling:
	@make -C docs spelling SPHINXOPTS="-W -n --keep-going -E"

install:
	@pip install -U 'pip'
	@pip install -Ur requirements/dev.txt

install-dev: .develop


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
	rm -f multidict/*.html
	rm -f multidict/*.so
	rm -f multidict/*.pyd
	rm -rf .tox
