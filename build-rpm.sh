#!/bin/bash

set -eux

outdir=$PWD
if [ $# -gt 0 ]; then
    outdir=$1
fi

rpmbuild --without srpm --define "_sourcedir $PWD" \
    --define "_specdir $PWD" --define "_builddir $PWD" \
    --define "_rpmdir $outdir" --define "_srcrpmdir $outdir" \
    -bb gms-utils.spec
