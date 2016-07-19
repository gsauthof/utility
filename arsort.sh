#!/bin/bash

if [ "$1" = '-h' -o "$1" = '--help' ]; then
  cat <<EOF
call: $0 libfoo.a libbar.a ...

Topilogically sort a bunch of static libraries.

2012, Georg Sauthoff <mail@georg.so>
EOF

  exit 0
fi

set -e
set -u

# in general, assuming POSIX and/or GNU versions
# e.g. on Solaris 10, the nm comes from /usr/ccs/bin and
# the rest should come from /opt/csw/gnu
# should be portable, because nm is used with the -P option
: ${awk:=awk}
: ${grep:=grep}
: ${nm:=nm}
: ${sort:=sort}
: ${tac:=tac}
: ${tr:=tr}
: ${tsort:=tsort}

for i in "$@"; do

  if [ "${i%.a}" = "$i" ]; then continue; fi

  "$nm" -P -g "$i"   \
      | "$awk" ' $2 ~ /[UT]/        {          print $1      , $2        }' \
      | "$sort" -u   \
      | "$sort" -k2  \
      | "$awk" ' $2 ~ /T/           { d[$1]=1; print "'"$i"'", $1        }
                 $2 ~ /U/ && !d[$1] {          print $1      , "'"$i"'"  }' \

done  | "$tsort"      \
      | "$tac"        \
      | "$grep" '\.a' \
      | "$tr"    '\n' ' '
