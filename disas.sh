#!/bin/bash

# disas - disassemble single functions
#
# 2020, Georg Sauthoff <mail@gms.tf>
#
# SPDX-License-Identifier: GPL-3.0-or-later

bin=
query_prefix='<'
query=
query_suffix='>:'
use_gdb=
show_next=0
dump=${OBJDUMP:-objdump}

function parse_args
{
    local t
    t=$(getopt -o acgnd:h --long address,call,gdb,next,help -n disas -- "$@")

    if [ $? -ne 0 ]; then
        echo 'Option parse error' >&2
        exit 1
    fi

    eval set -- "$t"

    while :; do
        case "$1" in
            -a|--address)
                query_prefix='\n *'
                query_suffix=':'
                shift
                ;;
            -c|--call)
                query_prefix='<'
                query_suffix='[@>]'
                shift
                ;;
            -g|--gdb)
                use_gdb=1
                dump=${GDB:-gdb}
                shift
                ;;
            -n|--next)
                show_next=1
                shift
                ;;
            -d)
                dump=$2
                shift 2
                ;;
            -h|--help)
                cat <<EOF
disas - disassemble certain functions

call: disas [OPT...] BINARY FUNCNAME|ADDRESS

The funcname/address might be a regular expression.
For x86 binaries, objdump is instructed to emit Intel syntax.

Options:

    -a, --address  disassemble function that includes this address
    -c, --call     also disassemble functions that call this function
    -g, --gdb      use gdb instead of objdump for disassembling,
                   useful when a symbol table is in the .gnu_debugdata
                   section (cf. MiniDebugInfo) or there is debuginfo
                   in separate files
    -h, --help     display this help and exit
    -n, --next     also display the next adjacent symbol name,
                   useful for identifying premature dissambles caused by
                   emitted local symbols
    -d PATH        use specific objdump/gdb binary

Environment variables:

    GDB            alternative gdb executable
    OBJDUMP        alternative objdump executable
    ODFLAGS        additional objdump flags
EOF
                exit 0
                shift
                ;;
            --)
                shift
                break
                ;;
        esac
    done
    if [ $# -lt 2 ]; then
        echo 'binary/func name missing' >&2
        exit 1
    fi
    bin=$1
    shift
    query=$1
}


function main
{
    parse_args "$@"
    if [ "$use_gdb" ]; then
        "$dump" --batch -q -ex 'set height 0' -ex 'disas '"$query" "$bin" \
            | head -n -1
    else
        local intel_flags
        if file -b "$bin" | grep '\(80386\|x86-64\),' > /dev/null; then
            intel_flags='-M intel'
        fi
        "$dump" $intel_flags -dC $ODFLAGS "$bin" \
            | awk -F'\n' -vRS='\n\n' /"$query_prefix$query$query_suffix"/' {
                  q=1; print; next }  q && 1=='"$show_next"' { print $1 } q { q=0 }'
    fi
}

main "$@"
