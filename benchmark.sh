#!/bin/bash

# 2016, Georg Sauthoff <mail@georg.so>

set -eu

time=/usr/bin/time
Rscript=Rscript

f=$(mktemp)

function cleanup
{
  rm -f "$f"
}
trap cleanup EXIT

if [ "$1" = -h -o "$1" = --help ]; then
  cat <<EOF
Usage: $0 REPEATS COMMAND ARGS..
       $0 [OPTIONS] REPEATS COMMAND ARGS..

Options:

-s SECONDS    seconds to sleep after each iteration

EOF
fi
s=0
if [ "$1" = -s ]; then
  shift
  s=$1
  shift
fi
n=$1
shift

echo wall,user,sys,rss > "$f"
for i in $(seq $n); do
  "$time" --output "$f" --append --format '%e,%U,%S,%M' "$@" >/dev/null
  if [ "$s" -gt 0 ]; then
    sleep "$s"
  fi
done

{
echo "n="$n" min Q1 med mean Q3 max std"
"$Rscript" --vanilla -e "b=read.csv(file='$f');summary(b);sapply(b, sd);q()" \
  | tr ': ' '\n'  \
  | grep '^[=.0-9]\+$' \
  | awk 'BEGIN { b[0]="wall"; b[1]="user"; b[2]="sys"; b[3]="rss" }
               { a[(NR-1)%4] = a[(NR-1)%4]$0" " }
           END {for (i=0; i<4; ++i)
                  print(b[i]" "a[i]) } '
} | column  -t  -o ' | '


