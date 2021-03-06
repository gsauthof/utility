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

# make sure to use the default gonzofilter DB
cd

base=$HOME/maildir/.Spam

temp=$(mktemp --directory --tmpdir="$base")

find  "$base"/{new,cur} -type f -print0 \
        | xargs --no-run-if-empty -0 mv -t "$temp"

if [ -z "$(find "$temp" -prune -empty)" ]; then
    find  "$temp" -type f -print0 | xargs -0rl1 gonzofilter -spam -in
fi
