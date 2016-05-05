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
chronic = os.getenv('chronic', './silence')
fail = os.getenv('fail', './fail')
echo = os.getenv('echo', './echo.sh')



class Ctrl_C_Thread(threading.Thread):

  def __init__(self, pid):
    self.pid = pid
    threading.Thread.__init__(self)

  def run(self):
    time.sleep(1)
    p = psutil.Process(self.pid)
    self.children = p.children()
    if len(self.children) != 1:
      return
    q = self.children[0]
    if len(q.children()) != 1:
      return
    r = q.children()[0]
    q.send_signal(signal.SIGINT)
    time.sleep(0.2)
    r.send_signal(signal.SIGINT)

class Kill_Thread(threading.Thread):

  def __init__(self, pid):
    self.pid = pid
    threading.Thread.__init__(self)

  def run(self):
    time.sleep(1)
    p = psutil.Process(self.pid)
    if len(p.children()) != 1:
      return
    q = p.children()[0]
    # i.e. kill(pid, SIGTERM)
    p.terminate()
    time.sleep(1)
    self.was_still_running = q.is_running();
    if self.was_still_running:
      q.kill()

class Basic(unittest.TestCase):


  def test_wait(self):
    begin = timeit.default_timer()
    subprocess.check_output([chronic, 'sleep', '1'], stderr=subprocess.STDOUT)
    end = timeit.default_timer()
    self.assertTrue(end-begin >= 1)

  def test_true(self):
    begin = timeit.default_timer()
    subprocess.check_output([chronic, 'true'], stderr=subprocess.STDOUT)
    end = timeit.default_timer()
    # moreutils chronic fails this on an i7 with SSD
    self.assertTrue(end-begin <= 0.01)

  def test_false(self):
    begin = timeit.default_timer()
    code = subprocess.call([chronic, 'false'])
    end = timeit.default_timer()
    # moreutils chronic fails this on an i7 with SSD
    self.assertTrue(end-begin <= 0.01)
    self.assertEqual(code, 1)

  def test_abort(self):
    code = subprocess.call([fail])
    self.assertEqual(code, -6)
    code = subprocess.call([chronic, fail])
    # GNU time returns 6
    # moreutils' chronic returns 1
    self.assertEqual(code, 134)


  def test_no_out(self):
    o = subprocess.check_output([chronic, echo, 'Hello World\n23', '1', '0'])
    self.assertEqual(o, b'')

  def test_no_out_other(self):
    o = subprocess.check_output([chronic, '-e', '11', echo, 'xyz', '1', '11'])
    self.assertEqual(o, b'')

  def test_no_err(self):
    o = subprocess.check_output([chronic, echo, 'Hello World\n23', '2', '0'])
    self.assertEqual(o, b'')

  def test_out(self):
    p = subprocess.Popen([chronic, echo, 'Hello World\n23', '1', '1'],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    o, e = p.communicate()
    self.assertEqual(o, b'Hello World\n23\n')
    self.assertEqual(e, b'')
    self.assertEqual(p.returncode, 1)

  def test_opt_ordering(self):
    p = subprocess.Popen([chronic, echo, '-k', '1', '23'],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    o, e = p.communicate()
    self.assertEqual(o, b'-k\n')
    self.assertEqual(e, b'')
    self.assertEqual(p.returncode, 23)

  def test_err(self):
    p = subprocess.Popen([chronic, echo, 'Hello World\n23', '2', '1'],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    o, e = p.communicate()
    self.assertEqual(o, b'')
    self.assertEqual(e, b'Hello World\n23\n')
    self.assertEqual(p.returncode, 1)

  def test_exit_code(self):
    o = b''
    code = 0
    try:
      o = subprocess.check_output([chronic, echo, 'Hello', '1', '23'])
    except subprocess.CalledProcessError as e:
      o = e.output
      code = e.returncode
    self.assertEqual(o, b'Hello\n')
    self.assertEqual(code, 23)


  def test_ctrl_c(self):
    p = subprocess.Popen([chronic, echo, 'Hello World\n23', '2', '0', '3'],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    t = Ctrl_C_Thread(p.pid)
    t.start()
    o, e = p.communicate()
    t.join()
    self.assertEqual(len(t.children), 1)
    # GNU time returns 2
    # moreutils' chronic returns 1
    self.assertEqual(p.returncode, 130)
    self.assertEqual(o, b'')
    self.assertEqual(e, b'Hello World\n23\n')
    p.wait()

  def test_tmp_empty(self):
    base_dir = tempfile.mkdtemp()
    old_tmpdir = None
    if 'TMPDIR' in os.environ:
      old_tmpdir = os.environ['TMPDIR']
    os.environ['TMPDIR'] = base_dir
    o = subprocess.check_output([chronic, echo, 'Foo', '1', '0'])
    e = subprocess.check_output([chronic, echo, 'Bar', '2', '0'])
    self.assertFalse(os.listdir(base_dir))
    if old_tmpdir:
      os.environ['TMPDIR'] = old_tmpdir
    else:
      os.environ.pop('TMPDIR')
    shutil.rmtree(base_dir)


  def test_tmpdir_is_used(self):
    old_tmpdir = None
    if 'TMPDIR' in os.environ:
      old_tmpdir = os.environ['TMPDIR']
    os.environ['TMPDIR'] = '/does/not/exist/like/never/ever' 
    p = subprocess.Popen([chronic, echo, 'Foo', '1', '0'],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    o, e = p.communicate()
    if old_tmpdir:
      os.environ['TMPDIR'] = old_tmpdir
    else:
      os.environ.pop('TMPDIR')

    # moreutils chronic return 0, i.e. doesn't honor TMPDIR
    self.assertEqual(p.returncode, 1)
    self.assertTrue(b'No such file or directory' in e)
    # errno string doesn't include the function everywhere
    # self.assertTrue(b'open' in e or b'mkstemp' in e)

  def test_keep_running(self):
    p = subprocess.Popen([chronic, echo, 'Foo Bar', '1', '0', '3'],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    t = Kill_Thread(p.pid)
    t.start()
    o, e = p.communicate()
    t.join()
    self.assertEqual(p.returncode, -15)
    self.assertEqual(o, b'')
    self.assertEqual(e, b'')
    self.assertTrue(t.was_still_running)

  def test_keep_running_not(self):
    p = subprocess.Popen([chronic, '-k', echo, 'Foo Bar', '1', '0', '3'],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    t = Kill_Thread(p.pid)
    t.start()
    o, e = p.communicate()
    t.join()
    self.assertIn(p.returncode, [-15, 143])
    self.assertEqual(o, b'')
    self.assertEqual(e, b'')
    self.assertFalse(t.was_still_running)

