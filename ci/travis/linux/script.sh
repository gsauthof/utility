#!/bin/bash

set -eux

if [ "$docker_img" ]; then
  docker exec devel \
    /srv/src/ci/docker/run.sh
else
  # PWD -> build
  make check
fi
