#!/usr/bin/env python3

import unittest
import logging

import subprocess
import tempfile
import shutil
import os
import sys

# cf. test/main.py for the global defaults
bin_dir = os.getenv('bin_dir', os.getcwd())
src_dir = os.getenv('src_dir', os.getcwd()+'/..')
arsort = src_dir + '/arsort.sh'

class Basic(unittest.TestCase):

  def setUp(self):
    self.base_dir = tempfile.mkdtemp()
    self.log = logging.getLogger(__name__)
    self.log.debug('using tempdir: ' + self.base_dir)
    self.old_dir = os.getcwd()
    os.chdir(self.base_dir)

  def tearDown(self):
    shutil.rmtree(self.base_dir)
    self.base_dir = None
    os.chdir(self.old_dir)

  def create_ar(self, name, text):
    with open(name + '.c', 'w') as f:
      f.write(text)
    subprocess.check_output(['cc', '-c', name+'.c'], stderr=subprocess.STDOUT)
    subprocess.check_output(['ar', 'r', name+'.a', name+'.o'],
            stderr=subprocess.STDOUT)

  def test_sort(self):
    self.create_ar('zoo', '''void zoo()
{
  puts("zoo");
  yar();
}''')
    self.create_ar('yar', '''void yar()
{
  puts("yar");
  xaz();
}''')
    self.create_ar('xaz', '''void xaz()
{
  puts("xaz");
}''')
    r = subprocess.check_output([arsort, 'zoo.a', 'yar.a', 'xaz.a'])
    self.assertEqual(r, b'zoo.a yar.a xaz.a ')
    s = subprocess.check_output([arsort, 'xaz.a', 'yar.a', 'zoo.a'])
    self.assertEqual(s, b'zoo.a yar.a xaz.a ')
    t = subprocess.check_output([arsort, 'zoo.a', 'xaz.a', 'yar.a'])
    self.assertEqual(t, b'zoo.a yar.a xaz.a ')



if __name__ == '__main__':
  logging.basicConfig(stream=sys.stderr)
  log = logging.getLogger(__name__)
  log.setLevel(logging.DEBUG)
  unittest.main()


