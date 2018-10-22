#!/bin/bash

# pldd - print shared libraries loaded into a running process
#
# inspired by Solaris' pldd
# and the NOTES Section of glibc's pldd(1) man page
# (note that glibc's pldd is broken for years)
#
# 2018, Georg Sauthoff <mail@gms.tf>
#
# SPDX-License-Identifier: GPL-3.0-or-later

set -eu

exe=$0

function help
{
    cat >&"$1" <<EOF
usage: $exe [-h] PID

print shared libraries loaded into a running process

positional arguments:
  PID         process identifier (PID)

optional arguments:
  -h, --help  show this help message and exit
EOF
    if [ "$1" -eq 1 ]; then
        exit 0
    else
        exit 1
    fi
}

if [ $# -ne 1 ]; then
    help 2
fi
if [ "$1" = -h -o "$1" = --help ]; then
    help 1
fi
if [ $(echo "$1" | tr -c -d '0-9') != "$1" ]; then
    help 2
fi

pid=$1

exec gdb -nh -n -q -ex 'set confirm off' -ex 'set height 0' \
    -ex 'info shared' -ex quit \
    -p "$pid" \
    | awk '$1 !~ /^0x/ {p=0} p { print $NF} $1=="From" {p=1}' \
    | sort

