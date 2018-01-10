#!/usr/bin/env python3
#
# generate the stored core files for test/pargs.py unittests
#
# 2018, Georg Sauthoff <mail@gms.tf>
#
# SPDX-License-Identifier: GPL-3.0-or-later

import os
import resource
import subprocess
import sys
import time

std_env = {
  'EDITOR'       : 'vim',
  'LESS'         : 'FR',
  'LESSOPEN'     : '||/usr/bin/lesspipe.sh %s',
  'PAGER'        : 'less',
  'PATH'         : '/usr/bin:/bin',
  'SYSTEMD_LESS' : 'FRXMK',
  'VISUAL'       : 'vim',
}
args = [ '10', 'hello', 'world' ]
arch = os.uname().machine

have_cdctl = os.path.exists('/usr/bin/coredumpctl')
if not have_cdctl:
  resource.setrlimit(resource.RLIMIT_CORE, (resource.RLIM_INFINITY, resource.RLIM_INFINITY))
  if os.path.exists('core'):
    os.unlink('core')

p = subprocess.Popen(['./snooze'] + args, env=std_env)
time.sleep(1)
p.send_signal(6)
p.wait()
time.sleep(2)
filename = 'core.snooze.{}.abrt.{}'.format(arch, p.pid)
if have_cdctl:
  subprocess.check_output(['coredumpctl', 'dump', str(p.pid), '-o', filename])
else:
  os.rename('core', filename)
subprocess.check_output(['xz', '--compress', filename])

for exe in [ 'snooze', 'snooze32' ]:
  p = subprocess.Popen(['./{}'.format(exe)] + args, env=std_env)
  time.sleep(1)
  subprocess.check_output(['gcore', str(p.pid)])
  filename = 'core.{}.{}.{}'.format(exe, arch, p.pid)
  os.rename('core.{}'.format(p.pid), filename)
  subprocess.check_output(['xz', '--compress', filename])

