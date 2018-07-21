# Some simple testing tasks (sorry, UNIX only).

all: test


flake:
	tox -e flake8


rmcache:
	rm -rf tests/__pycache__


mypy:
	tox -e mypy


test:
	tox


test-all:
	tox -e,


vtest:
	tox -- -vv


qtest:
	tox -- -q


cov-dev: mypy
	tox -e profile-dev -- --cov-report=html
	@which xdg-open 2>/dev/null 1>&2 && export opener=xdg-open || export opener=open && \
	$${opener} "file://`pwd`/htmlcov/index.html"

cov-dev-full: mypy
	MULTIDICT_NO_EXTENSIONS=1 tox -e profile-dev -- --cov-report=html
	tox -e profile-dev -- --cov-report=html
	@which xdg-open 2>/dev/null 1>&2 && export opener=xdg-open || export opener=open && \
	$${opener} "file://`pwd`/htmlcov/index.html"

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
	rm -f multidict/_pair_list.*.so
	rm -f multidict/_pair_list.*.pyd
	rm -f multidict/_multidict_iter.*.so
	rm -f multidict/_multidict_iter.*.pyd
	rm -rf .tox

doc:
	tox -e doc-html
	@which xdg-open 2>/dev/null 1>&2 && export opener=xdg-open || export opener=open && \
	$${opener} "file://`pwd`/docs/_build/html/index.html"

doc-spelling:
	tox -e doc-spelling

install:
	pip install -U tox
	pip install -U pip
	pip install -Ur requirements/dev.txt

.PHONY: all build venv flake test test-all vtest qtest cov clean doc
