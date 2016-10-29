#!/bin/bash

set -e
set -u

# Script trains and deletes Spam messages which are
# located in a base directory.
# The base directory is filled e.g. by marking messages as SPAM
# in an IMAP client.
# Can be called from cron.
#
# 2014-12-01, Georg Sauthoff <mail@georg.so>

# Assuming GNU versions of find, xargs, mktemp ...

temp=""

function cleanup()
{
  rm -rf "$temp"
}
trap cleanup EXIT

base=$HOME/maildir/.Spam
: ${bogofilter:=bogofilter}

temp=$(mktemp --directory --tmpdir="$base")

find  "$base"/{new,cur} -type f -print0 \
  | xargs --no-run-if-empty -0 mv -t "$temp"

if [ -z "$(find "$temp" -prune -empty)" ]; then
  #ls -l "$temp"
  echo "$bogofilter" -B "$temp" -s -v
fi
