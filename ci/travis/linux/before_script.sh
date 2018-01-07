#!/bin/bash

set -ex

. "${BASH_SOURCE%/*}"/config.sh


function switch_container
{
  if [ "$docker_img" -a "$docker_img_b" ] ; then
    docker stop devel
    docker rm devel

    docker create --name devel  \
      -v "$src":/srv/src:ro,Z \
      -v "$build":/srv/build:Z \
      "$docker_img_b"

    docker start devel
  fi
}

function compile
{
  mkdir build
  cd build
  cmake -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE ..
  make
}

if [ "$docker_img" ]; then
  switch_container
else
  compile
fi
