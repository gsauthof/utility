#!/bin/bash

# SPDX-FileCopyrightText: © 2020 Georg Sauthoff <mail@gms.tf>
# SPDX-License-Identifier: GPL-3.0-or-later

function help
{
    cat <<EOF
addrof - list IP address(es) of a network device

call: addrof [OPT...] DEVICE

Options:

    -4             only list IPv4 addresses
    -6             only list IPv6 addresses
    -a, --all      list all IP addresses (i.e. not just global ones)
    -p, --primary  list only primary IP addresses
    -h, --help     display this help and exit
EOF
}


dev=
addrflag=
primaryflag=
scopeflag="scope global"


function parse_args
{
    local t
    t=$(getopt -o ap46h --long all,primary,help -n addrof -- "$@")

    if [ $? -ne 0 ]; then
        echo 'Option parse error' >&2
        exit 1
    fi

    eval set -- "$t"

    while :; do
        case "$1" in
            -a|--all)
                scopeflag=
                shift
                ;;
            -p|--primary)
                primaryflag=primary
                shift
                ;;
            -4)
                addrflag=-4
                shift
                ;;
            -6)
                addrflag=-6
                shift
                ;;
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
    dev=$1
}

parse_args "$@"

ip $addrflag -o addr show dev "$dev" up $primaryflag $scopeflag \
    | awk -F' +|/' '{ print $4}'

