#!/bin/bash

set -ex

CFLAGS="-Wall -Wextra -Wno-missing-field-initializers \
    -Wno-parentheses -Wno-missing-braces \
    -Wmissing-prototypes -Wfloat-equal \
    -Wwrite-strings -Wpointer-arith -Wcast-align \
    -Wnull-dereference \
    -Werror=multichar -Werror=sizeof-pointer-memaccess -Werror=return-type \
    -fstrict-aliasing"
if [ "$CMAKE_BUILD_TYPE" = Release ]; then
    CFLAGS="-Og $CFLAGS"
fi
export CFLAGS

CXXFLAGS="-Wall -Wextra -Wno-missing-field-initializers \
    -Wno-parentheses -Wno-missing-braces \
    -Wno-unused-local-typedefs \
    -Wfloat-equal \
    -Wpointer-arith -Wcast-align \
    -Wnull-dereference \
    -Wnon-virtual-dtor -Wmissing-declarations \
    -Werror=multichar -Werror=sizeof-pointer-memaccess -Werror=return-type \
    -Werror=delete-non-virtual-dtor \
    -fstrict-aliasing"
if [ "$CMAKE_BUILD_TYPE" = Release ]; then
    CXXFLAGS="-Og $CXXFLAGS"
fi
export CXXFLAGS


cd /srv/build
cmake -G Ninja -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" /srv/src
ninja-build $targets


