#!/bin/bash

# Wrapper around Solaris Studio's DBX runtime checking feature.
#
# It is similar to the `bcheck` utility that is bundled with Solaris
# Studio, although this wrapper prints additional information
# and doesn't redirect the messages to a file.
#
# 2017, Georg Sauthoff <mail@gms.tf>, WTFPL

dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

prog=$1
shift

exec dbx -c 'runargs '"$*"'; source '"$dir"'/check.dbx; rcheck' "$prog"
