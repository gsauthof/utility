#!/bin/bash

set -eux

. "${BASH_SOURCE%/*}"/config.sh

: ${docker_flags:=}

function setup_dirs
{
  mkdir -p "$build"
  chmod 770 "$build"/..
  chmod 777 "$build"
}

# the Docker 1.12.3 under Ubuntu 14/Trusty on Travis
# uses aufs, by default - we want overlayfs instead
# because it is part of the kernel, aufs is abandoned
# and aufs doesn't support renameat2(..., RENAME_EXCHANGE)
function configure_overlayfs
{
  pwd; id; uname -a

  sudo service docker stop
  ls -l /etc/default/docker
  sudo cat /etc/default/docker
  sudo bash -c 'echo "DOCKER_OPTS=\"--storage-driver=overlay\"" \
    >> /etc/default/docker'
  sudo service docker start
}

function start_docker
{
  docker create --name devel \
    -v "$src":/srv/src:ro,Z \
    -v "$build":/srv/build:Z \
    $docker_flags \
    $docker_img

  docker start devel

  sleep 4

  docker ps
}

configure_overlayfs
setup_dirs
start_docker


