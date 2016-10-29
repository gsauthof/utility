#!/bin/bash

# Check if the currently running kernel is latest available one.
#
# Tested on Fedora 24/CentOS 7.
#
# 2014, Georg Sauthoff <mail@georg.so>

set -e
set -u

latest=$(find /boot -maxdepth 1 -name 'vmlinuz-*' \
                           -not -name '*rescue*' -printf '%f\n'\
  | sed 's/vmlinuz-\([0-9.-]\+\)\.[A-Za-z].*$/\1/' \
  | sort -V -r | head -n 1)

running=$(uname -r | sed 's/\([0-9.-]\+\)\.[A-Za-z].*$/\1/')

verbose=false
if [ $# -ge 1 ]; then
  if [ "$1" = "-v" ]; then
    verbose=true
  fi
fi

if [ "$latest" != "$running" ]; then
  if [ "$verbose" = true ]; then
    echo We are running vmlinuz-"$running",\
 but the most current installed kernel is "$latest".
  fi
  exit 1
fi
