#!/usr/bin/env python3

# 2018, Georg Sauthoff <mail@gms.tf>
# SPDX-License-Identifier: GPL-3.0-or-later

import mmap
import sys

def map_file(filename):
  with open(filename, 'rb', buffering=0) as f:
    try:
      b = mmap.mmap(f.fileno(), 0, prot=mmap.PROT_READ)
      return b
    except ValueError:
      return bytes()

def main(qfilename, filename):
  q, t = (map_file(x) for x in (qfilename, filename))
  i = t.find(q)
  if i == -1:
    return 1
  print(i)

if __name__ == '__main__':
  sys.exit(main(sys.argv[1], sys.argv[2]))
