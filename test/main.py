#!/usr/bin/env python3

import unittest
import sys
import os


if __name__ == '__main__':

  if 'src_dir' not in os.environ:
    os.environ['src_dir'] = '.'
  if 'bin_dir' not in os.environ:
    os.environ['bin_dir'] = '.'
  src_dir = os.environ['src_dir']
  bin_dir = os.environ['bin_dir']
  for i in [ 'chronic', 'fail' ]:
    if i not in os.environ:
      os.environ[i] = bin_dir + '/' + i
  if 'echo' not in os.environ:
    os.environ['echo'] = src_dir + '/test/echo.sh'

  unittest.main(module=None, argv=[sys.argv[0], 'discover',
      '--start-directory',  src_dir + '/test', '--pattern', '*.py',
      '--top-level-directory', src_dir])


