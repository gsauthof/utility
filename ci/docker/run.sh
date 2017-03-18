#!/bin/bash

set -eux

cd /srv/build

# the overlayfs docker 1.12.3/Ubuntu 14/Travis doesn't
# support O_TMPFILE (operation not supported)
# thus using the mapped FS - which is ext4
mkdir -p tmp
export TMPDIR=$PWD/tmp

stat -f -c %T .
ninja-build check
