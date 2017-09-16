#!/bin/bash
package_name="$1"
if [ -z "$package_name" ]
then
    &>2 echo "Please pass package name as a first argument of this script ($0)"
    exit 1
fi

manylinux1_image_prefix="quay.io/pypa/manylinux1_"
declare -A docker_pull_pids=()  # This syntax requires at least bash v4

for arch in x86_64 i686
do
    docker pull "${manylinux1_image_prefix}${arch}"
done
echo
echo "===================== Images pull done ======================"
echo
echo

for arch in x86_64 i686
do
    if [ $arch == "i686" ]; then
      dock_ext_args="linux32"
    else
      dock_ext_args=""
    fi

    echo Building wheel for $arch arch
     docker run --rm -v `pwd`:/io "${manylinux1_image_prefix}${arch}" $dock_ext_args /io/tools/build-wheels.sh "$package_name"
done
