#!/usr/bin/env python3

import unittest

import subprocess
import timeit

# fedora: python3-psutil
# -> https://github.com/giampaolo/psutil
import psutil
import threading
import signal
import time
import os

import tempfile
import shutil

# cf. test/main.py for the global defaults
lockf = os.getenv('lockf', './lockf')
fail = os.getenv('fail', './fail')
echo = os.getenv('echo', './echo.sh')


class Basic(unittest.TestCase):

  def setUp(self):
    self.base_dir = tempfile.mkdtemp()

  def tearDown(self):
    shutil.rmtree(self.base_dir)
    self.base_dir = None

  def test_wait(self):
    begin = timeit.default_timer()
    subprocess.check_output([lockf, '-c', self.base_dir + '/foo', 'sleep', '1'],
        stderr=subprocess.STDOUT)
    end = timeit.default_timer()
    self.assertTrue(end-begin >= 1)

  def test_true(self):
    begin = timeit.default_timer()
    subprocess.check_output([lockf, '-c', self.base_dir + '/foo', 'true'], stderr=subprocess.STDOUT)
    end = timeit.default_timer()
    # too slow for docker inside Travis-CI VM
    self.assertTrue(end-begin <= 0.07)

  def test_false(self):
    begin = timeit.default_timer()
    code = subprocess.call([lockf, '-c', self.base_dir + '/foo', 'false'])
    end = timeit.default_timer()
    self.assertTrue(end-begin <= 0.01)
    self.assertEqual(code, 1)

  def test_abort(self):
    code = subprocess.call([lockf, '-c', self.base_dir + '/foo', fail])
    # Coreutils flock also returns the exit code of the child
    self.assertEqual(code, 134)

  def test_block(self):
    begin = timeit.default_timer()
    p = subprocess.Popen([lockf, '-c', self.base_dir + '/foo', echo, 'x', '1', '0', '1'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.02)
    q = subprocess.Popen([lockf, '-c', self.base_dir + '/foo', '-b', echo, 'x', '1', '0', '1'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    p.communicate()
    self.assertEqual(p.returncode, 0);
    end = timeit.default_timer()
    self.assertTrue(abs((end-begin)-1) < 0.04)
    q.communicate()
    self.assertEqual(q.returncode, 0);
    end = timeit.default_timer()
    self.assertTrue(abs((end-begin)-2) < 0.04)

  def test_directly_fail(self):
    begin = timeit.default_timer()
    p = subprocess.Popen([lockf, '-c', self.base_dir + '/foo', echo, 'x', '1', '0', '1'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.02)
    q = subprocess.Popen([lockf, '-c', self.base_dir + '/foo', echo, 'x', '1', '0', '1'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    q.communicate()
    self.assertEqual(q.returncode, 1);
    end = timeit.default_timer()
    self.assertTrue(abs((end-begin)) < 0.04)

    p.communicate()
    self.assertEqual(p.returncode, 0);
    end = timeit.default_timer()
    self.assertTrue(abs((end-begin)-1) < 0.04)

  def test_block_flock(self):
    begin = timeit.default_timer()
    p = subprocess.Popen([lockf, '-l', '-c', self.base_dir + '/foo', echo, 'x', '1', '0', '1'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.02)
    q = subprocess.Popen([lockf, '-c', self.base_dir + '/foo', '-bl', echo, 'x', '1', '0', '1'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    p.communicate()
    self.assertEqual(p.returncode, 0);
    end = timeit.default_timer()
    self.assertTrue(abs((end-begin)-1) < 0.04)
    q.communicate()
    self.assertEqual(q.returncode, 0);
    end = timeit.default_timer()
    self.assertTrue(abs((end-begin)-2) < 0.04)

  def test_directly_fail_flock(self):
    begin = timeit.default_timer()
    p = subprocess.Popen([lockf, '-l', '-c', self.base_dir + '/foo', echo, 'x', '1', '0', '1'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.02)
    q = subprocess.Popen([lockf, '-l', '-c', self.base_dir + '/foo', echo, 'x', '1', '0', '1'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    q.communicate()
    self.assertEqual(q.returncode, 1);
    end = timeit.default_timer()
    self.assertTrue(abs((end-begin)) < 0.04)

    p.communicate()
    self.assertEqual(p.returncode, 0);
    end = timeit.default_timer()
    self.assertTrue(abs((end-begin)-1) < 0.04)

  def test_block_fcntl(self):
    begin = timeit.default_timer()
    p = subprocess.Popen([lockf, '-n', '-c', self.base_dir + '/foo', echo, 'x', '1', '0', '1'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.02)
    q = subprocess.Popen([lockf, '-c', self.base_dir + '/foo', '-bn', echo, 'x', '1', '0', '1'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    p.communicate()
    self.assertEqual(p.returncode, 0);
    end = timeit.default_timer()
    self.assertTrue(abs((end-begin)-1) < 0.04)
    q.communicate()
    self.assertEqual(q.returncode, 0);
    end = timeit.default_timer()
    self.assertTrue(abs((end-begin)-2) < 0.04)

  def test_directly_fail_fcntl(self):
    begin = timeit.default_timer()
    p = subprocess.Popen([lockf, '-n', '-c', self.base_dir + '/foo', echo, 'x', '1', '0', '1'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.02)
    q = subprocess.Popen([lockf, '-n', '-c', self.base_dir + '/foo', echo, 'x', '1', '0', '1'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    q.communicate()
    self.assertEqual(q.returncode, 1);
    end = timeit.default_timer()
    self.assertTrue(abs((end-begin)) < 0.1)

    p.communicate()
    self.assertEqual(p.returncode, 0);
    end = timeit.default_timer()
    self.assertTrue(abs((end-begin)-1) < 0.04)

  def test_directly_fail_excl(self):
    begin = timeit.default_timer()
    p = subprocess.Popen([lockf, '-e', self.base_dir + '/foo', echo, 'x', '1', '0', '1'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.02)
    q = subprocess.Popen([lockf, '-e', self.base_dir + '/foo', echo, 'x', '1', '0', '1'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    q.communicate()
    self.assertEqual(q.returncode, 1);
    end = timeit.default_timer()
    self.assertTrue(abs((end-begin)) < 0.04)

    p.communicate()
    self.assertEqual(p.returncode, 0);
    end = timeit.default_timer()
    self.assertTrue(abs((end-begin)-1) < 0.04)

  def test_excl_unlink(self):
    p = subprocess.Popen([lockf, '-u', '-e', self.base_dir + '/foo', echo, 'x', '1', '0', '1'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.2)
    self.assertTrue(os.path.isfile(self.base_dir + '/foo'))
    p.communicate()
    self.assertEqual(p.returncode, 0)
    self.assertFalse(os.path.isfile(self.base_dir + '/foo'))

  def test_directly_fail_mkdir(self):
    begin = timeit.default_timer()
    p = subprocess.Popen([lockf, '-m', self.base_dir + '/foo', echo, 'x', '1', '0', '1'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.02)
    q = subprocess.Popen([lockf, '-m', self.base_dir + '/foo', echo, 'x', '1', '0', '1'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    q.communicate()
    self.assertEqual(q.returncode, 1);
    end = timeit.default_timer()
    self.assertTrue(abs((end-begin)) < 0.04)

    p.communicate()
    self.assertEqual(p.returncode, 0);
    end = timeit.default_timer()
    self.assertTrue(abs((end-begin)-1) < 0.04)

  def test_directly_fail_link(self):
    begin = timeit.default_timer()
    p = subprocess.Popen([lockf, '-i', self.base_dir + '/foo',
        echo, 'x', '1', '0', '1'],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.02)
    q = subprocess.Popen([lockf, '-i', self.base_dir + '/foo',
        echo, 'x', '1', '0', '1'],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    q.communicate()
    self.assertEqual(q.returncode, 1);
    end = timeit.default_timer()
    self.assertTrue(abs((end-begin)) < 0.04)

    p.communicate()
    self.assertEqual(p.returncode, 0);
    end = timeit.default_timer()
    self.assertTrue(abs((end-begin)-1) < 0.04)

  def test_directly_fail_rename(self):
    with open(self.base_dir + '/foo', 'x') as f:
      pass
    begin = timeit.default_timer()
    p = subprocess.Popen([lockf, '-r', self.base_dir + '/foo',
        echo, 'x', '1', '0', '1'],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.02)
    q = subprocess.Popen([lockf, '-r', self.base_dir + '/foo',
        echo, 'x', '1', '0', '1'],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    q.communicate()
    self.assertEqual(q.returncode, 1);
    end = timeit.default_timer()
    self.assertTrue(abs((end-begin)) < 0.04)

    p.communicate()
    self.assertEqual(p.returncode, 0);
    end = timeit.default_timer()
    self.assertTrue(abs((end-begin)-1) < 0.04)


if __name__ == '__main__':
    unittest.main()


