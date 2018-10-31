#!/usr/bin/env python3
#
# cat unittests
#
# 2018, Georg Sauthoff <mail@gms.tf>
#
# SPDX-License-Identifier: GPL-3.0-or-later

import glob
import os
import pytest
import re
import shutil
import subprocess
import tempfile

src_dir = os.getenv('src_dir', os.getcwd()+'/..')
test_dir = src_dir + '/test/in'

dcat    = os.getenv('dcat', './dcat')

@pytest.mark.parametrize('c', ( 'gzip', 'bzip2', 'xz', 'lz4', 'zstd' ))
def test_basic(c):
    cmd = 'echo Hello World | {} -c | {}'.format(c, dcat)
    o = subprocess.check_output(cmd, shell=True, universal_newlines=True)
    assert o == 'Hello World\n'

@pytest.mark.parametrize('suf', ('', ' -'))
def test_not_compressed(suf):
    s = dcat + suf
    cmd = 'echo foo | {}'.format(s)
    o = subprocess.check_output(cmd, shell=True, universal_newlines=True)
    assert o == 'foo\n'

@pytest.mark.parametrize('hstr', ['-h', '--help'])
def test_help(hstr):
    p = subprocess.run([dcat, hstr], stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, universal_newlines=True)
    assert p.returncode == 0
    assert p.stderr == ''
    assert 'help' in  p.stdout

def test_multiple():
    with tempfile.TemporaryDirectory() as d:
        for c in ( 'gzip', 'bzip2', 'xz', 'lz4', 'zstd' ):
            with open('{}/txt.{}'.format(d, c), 'wt') as f:
                subprocess.run([c, '-c'], input='blah ', universal_newlines=True,
                        stdout=f, check=True)
        with open('{}/txt'.format(d), 'wt') as f:
            f.write('blah ')
        p = subprocess.run([dcat] + glob.glob(d+'/txt*'), universal_newlines=True,
                stdout=subprocess.PIPE)
        assert p.returncode == 0
        p.stdout = 'blah\n'*6


