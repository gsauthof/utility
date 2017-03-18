#!/bin/bash

set -ex


# Docker 17.03 has the --env option, e.g.
# --env CMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE"
# but Travis currently is at 1.12.3 which doesn't have it

docker exec devel env \
  CMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
  targets="$targets" \
  /srv/src/ci/docker/build.sh
