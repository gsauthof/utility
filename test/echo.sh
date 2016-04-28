#!/bin/bash


echo "$1" >&$2

[[ $# > 3 ]] && sleep $4

exit $3
