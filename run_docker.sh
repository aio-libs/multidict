if [ -z $MULTIDICT_NO_EXTENSIONS ]; then
    docker pull quay.io/pypa/manylinux1_x86_64
    docker run --rm -v `pwd`:/io quay.io/pypa/manylinux1_x86_64 /io/build-wheels.sh

    docker pull quay.io/pypa/manylinux1_i686
    docker run --rm -v `pwd`:/io quay.io/pypa/manylinux1_i686 linux32 /io/build-wheels.sh
    echo "Dist folder content is:"
    ls dist
fi

if [ ! -z $TRAVIS_TAG ]; then
    echo "Upload dists to PyPI"
    ls
    for f in dist/*.whl
    do
        if [[ f == *"linux_x86_64"* ]]
        then
            docker run --rm -v `pwd`:/io auditwheel quay.io/pypa/manylinux1_x86_64 repair $f
        fi
        if [[ f == *"linux_i686"* ]]
        then
            docker run --rm -v `pwd`:/io auditwheel quay.io/pypa/manylinux1_i686 linux32 repair $f
        fi
        python -m twine upload $f --username andrew.svetlov --password $PYPI_PASSWD
    done
fi


if [ -z $MULTIDICT_NO_EXTENSIONS ]; then
    docker run --rm -v `pwd`:/io quay.io/pypa/manylinux1_x86_64 rm -rf /io/dist
fi
