#!/bin/bash

# SPDX-License-Identifier: GPL-3.0-or-later

function help
{
cat <<EOF
password generator that creates secure and easy to communicate passwords

call: $0
      $0 [OPTS] [#WORDS]

Example password: kingdoms-racists-sociologist-mute

See http://xkcd.com/936/ for the general idea.

This password generator creates a password via randomly
selecting words from a dictionary. By default 4 words are
selected.

Easy to communicate means that the password is easy to write down
and easy to phrase over a (secure!) telephone line (no marking of
characters like O vs. 0, l vs. I vs. 1, ..., no spelling of
single characters necessary). As a consequence, a short sequence
of real words is easier to remember than a long sequence of
random alphanumeric characters.

With an - say - english dictionary of 119773 words the generated
password has an entropy of log2(119773^4)~=67 bits. In contrast
to that a randomly generated alphanumeric password of 11
characters has log2((26*2+10)^11)~=64 bits of entropy.

The generated password is printed to stdout, while its entropy is
printed to stderr.

Options:

   -c            don't use a dict, just random alphanumeric characters
                 (#WORDS is number of characters then)
   -f FILENAME   wordlist filename (default: ~/.cache/words.gz)
   -h            show this help (more times for more help)
   -m N          specify minium #bits entropy (default: N=60)
   -q            don't print entropy message to stderr
   -w N          number of words (default: 4; the -c default: 12)

2016-2019, Georg Sauthoff <mail@gms.tf>, GPLv3+

EOF
if [ $1 -gt 1 ]; then
  add_help
fi
}


function add_help
{
cat <<EOF

##  Entropy Examples

    method                bits        expr
    -------------------+-----------+--------------------
     8 char alphnum        47         log2((26*2)+10)^8)
     3 word dict           50         log2(119773^3)
     9 char alphnum        53         log2((26*2)+10)^9)
    10 char alphnum        59         log2((26*2)+10)^10)
    11 char alphnum        65         log2((26*2)+10)^11)
     4 word dict           67         log2(119773^4)
    12 char alphnum        71         log2((26*2)+10)^12)
    13 char alphnum        77         log2(((26*2)+10)^13)
    14 char alphnum        83         log2(((26*2)+10)^14)
     5 word dict           84         log2(119773^5)
    15 char alphnum        89         log2(((26*2)+10)^15)
    16 char alphnum        95         log2(((26*2)+10)^16)
     6 word dict          101         log2(119773^6)



## Related Generators

### pwgen (http://sf.net/projects/pwgen)

Generates 'to be easily memorized' passwords that are 8
characters long, by default. The easy to memorize property thus
yields an entropy less than log2((26*2+10)^8)~=47 bits.

Example password: ooye0iSh

If those passwords are easy to remember depends on the used
definition of easy-to-remember.

### pwqgen (http://www.openwall.com/passwdqc/)

Generates 'quality controllable random passphrase[s]' with 47
bits of entropy, by default. Supports up to 85 bits of entropy.

Example: Home2Visual&Pizza

This goes into the right direction. A combination of words is
easy to communicate. The special characters and the the upper
case characters might be there to satisfy password policies but
otherwise they make it harder to remember.

### pwmake (http://libpwquality.fedorahosted.org/)

Generates 'random relatively easily pronounceable passwords'. It
has a minimum of 56 bits of entropy.

The man page contains some recommendations regarding the level of
entropy.

Example: ]4h#0J#@zEG#

It is easy to pronounce if one considers a random selection of
printable characters including left-bracket, hash, ampersand etc.
easy to pronounce.

2016, Georg Sauthoff <mail@georg.so>
EOF
}

set -eu

function check_entropy
{
  local entropy=$1
  if [ $quiet -eq 0 ]; then
    echo "$entropy bits of entropy" >&2
  fi
  if [ "$entropy" -lt "$min_entropy" ]; then
    echo "not enough entropy ($min_entropy)\
- use bigger dictionary and/or more words" >&2
    exit 1
  fi
}

function gen_nodict
{
  entropy=$(echo "l((26*2+10)^$words)/l(2)" | bc -l | sed 's/\..*$//')
  check_entropy $entropy

  dd if=/dev/urandom status=none | LC_ALL=C tr -d -c 'A-Za-z0-9' \
    | dd bs=1 count=$words status=none
  echo
}

function parse_args
{
  if [ $# -gt 0 ]; then
    if [ "$1" = '--help' ]; then help 1; exit 0; fi
  fi
  quiet=0
  use_dict=1
  use_help=0
  if command -v "$shuf" > /dev/null ; then : ; else shuf=""; fi
  while getopts "cf:hm:nqw:" arg; do
    case $arg in
      c)
        words=12
        use_dict=0
        ;;
      f)
        wfile=$OPTARG
        ;;
      h)
        use_help=$((use_help+1))
        ;;
      m)
        min_entropy=$OPTARG
        ;;
      n)
        shuf=""
        ;;
      q)
        quiet=1
        ;;
      w)
        words=$OPTARG
        ;;
      ?)
        exit 1
        ;;
    esac
  done
  if [ $OPTIND -eq $# -o $OPTIND -lt $# ]; then
    words=${!OPTIND}
  fi
  if [ $use_help -gt 0 ]; then
    help $use_help
    exit 0
  fi
}

function init_wordlist
{
    if [ -f "$wfile" ]; then
        return
    fi
    if command -v "$aspell" > /dev/null ; then
        for i in $lang; do
            "$aspell" -d "$i" dump master | "$aspell" -l $i expand
        done | "$sort" -u  | gzip > "$wfile"
    elif [ -f "/usr/share/myspell/en_US.dic" ]; then
        for i in $lang; do
            sed 's@/.*$@@' /usr/share/myspell/"$i"*.dic
        done | "$sort" -u | gzip > "$wfile"
    fi

}

function gen_dict
{
    init_wordlist

    local n=$(zcat "$wfile" | wc -l)
    if [ "$n" -eq 0 ]; then
      echo "Wordlist $wfile is empty" >&2
      exit 1
    fi

    local entropy=$(echo "l($n^$words)/l(2)" | bc -l | sed 's/\..*$//')
    check_entropy "$entropy"

    if [ -n "$shuf" ]; then
      zcat "$wfile" | "$shuf" --random-source /dev/urandom -r -n $words \
        | "$awk" '{print(tolower($0))}' \
        | tr -d "'" \
        | "$paste" -sd '-'
    else
      # NB: 1048576 = 2**20
      if [ "$n" -ge 1048576 ]; then
          echo "Wordlist $wfile is too large" >&2
          exit 1
      fi
      for i in `seq 1 $words`; do
        zcat "$wfile" \
          | "$sed" -n $(< /dev/urandom od  -tu4 -An -v  -w4 | awk '{ $1 = $1 % 1048576 }  $1 < '$n' { print $1+1; exit; }')p \
          | "$awk" '{print(tolower($0))}' \
          | tr -d "'"
      done | "$paste" -sd '-'
    fi
}


: ${aspell:=aspell}
: ${awk:=awk}
: ${lang:=en}
min_entropy=60
# GNU coreutils paste
: ${paste:=paste}
: ${sed:=sed}
# GNU coreutils shuf
: ${shuf:=shuf}
: ${sort:=sort}
: ${wfile:=~/.cache/words.gz}
words=4

function main
{
    parse_args "$@"
    if [ $use_dict -ne 0 ]; then
        gen_dict
    else
        gen_nodict
    fi
}

main "$@"

