#!/bin/bash

# List disk device for each ataX identifier
#
# cf. https://unix.stackexchange.com/q/13960/1131
#
# 2017, Georg Sauthoff <mail@gms.tf>

set -eu

echo /sys/class/ata_port/ata*/../../host*/target*/*/block/s* \
    | tr ' ' '\n' \
    | awk -F/ '{printf("%s => /dev/%s\n", $5, $NF)}'
