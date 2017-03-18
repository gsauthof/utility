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

switch_container
