#!/usr/bin/env python3

import unittest

import subprocess
import timeit

import time
import os

import tempfile
import shutil

bin_dir = os.getenv('bin_dir', '.')
swap    = os.getenv('swap', bin_dir + '/swap')

class Basic(unittest.TestCase):

  def setUp(self):
    self.base_dir = tempfile.mkdtemp()

  def tearDown(self):
    shutil.rmtree(self.base_dir)
    self.base_dir = None


  def test_swap(self):
    with open(self.base_dir + '/foo', 'w') as f, \
         open(self.base_dir + '/bar', 'w') as g:
      print('foo', file=f)
      print('bar', file=g)

      self.assertNotEqual(os.fstat(f.fileno()).st_ino,
          os.fstat(g.fileno()).st_ino)

      subprocess.check_output([swap, self.base_dir + '/foo',
        self.base_dir + '/bar'])
      with open(self.base_dir + '/foo', 'w') as f2, \
           open(self.base_dir + '/bar', 'w') as g2:

        self.assertEqual(os.fstat(f.fileno()).st_ino,
            os.fstat(g2.fileno()).st_ino)
        self.assertEqual(os.fstat(g.fileno()).st_ino,
            os.fstat(f2.fileno()).st_ino)

      subprocess.check_output([swap, self.base_dir + '/foo',
        self.base_dir + '/bar'])
      with open(self.base_dir + '/foo', 'w') as f2, \
           open(self.base_dir + '/bar', 'w') as g2:

        self.assertEqual(os.fstat(f.fileno()).st_ino,
            os.fstat(f2.fileno()).st_ino)
        self.assertEqual(os.fstat(g.fileno()).st_ino,
            os.fstat(g2.fileno()).st_ino)


