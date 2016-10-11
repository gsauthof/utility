#!/bin/bash

# 2016, Georg Sauthoff <mail@georg.so>

if [ "$1" = -h -o "$1" = --help -o $# -lt 1 ]; then
  cat <<EOF
$0 -o outfile CMD [ARGS..]

Wraps 'perf stat' such that a single line of comma seperated values
is printed.

Header:

nsec,cswitch,cpu_migr,page_fault,cycles,ghz,ins,ins_cyc,br,br_mis,br_mis_rate
EOF
  if [ $# -lt 1 ]; then exit 1; else exit 0; fi
fi

if [ "$1" = -o -o $1 = --output ]; then
  shift
  ofile=$1
  shift
else
  echo 'no -o filename specified' >&2
  exit 1
fi

set -eu

set -o pipefail

function cleanup
{
  rm -f "$tfile"
#  echo "$tfile" >&2
}
trap cleanup EXIT

tfile=$(mktemp)


end_clause='END {
    OFS=",";
    # CentOS 7 perf does not print those with -x ...
    if (!a["ghz"])
      a["ghz"]=a["cycles"]/a["nsec"];
    if (!a["ins_cyc"])
      a["ins_cyc"]=a["ins"]/a["cycles"];
    if (!a["br_mis_rate"])
      a["br_mis_rate"]=a["br_mis"]/a["br"]*100.0;

    print a["nsec"],a["cswitch"],a["cpu_migr"],a["page_fault"],a["cycles"],a["ghz"],a["ins"],a["ins_cyc"],a["br"],a["br_mis"],a["br_mis_rate"]
  }
'

function perfstat_Linux
{
perf stat -x, -o "$tfile" "$@"

awk -F, '
  # on Fedora 23 the value have the :u suffix, on CentOS they do not
  $3 ~ /^task-clock(:u)?$/       { a["nsec"]=$4;                        next }
  $3 ~ /^context-switches(:u)?$/ { a["cswitch"]=$1;                     next }
  $3 ~ /^cpu-migrations(:u)?$/   { a["cpu_migr"]=$1;                    next }
  $3 ~ /^page-faults(:u)?$/      { a["page_fault"]=$1;                  next }
  $3 ~ /^cycles(:u)?$/           { a["cycles"]=$1;     a["ghz"]=$6;     next }
  $3 ~ /^instructions(:u)?$/     { a["ins"]=$1;        a["ins_cyc"]=$6; next }
  $3 ~ /^branches(:u)?$/         { a["br"]=$1;                          next }
  $3 ~ /^branch-misses(:u)?$/    { a["br_mis"]=$1;     a["br_mis_rate"]=$6;  }
  '"$end_clause" "$tfile" > "$ofile"
}

function perfstat_SunOS
{
  cputrack -o "$tfile" -c Cycles_user,PAPI_tot_ins,PAPI_br_cn,PAPI_br_msp "$@"
  awk '
  $3 == "exit" {
    a["nsec"]=$1*1000000000;
    a["cycles"]=$4;
    a["ins"]=$5;
    a["br"]=$6;
    a["br_mis"]=$7;
  }
  '"$end_clause" "$tfile" > "$ofile"
}

sys=$(uname -s)
perfstat_"$sys" "$@"

