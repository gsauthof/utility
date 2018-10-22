#!/usr/bin/env python3

# pldd - print shared libraries loaded into a running process
#
# inspired by Solaris' pldd
#
# 2018, Georg Sauthoff <mail@gms.tf>
#
# SPDX-License-Identifier: GPL-3.0-or-later

import argparse
import os
import sys

def parse_args(*a):
  p = argparse.ArgumentParser(
          description='print shared libraries loaded into a running process')
  p.add_argument('pids', metavar='PID', type=int, nargs=1,
      help='process identifier (PID)')
  return p.parse_args(*a)

def pldd(pid):
    exe = os.readlink('/proc/{}/exe'.format(pid))
    xs = []
    with open('/proc/{}/maps'.format(pid)) as f:
        for line in f:
            row = line.split()
            if row[1].endswith('r-xp'):
                i = line.find('/')
                if i > 0:
                    name = line[i:-1]
                    if name != exe:
                        xs.append(name)
    xs.sort()
    print('\n'.join(xs))

def main():
    args = parse_args()
    pid = args.pids[0]
    return pldd(pid)

if __name__ == '__main__':
    sys.exit(main())

