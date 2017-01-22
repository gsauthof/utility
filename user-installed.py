#!/usr/bin/env python3

# List all manually installed packages.
#
# Supported distributions:
#
# - Fedora
# - CentOS/RHEL
# - Termux
#
# 2017, Georg Sauthoff <mail@gms.tf>, GPLv3+


from distutils.version import LooseVersion
import io
import itertools
import operator
import os
import platform
import subprocess
import sys
import unittest.mock as mock

# to try something different: pytest instead of unittest and
# in-module tests instead of separate test file
# unittests: pytest style test_ functions
# call e.g.: py.test-3 user-installed.py


# Fedora >= 23
# cf. http://unix.stackexchange.com/questions/82880/how-to-replicate-installed-package-selection-from-one-fedora-instance-to-another/82882#82882
def list_fedora():
  import dnf
  b = dnf.Base()
  b.fill_sack();
  l = sorted(set(x.name for x in b.iter_userinstalled()
       if not x.name.endswith("-debuginfo") \
          and x.name not in \
             ["kernel-modules", "kernel", "kernel-core", "kernel-devel"] ))
  print("\n".join(l))

def test_list_fedora():
  f_linux_distribution = lambda : ('Fedora', '', '')
  import collections
  Rec = collections.namedtuple('Rec', ['name'])
  f_dnf = mock.Mock()
  f_Base = mock.Mock()
  f_Base.fill_sack.return_value = None
  f_Base.iter_userinstalled.return_value = iter([Rec('kernel-modules'), Rec('foo-debuginfo'), Rec('zsh'), Rec('vim')])
  f_dnf.Base.return_value = f_Base
  # yes, we are mocking the local import of dnf
  with mock.patch('platform.linux_distribution', f_linux_distribution), \
       mock.patch.dict('sys.modules', {'dnf': f_dnf}), \
       mock.patch('sys.stdout', new=io.StringIO()) as fake_out:
    main() # calls list_fedora()
    assert fake_out.getvalue() == 'vim\nzsh\n'


# cf. http://unix.stackexchange.com/questions/82880/how-to-replicate-installed-package-selection-from-one-fedora-instance-to-another/82882#82882
def list_centos():
  rq = subprocess.check_output(['repoquery', '--installed',
    '--qf', '%{n},%{yumdb_info.reason},%{yumdb_info.installed_by}', '--all']
    ).decode()
  ps = [ row[0] for row in (line.split(',') for line in rq.splitlines())
           if row[1] == 'user' and row[2] != '4294967295' ]
  for p in map(operator.itemgetter(0), itertools.groupby(sorted(ps))):
    print(p)


def test_list_centos(name='CentOS Linux'):
  f_linux_distribution = lambda : (name, '7.3.1611', '')
  f_check_output = mock.Mock(return_value=b'''time,user,0
xfsprogs,user,4294967295
xz,dep,4294967295
yum,user,4294967295
yum-cron,user,0
yum-cron,user,0
yum-utils,dep,0
zsh,user,1000''')
  with mock.patch('platform.linux_distribution', f_linux_distribution), \
       mock.patch('subprocess.check_output', f_check_output), \
       mock.patch('sys.stdout', new=io.StringIO()) as fake_out:
    main() # calls list_centos
    assert fake_out.getvalue() == 'time\nyum-cron\nzsh\n'

def test_list_rhel(name='Red Hat Enterprise Linux'):
  test_list_centos(name)

# i.e. lines from files/usr/var/lib/apt/extended_states
# thus should be similar to what e.g. apt-mark showmanual does
# (apt-mark is not available on termux)
# cf. http://askubuntu.com/questions/2389/generating-list-of-manually-installed-packages-and-querying-individual-packages
def parse_auto_installed(lines):
  ps = set()
  package = None
  for line in lines:
    if line.startswith('Package: '):
      package = line[9:]
    elif line.startswith('Auto-Installed: 1'):
      ps.add(package)
  return ps

def test_parse_auto_installed():
  s = parse_auto_installed('''Package: libgcc
Architecture: aarch64
Auto-Installed: 1

Package: binutils
Architecture: aarch64
Auto-Installed: 1

Package: ndk-sysroot
Architecture: aarch64
Auto-Installed: 1

'''.splitlines())
  l = sorted(s)
  assert l == ['binutils', 'libgcc', 'ndk-sysroot']

# i.e. lines from dpkg --get-selections
def get_selections(lines):
  ps = [ line.split('\t', maxsplit=1)[0] for line in lines ]
  return ps

def test_get_selections():
  lines = '''apt						install
bash						install
bc						install
binutils					install'''.splitlines()
  assert get_selections(lines) == [ 'apt', 'bash', 'bc', 'binutils' ]


# Termux uses apt and dpkg - but doesn't have something like
# var/log/installer/initial-status.gz
# thus, some base packages are not marked as auto-installed
# in the extended_states file
#
# this is an approximation of what is per-installed on Termux (as of 2016-12)
default_termux_pkgs = set([
  'apt',
  'bash',
  'busybox',
  'command-not-found',
  'dash',
  'dpkg',
  'gpgv',
  'libandroid-support',
  'libgnustl',
  'liblzma',
  'ncurses',
  'readline',
  'termux-tools'
 ])

# Test on the command line:
# paste - - - - < /data/data/com.termux/files/usr/var/lib/apt/extended_states  | grep 'Auto-Installed: 1' | awk '{print $2}' > ~/a
# dpkg --get-selections | cut -f1 > ~/i
# grep install /data/data/com.termux/files/usr/var/log/apt/history.log  | sed 's/^.*install //' | tr ' ' '\n' > ~/h
# sed -e 's/^/^/' -e 's/$/$/' -i a h
# grep -v -f ~/a ~/i | grep -v -f ~/h | sed "s/\(.*\)/'\1',/"
def list_termux():
  extended_states_filename = \
      '/data/data/com.termux/files/usr/var/lib/apt/extended_states'
  with open(extended_states_filename, 'r') as es:
    ss = subprocess.check_output(
        ['dpkg', '--get-selections']).decode().splitlines()
    # when line comes from open() it has a trailing newline that we strip
    auto_installed = parse_auto_installed(map(lambda line : line[:-1],  es))
    selections = get_selections(ss)
    for s in selections:
      if s not in auto_installed and s not in default_termux_pkgs:
        print(s)

def takefind(f, g):
  for i in g:
    yield i
    if f(i):
      break

# we basically approximate a package list similar to what would be
# in /var/log/installer/initial-status.gz via looking at
# the mtime of /var/lib/dpkg/info/*.list files - until a well-known
# system package
def get_initial_lst(dirname='/var/lib/dpkg/info', last_pkg='grub-common'):
  l = set(takefind(lambda fn: fn == last_pkg,
      map(lambda fn: fn.split(':')[0],
        map(lambda fn: fn[:-5],
          map(operator.itemgetter(0),
            sorted(
              map(lambda fn: (fn, os.path.getmtime(dirname+'/'+fn)),
                filter(lambda fn: fn.endswith('.list'),
                  os.listdir(dirname))),
              key=operator.itemgetter(1)))))))
  return l


# - in contrast to termux: debian has `apt-mark showmanual`, i.e.
# we don't need to parse extended_states ourselves
# - similar to termux: there are installations without
# /var/log/installer/initial-status.gz
# - there is /var/log/dpkg.log - grepping for 'install ' there
# yields all packages that are installed after sys-creation (i.e. inner join
# with apt-mark showmanual), but
# this file gets rotated away after 12 months
# - /var/log/apt/history.log gets also rotated away and takes more
# effort to parse because uninstalls have to be taken into account
def list_debian():
  ms = subprocess.check_output(['apt-mark', 'showmanual']).decode() \
      .splitlines()
  ts = get_initial_lst('/var/lib/dpkg/info', 'grub-common')
  for m in ms:
    if m not in ts:
      print(m)

# work-around bug:
# http://bugs.python.org/issue21258
# cf. http://stackoverflow.com/questions/24779893/customizing-unittest-mock-mock-open-for-iteration
def mock_open(*args, **kargs):
  f_open = mock.mock_open(*args, **kargs)
  f_open.return_value.__iter__ = lambda self : iter(self.readline, '')
  return f_open

def test_list_termux():
  f_linux_distribution = lambda : ('', '', '')
  def f_check_output(*args, **kargs):
    return b'''apt						install
bash						install
bc						install
binutils					install
zsh					install'''
  def f_exists(a):
    return a == '/data/data/com.termux'
  f_open = mock_open(read_data='''Package: libgcc
Architecture: aarch64
Auto-Installed: 1

Package: binutils
Architecture: aarch64
Auto-Installed: 1

Package: ndk-sysroot
Architecture: aarch64
Auto-Installed: 1

''')
  with mock.patch('platform.linux_distribution', f_linux_distribution), \
       mock.patch('subprocess.check_output', f_check_output), \
       mock.patch('os.path.exists', f_exists), \
       mock.patch('{}.open'.format(__name__), f_open), \
       mock.patch('sys.stdout', new=io.StringIO()) as fake_out:
    main() # calls list_termux()
    assert fake_out.getvalue() == 'bc\nzsh\n'



list_fn = {
    'Fedora': list_fedora,
    'CentOS Linux': list_centos,
    'Red Hat Enterprise Linux': list_centos,
    'debian': list_debian
    }

def main():
  (dname, version, _) = platform.linux_distribution()
  try:
    fn = list_fn[dname]
    if fn == list_centos and LooseVersion(version) >= LooseVersion('8.0'):
      list_fedora()
    else:
      fn()
  except KeyError:
    if os.path.exists('/data/data/com.termux'):
      list_termux()
    else:
      raise RuntimeError('Unknown system (distribution: {})'.format(dname))
  return 0
  

if __name__ == '__main__':
  sys.exit(main())

