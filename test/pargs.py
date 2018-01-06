#!/usr/bin/env python3
#
# pargs unittests
#
# 2017, Georg Sauthoff <mail@gms.tf>
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

pargs    = os.getenv('pargs', './pargs')
snooze32 = os.getenv('snooze32', './snooze32')
snooze = os.getenv('snooze', './snooze')
busy_snooze = os.getenv('busy_snooze', './busy_snooze')

def test_noargs():
  p = subprocess.run([pargs], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
      universal_newlines=True)
  assert not p.stdout
  assert p.returncode == 2
  assert len(p.stderr) > 10
  assert 'Usage: ' in p.stderr
  assert '-a ' in p.stderr
  assert '-e ' in p.stderr

@pytest.mark.parametrize("hstr", ['-h', '--help'])
def test_help(hstr):
  p = subprocess.run([pargs, hstr], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert not p.stderr
  assert p.returncode == 0
  assert len(p.stdout) > 10
  assert 'Usage: ' in p.stdout
  assert '-a ' in p.stdout
  assert '-e ' in p.stdout

@pytest.mark.parametrize("ostr", ['-z', '--hel', '--helper', '--foo'])
def test_unk(ostr):
  p = subprocess.run([pargs, ostr, '123'], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 2
  assert not p.stdout
  assert 'Unknown' in p.stderr
  assert ostr in p.stderr

@pytest.mark.parametrize("ostr", ['-x', '-e'])
def test_cmdline_combi(ostr):
  p = subprocess.run([pargs, '-l', ostr, '123'], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 2
  assert not p.stdout
  assert 'incompatible' in p.stderr
  assert ostr in p.stderr

@pytest.mark.parametrize("opts", [[], ['-a'], ['--'], ['-a', '--'] ])
def test_argv(opts):
  c = subprocess.Popen([snooze, '1', '0'])
  assert c.pid
  p = subprocess.run([pargs] + opts +  [str(c.pid)], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 0
  assert not p.stderr
  assert p.stdout == '''{0}: {1} 1 0
argv[0]: {1}
argv[1]: 1
argv[2]: 0
'''.format(c.pid, snooze)
  c.wait()

def test_envp():
  c = subprocess.Popen([snooze, '1', '0'])
  assert c.pid
  p = subprocess.run([pargs, '-e', str(c.pid)], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 0
  assert not p.stderr
  ls = p.stdout.splitlines()
  assert ls[0] == '''{}: {} 1 0'''.format(c.pid, snooze)
  assert ls[1].startswith('envp[0]: ')
  assert ls[-1].startswith('envp[{}]: '.format(len(ls)-2))
  assert re.search('\nenvp\[[0-9]+\]: PATH={}\n'.format(os.environ['PATH']), p.stdout)
  c.wait()

@pytest.mark.parametrize("opts", [ ['-ea'], ['-a', '-e'] ])
def test_argv_envp(opts):
  c = subprocess.Popen([snooze, '1', '0'])
  assert c.pid
  p = subprocess.run([pargs] + opts + [  str(c.pid)], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 0
  assert not p.stderr
  ls = p.stdout.splitlines()
  assert ls[0] == '''{}: {} 1 0'''.format(c.pid, snooze)
  assert ls[1] == 'argv[0]: {}'.format(snooze)
  assert ls[3] == 'argv[2]: 0'
  assert ls[4] == ''
  assert ls[5].startswith('envp[0]: ')
  assert ls[-1].startswith('envp[{}]: '.format(len(ls) - 2 - 3 - 1))
  assert re.search('\nenvp\[[0-9]+\]: PATH={}\n'.format(os.environ['PATH']), p.stdout)
  c.wait()

def test_cmdline():
  c = subprocess.Popen([snooze, '1', '0'])
  assert c.pid
  p = subprocess.run([pargs, '-l', str(c.pid)], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 0
  assert not p.stderr
  assert p.stdout == '{} 1 0\n'.format(snooze)
  c.wait()


def test_auxv():
  c = subprocess.Popen([snooze, '1', '0'])
  assert c.pid
  p = subprocess.run([pargs, '-x', str(c.pid)], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 0
  assert not p.stderr
  ls = p.stdout.splitlines()
  assert ls[0] == '''{}: {} 1 0'''.format(c.pid, snooze)
  v = '\nAT_PAGESZ       0x{:016x} {} KiB\n'.format(os.sysconf('SC_PAGESIZE'),
      int(os.sysconf('SC_PAGESIZE')/1024))
  assert v in p.stdout
  c.wait()

@pytest.mark.parametrize("opts", [ ['-eax'], ['-xea'], ['-a', '-x', '-e' ] ])
def test_eax(opts):
  c = subprocess.Popen([snooze, '1', '0'])
  assert c.pid
  p = subprocess.run([pargs] + opts + [ str(c.pid)], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 0
  assert not p.stderr
  ls = p.stdout.splitlines()
  assert ls[0] == '''{}: {} 1 0'''.format(c.pid, snooze)
  assert sum(not x for x in ls) == 2
  assert '\nAT_PLATFORM ' in p.stdout
  assert '\nargv[0]: ' in p.stdout
  assert '\nenvp[0]: ' in p.stdout
  assert p.stdout.find('\nargv[0]') < p.stdout.find('\nenvp[0]')
  assert p.stdout.find('\nenvp[0]') < p.stdout.find('\nAT_PLATFORM ')
  c.wait()

def test_multiple():
  c = subprocess.Popen([snooze, '1', '0'])
  assert c.pid
  d = subprocess.Popen([snooze, '1', '1'])
  assert d.pid
  p = subprocess.run([pargs, str(c.pid), str(d.pid)], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 0
  assert not p.stderr
  ls = p.stdout.splitlines()
  assert len(ls) == 9
  assert ls[0] == '''{}: {} 1 0'''.format(c.pid, snooze)
  assert not ls[4]
  assert ls[5] == '''{}: {} 1 1'''.format(d.pid, snooze)
  assert ls[-1] == 'argv[2]: 1'
  c.wait()
  d.wait()

def test_auxv32():
  c = subprocess.Popen([snooze32, '1', '42'])
  assert c.pid
  p = subprocess.run([pargs, '-x', str(c.pid)], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 0
  assert not p.stderr
  ls = p.stdout.splitlines()
  assert ls[0] == '''{}: {} 1 42'''.format(c.pid, snooze32)
  assert sum(not x for x in ls) == 0
  assert '\nAT_PLATFORM ' in p.stdout
  v = '\nAT_UID          0x{0:016x} {0}\n'.format(os.getuid())
  assert v in p.stdout
  c.wait()

def test_no_such_pid():
  c = subprocess.Popen([snooze, '0'])
  assert c.pid
  c.wait()
  p = subprocess.run([pargs, str(c.pid)], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 1
  assert p.stdout == '{}: '.format(c.pid)
  assert p.stderr == 'No such file or directory\n'

@pytest.mark.parametrize("opts", [ [ '-e'], ['-x'] ])
def test_no_perm(opts):
  q = subprocess.run(['pgrep', '-u', 'root', '^writeback$'],
      stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True,
      check=True)
  pid = int(q.stdout)
  p = subprocess.run([pargs] + opts + [str(pid)], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 1
  assert p.stdout == '{}: \n'.format(pid)
  assert p.stderr == 'Permission denied\n'

def test_auxv_verbose():
  c = subprocess.Popen([snooze, '1', '42'])
  assert c.pid
  p = subprocess.run([pargs, '-v', '-x', str(c.pid)], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 0
  assert not p.stderr
  ls = p.stdout.splitlines()
  assert ls[0] == '''{}: {} 1 42'''.format(c.pid, snooze)
  v = '\nAT_EUID         0x{0:016x} {0} (Effective uid)\n'.format(os.geteuid())
  assert v in p.stdout
  c.wait()

def test_too_long():
  p = subprocess.run([pargs, '123456789012345678901'], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 2
  assert not p.stdout
  assert p.stderr == 'PID is too long.\n'

def test_long_enough():
  p = subprocess.run([pargs, '12345678901234567890'], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 1
  assert p.stdout == '12345678901234567890: '
  assert p.stderr == 'No such file or directory\n'

@pytest.mark.parametrize("opts", [ [], ['-s'] ])
def test_proc_mem(opts):
  c = subprocess.Popen([busy_snooze, '1', 'fo o'], stdout=subprocess.DEVNULL)
  assert c.pid
  p = subprocess.run([pargs, '-x'] + opts + [str(c.pid)],
      stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 0
  assert not p.stderr
  ls = p.stdout.splitlines()
  assert ls[0] == '''{}: {} 1 fo o'''.format(c.pid, busy_snooze)
  l = [x for x in ls if x.startswith('AT_EXECFN')][0]
  assert l.endswith(busy_snooze)
  c.wait()

def test_proc_mem_rand():
  c = subprocess.Popen([busy_snooze, '1', 'fo o'], stdout=subprocess.DEVNULL)
  assert c.pid
  p = subprocess.run([pargs, '-x', str(c.pid)], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 0
  assert not p.stderr
  ls = p.stdout.splitlines()
  assert ls[0] == '''{}: {} 1 fo o'''.format(c.pid, busy_snooze)
  l = [x for x in ls if x.startswith('AT_RANDOM')][0]
  assert re.match(
      'AT_RANDOM       0x[0-9a-f]{16} [0-9a-f]{2}( [0-9a-f]{2}){15}', l)
  c.wait()

def test_hwcap():
  c = subprocess.Popen([busy_snooze, '1', 'fo o'], stdout=subprocess.DEVNULL)
  assert c.pid
  p = subprocess.run([pargs, '-x', str(c.pid)], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 0
  assert not p.stderr
  ls = p.stdout.splitlines()
  l = [x for x in ls if x.startswith('AT_HWCAP')][0]
  assert re.match(
      'AT_HWCAP        0x[0-9a-f]{16} [0-9a-z]+( \| [0-9a-z]+)*', l)
  c.wait()

@pytest.fixture(scope='module', params=[snooze, snooze32])
def mk_core_file(request):
  exe = request.param
  with tempfile.TemporaryDirectory() as d:
    c = subprocess.Popen([exe, '10', 'hello', 'world'])
    core = '{}/core'.format(d)
    subprocess.check_output(['gcore', '-o', core, str(c.pid)])
    c.terminate()
    c.wait()
    yield (exe, core + '.{}'.format(c.pid))

def d_core_file(core_xz):
  with tempfile.TemporaryDirectory() as d:
    shutil.copy(core_xz, d)
    b = os.path.basename(core_xz)
    subprocess.check_output(['xz', '-d', b], cwd=d)
    b = b[:-3]
    exe = './' + b[5:b.find('.', 5)]
    core = d + '/' + b
    yield (exe, core)

@pytest.fixture(scope='module', params=glob.glob(test_dir + '/core.*.xz'))
def decompress_core_file(request):
  core_xz = request.param
  yield from d_core_file(core_xz)

@pytest.fixture(scope='module')
def decompress_sectionless_core(request):
  core_xz = test_dir + '/nosec.core.snooze.x86_64.coredumpctl.11677.xz'
  yield from d_core_file(core_xz)

def check_core(mk_core_file):
  exe, core_file = mk_core_file
  pid = core_file[core_file.rfind('.')+1:]
  p = subprocess.run([pargs, core_file], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 0
  assert not p.stderr
  assert p.stdout == '''core '{0:}' of {1:}: {2:} 10 hello world
argv[0]: {2:}
argv[1]: 10
argv[2]: hello
argv[3]: world
'''.format(core_file, pid, exe)

def test_core(mk_core_file):
  check_core(mk_core_file);

def test_stored_core(decompress_core_file):
  check_core(decompress_core_file);

def test_store_core_without_sections(decompress_sectionless_core):
  exe, core_file = decompress_sectionless_core
  pid = core_file[core_file.rfind('.')+1:]
  p = subprocess.run([pargs, core_file], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 1
  assert not p.stdout
  assert p.stderr == 'File {} has no sections.\n'.format(core_file)

def test_core_envp(mk_core_file):
  exe, core_file = mk_core_file
  p = subprocess.run([pargs, '-e', core_file], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 0
  assert not p.stderr
  for key, val in os.environ.items():
    if 'PYTEST' in key:
      continue
    assert ': {}={}\n'.format(key, val) in p.stdout

def check_core_auxv(mk_core_file):
  exe, core_file = mk_core_file
  pid = core_file[core_file.rfind('.')+1:]
  p = subprocess.run([pargs, '-x', core_file], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 0
  assert not p.stderr
  ls = p.stdout.splitlines()
  assert ls[0] == "core '{0:}' of {1:}: {2:} 10 hello world".format(
      core_file, pid, exe)
  l = [x for x in ls if x.startswith('AT_PAGESZ') ][0]
  assert l == 'AT_PAGESZ       0x{:016x} {} KiB'.format(
      os.sysconf('SC_PAGESIZE'), int(os.sysconf('SC_PAGESIZE')/1024))
  l = [x for x in ls if x.startswith('AT_EXECFN')][0]
  assert l[l.rfind(' ')+1:] == exe

def test_core_auxv(mk_core_file):
  check_core_auxv(mk_core_file)

def test_stored_core_auxv(decompress_core_file):
  check_core_auxv(decompress_core_file)

def check_core_auxv_random(mk_core_file):
  exe, core_file = mk_core_file
  pid = core_file[core_file.rfind('.')+1:]
  p = subprocess.run([pargs, '-x', core_file], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 0
  assert not p.stderr
  ls = p.stdout.splitlines()
  l = [x for x in ls if x.startswith('AT_RANDOM')][0]
  assert re.match(
      'AT_RANDOM       0x[0-9a-f]{16} [0-9a-f]{2}( [0-9a-f]{2}){15}', l)

def test_core_auxv_random(mk_core_file):
  check_core_auxv_random(mk_core_file)

def test_stored_core_auxv_random(decompress_core_file):
  check_core_auxv_random(decompress_core_file)

def check_core_all(mk_core_file):
  exe, core_file = mk_core_file
  pid = core_file[core_file.rfind('.')+1:]
  p = subprocess.run([pargs, '-aexv', core_file], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE, universal_newlines=True)
  assert p.returncode == 0
  assert not p.stderr
  ls = p.stdout.splitlines()
  assert ls[0] == "core '{0:}' of {1:}: {2:} 10 hello world".format(
      core_file, pid, exe)
  assert sum(not x for x in ls) == 2
  assert '\nenvp[0]: ' in p.stdout
  assert '\nenvp[1]: ' in p.stdout
  assert ls[3] == 'argv[2]: hello'
  l = [x for x in ls if x.startswith('AT_EGID')][0]
  assert l.endswith('(Effective gid)')

def test_core_all(mk_core_file):
  check_core_all(mk_core_file)

def test_stored_core_all(decompress_core_file):
  check_core_all(decompress_core_file)

# XXX test: PPC big-endian core, add the core file
