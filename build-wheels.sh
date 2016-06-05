#!/bin/bash
PYTHON_VERSIONS="cp34-cp34m cp35-cp35m"

# Compile wheels
for PYTHON in ${PYTHON_VERSIONS}; do
    /opt/python/${PYTHON}/bin/pip install -r /io/requirements-wheel.txt
    /opt/python/${PYTHON}/bin/pip wheel /io/ -w /io/dist/
done

# Bundle external shared libraries into the wheels
# for whl in wheelhouse/*.whl; do
#     auditwheel repair $whl -w /io/dist/
# done

# Install packages and test
for PYTHON in ${PYTHON_VERSIONS}; do
    /opt/python/${PYTHON}/bin/pip install multidict --no-index -f file:///io/dist
    rm -rf /io/tests/__pycache__
    /opt/python/${PYTHON}/bin/py.test /io/tests
    rm -rf /io/tests/__pycache__
done
