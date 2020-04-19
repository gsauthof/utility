#!/bin/bash

# SPDX-FileCopyrightText: © 2020 Georg Sauthoff <mail@gms.tf>
# SPDX-License-Identifier: GPL-3.0-or-later

function help
{
    cat <<EOF
devof - list name(s) of network device(s) that use the given address (prefix)

call: devof [OPT...] ADDRESS|ADRESS/PREFIXLEN

Options:

    -h, --help     display this help and exit
EOF
}


addr=


function parse_args
{
    local t
    t=$(getopt -o h --long help -n addrof -- "$@")

    if [ $? -ne 0 ]; then
        echo 'Option parse error' >&2
        exit 1
    fi

    eval set -- "$t"

    while :; do
        case "$1" in
            -h|--help)
                help
                exit 0
                shift
                ;;
            --)
                shift
                break
                ;;
        esac
    done
    if [ $# -lt 1 ]; then
        echo 'device name missing' >&2
        exit 1
    fi
    addr=$1
}

parse_args "$@"

ip -o addr show up  to "$addr" | cut -d' ' -f2
