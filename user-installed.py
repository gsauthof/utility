#!/usr/bin/env python3

# List all manually installed packages.
#
# Supported distributions:
#
# - Fedora
# - CentOS/RHEL
# - Termux
# - Debian
# - Ubuntu
#
# 2017, Georg Sauthoff <mail@gms.tf>, GPLv3+


import gzip
import io
import itertools
import operator
import os
import distro
import re
import subprocess
import sys
import unittest.mock as mock


split_version_re = re.compile('[.+-]')


class LooseVersion:
    def __init__(self, s):
        self.vs = split_version_re.split(s)
        self.n  = max(len(x) for x in self.vs)
    def cmp(self, other, op):
        n = max(self.n, other.n)
        return op(tuple(x.zfill(n) for x in self.vs), tuple(x.zfill(n) for x in other.vs))
    def __lt__(self, other):
        return self.cmp(other, operator.lt)
    def __le__(self, other):
        return self.cmp(other, operator.le)
    def __gt__(self, other):
        return self.cmp(other, operator.gt)
    def __ge__(self, other):
        return self.cmp(other, operator.ge)
    def __eq__(self, other):
        return self.cmp(other, operator.eq)
    def __ne__(self, other):
        return self.cmp(other, operator.ne)


def cmp_version(a, b, cmp):
    xs = split_version_re.split(a)
    ys = split_version_re.split(b)
    n = max(len(x) for x in xs + ys)
    return cmp(tuple(x.zfill(n) for x in xs), tuple(x.zfill(n) for x in ys))


def test_cmp_version():
    assert LooseVersion('10.3.5') > LooseVersion('9.3.5')
    assert LooseVersion('1.2.4-rc3') < LooseVersion('1.2.4-rc4')

# to try something different: pytest instead of unittest and
# in-module tests instead of separate test file
# unittests: pytest style test_ functions
# call e.g.: py.test-3 user-installed.py


# For Fedora >= 26 this functionality is also available via the dnf command:
# dnf repoquery --qf '%{name}' --userinstalled \
#    | grep -v -- '-debuginfo$' \
#    | grep -v '^\(kernel-modules\|kernel\|kernel-core\|kernel-devel\)$'
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
  f_distro_id = lambda : 'fedora'
  f_distro_version = lambda : '23'
  import collections
  Rec = collections.namedtuple('Rec', ['name'])
  f_dnf = mock.Mock()
  f_Base = mock.Mock()
  f_Base.fill_sack.return_value = None
  f_Base.iter_userinstalled.return_value = iter([Rec('kernel-modules'), Rec('foo-debuginfo'), Rec('zsh'), Rec('vim')])
  f_dnf.Base.return_value = f_Base
  # yes, we are mocking the local import of dnf
  with mock.patch('distro.id', f_distro_id), \
       mock.patch('distro.version', f_distro_version), \
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


def test_list_centos(name='centos'):
  f_distro_id = lambda : name
  f_distro_version  = lambda : '7.3.1611'
  f_check_output = mock.Mock(return_value=b'''time,user,0
xfsprogs,user,4294967295
xz,dep,4294967295
yum,user,4294967295
yum-cron,user,0
yum-cron,user,0
yum-utils,dep,0
zsh,user,1000''')
  with mock.patch('distro.id', f_distro_id), \
       mock.patch('distro.version', f_distro_version), \
       mock.patch('subprocess.check_output', f_check_output), \
       mock.patch('sys.stdout', new=io.StringIO()) as fake_out:
    main() # calls list_centos
  assert fake_out.getvalue() == 'time\nyum-cron\nzsh\n'

def test_list_rhel(name='rhel'):
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

# this basically approximate a package list similar to what would be
# in /var/log/installer/initial-status.gz via looking at
# the mtime of /var/lib/dpkg/info/*.list files - until a well-known
# system package
# limitation: upgraded packages get new mtime/ctimes such that
# the grub-common marker may miss some or catch too much (if grub-common
# is upgraded)
def get_initial_lst(selection, last_pkg='grub-common'):
  l = takefind(lambda fn: fn == last_pkg, selection)
  return l

def get_all_lst(dirname='/var/lib/dpkg/info'):
  l = map(lambda fn: fn.split(':')[0],
        map(lambda fn: fn[:-5],
          map(operator.itemgetter(0),
            sorted(
              map(lambda fn: (fn, os.path.getmtime(dirname+'/'+fn)),
                filter(lambda fn: fn.endswith('.list'),
                  os.listdir(dirname))),
              key=operator.itemgetter(1)))))
  return l

def dpkg_log_names(dirname='/var/log'):
  expr = re.compile(r'dpkg\.log(\.[0-9]+)?(\.gz)?')
  l = sorted(filter(expr.match, os.listdir(dirname)),
        key=LooseVersion, reverse=True)
  return l

def dpkg_log_lines(dirname='/var/log'):
  fns = dpkg_log_names(dirname)
  for fn in fns:
    ofn = gzip.open if fn.endswith('.gz') else open;
    with ofn(dirname+'/'+fn, 'rt') as f:
      for line in f:
        yield line

# kind of complement to /var/log/installer/initial-status.gz
# note that dpkg.log* is rotated, by default
# (montly, 12 times), thus this is not sufficient after 1 year
#
# i.e. /var/log/dpkg.log* - grepping for 'install|remove' there
# yields all packages that are installed after sys-creation (i.e.
# inner join with apt-mark showmanual)
def get_dpkg_installed(dirname='/var/log'):
  ps = set()
  expr = re.compile('^[^ ]+ [^ ]+ (install|remove) ')
  for line in filter(expr.match, dpkg_log_lines(dirname)):
    (action, p) = line.split(' ')[2:4]
    p = p.split(':')[0]
    if action == 'install':
      ps.add(p)
    else:
      ps.discard(p)
  return ps

# - in contrast to termux: debian has `apt-mark showmanual`, i.e.
# we don't need to parse extended_states ourselves
# - similar to termux: there are installations without
# /var/log/installer/initial-status.gz (debian/ubuntu)
# - /var/log/apt/history.log gets also rotated away and takes a
# bit more effort to parse
def list_debian():
  ms = subprocess.check_output(['apt-mark', 'showmanual']).decode() \
      .splitlines()
  ds = get_dpkg_installed()
  for p in ms:
    if p in ds:
      print(p)

def test_list_debian(dname='debian'):
  f_distro_id = lambda : dname
  f_distro_version = lambda : ''
  def f_check_output(*args, **kargs):
    return b'''acl
apt
grub-common
curl
vim
'''
  #f_listdir = lambda x : [ 'acl:amd64.list', 'apt.list', 'e',
  #    'grub-common.list', 'curl.list', 'd' ]
  f_listdir = lambda x : [ 'dpkg.log' ]
  # mock.patch('os.path.getmtime', F_Getmtime()),
  class F_Getmtime:
    def __init__(self):
      self.l = range(10).__iter__()
    def __call__(self, fn):
      return next(self.l)
  f_open = mock_open(read_data='''2017-01-22 17:44:14 install make:amd64 <none> 3.81-8.2ubuntu3
2017-01-22 17:44:31 remove make:amd64 3.81-8.2ubuntu3 <none>
2017-01-22 17:48:06 install curl foo bar
2017-01-22 17:48:06 install vim foo bar
''')
  with mock.patch('distro.id', f_distro_id), \
       mock.patch('distro.version', f_distro_version), \
       mock.patch('subprocess.check_output', f_check_output), \
       mock.patch('os.listdir', f_listdir), \
       mock.patch('builtins.open', f_open), \
       mock.patch('sys.stdout', new=io.StringIO()) as fake_out:
    main() # calls list_debian()
    assert fake_out.getvalue() == 'curl\nvim\n'

def test_list_ubuntu():
  test_list_debian('ubuntu')

# work-around bug:
# http://bugs.python.org/issue21258
# cf. http://stackoverflow.com/questions/24779893/customizing-unittest-mock-mock-open-for-iteration
def mock_open(*args, **kargs):
  f_open = mock.mock_open(*args, **kargs)
  f_open.return_value.__iter__ = lambda self : iter(self.readline, '')
  return f_open

def test_list_termux():
  f_distro_id = lambda : ''
  f_distro_version = lambda : ''
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
  with mock.patch('distro.id', f_distro_id), \
       mock.patch('distro.version', f_distro_version), \
       mock.patch('subprocess.check_output', f_check_output), \
       mock.patch('os.path.exists', f_exists), \
       mock.patch('builtins.open', f_open), \
       mock.patch('sys.stdout', new=io.StringIO()) as fake_out:
    main() # calls list_termux()
    assert fake_out.getvalue() == 'bc\nzsh\n'



list_fn = {
     'fedora': list_fedora
    ,'centos': list_centos
    ,'rhel': list_centos
    ,'debian': list_debian
    ,'ubuntu': list_debian
    }

def main():
  dname, version = distro.id(), distro.version()
  try:
    fn = list_fn[dname]
    if fn == list_centos and LooseVersion(version) >= LooseVersion('8.0'):
      list_fedora()
    elif fn == list_fedora and LooseVersion(version) < LooseVersion('23'):
      list_centos()
    else:
      fn()
  except KeyError:
    if os.path.exists('/data/data/com.termux'):
      list_termux()
    else:
      raise RuntimeError(f'Unknown system (distribution: {dname})')
  return 0
  

if __name__ == '__main__':
  sys.exit(main())

