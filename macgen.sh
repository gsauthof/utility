#!/bin/bash

# Randomly generate a locally administered unicast MAC address.
#
# 2017-01, Georg Sauthoff <mail@gms.tf>

od -An -N6 -tx1 /dev/urandom \
  | tr ' ' ':' \
  | sed 's/^.\(.\)./\1'$(shuf -e 2 -e 6 -e a -e e  -n1)'/'

