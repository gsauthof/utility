#!/bin/bash

set -ex

if [ -z "$docker_img" ]; then
  exit 0
fi

# Docker 17.03 has the --env option, e.g.
# --env CMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE"
# but Travis currently is at 1.12.3 which doesn't have it
#
# -> as of 2018-01, Travis Trusty (Ubuntu 12) is at
# docker 17.09.0.ce

docker exec --user root devel dnf -y install python3-distro
docker exec devel env \
  CMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
  targets="$targets" \
  /srv/src/ci/docker/build.sh
