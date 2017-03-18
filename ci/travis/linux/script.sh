#!/bin/bash

set -eux

docker exec devel \
  /srv/src/ci/docker/run.sh
