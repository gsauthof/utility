#!/bin/bash

set -ex


cd /srv/build
cmake -G Ninja -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" /srv/src
ninja-build $targets


