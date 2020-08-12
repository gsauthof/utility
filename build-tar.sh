#!/bin/bash

set -eux

outdir=$PWD
if [ $# -gt 0 ]; then
    outdir=$1
fi

for i in libixxx libixxxutil ; do
    echo $i
    pushd $i
    git archive --prefix gms-utils/$i/ -o "$outdir/$i.tar" HEAD
    popd
done
git archive --prefix gms-utils/ -o "$outdir/main.tar" HEAD

pushd "$outdir"
for i in {main,libixxx,libixxxutil}.tar; do
    tar xf $i
done
tar cfz gms-utils.tar.gz gms-utils
rm {main,libixxx,libixxxutil}.tar
rm -rf gms-utils


