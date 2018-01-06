#!/usr/bin/env python3
#
# generate the stored core files for test/pargs.py unittests
#
# 2018, Georg Sauthoff <mail@gms.tf>
#
# SPDX-License-Identifier: GPL-3.0-or-later

import os
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

p = subprocess.Popen(['./snooze'] + args, env=std_env)
time.sleep(1)
p.send_signal(6)
p.wait()
time.sleep(2)
filename = 'nosec.core.snooze.{}.coredumpctl.{}'.format(arch, p.pid)
subprocess.check_output(['coredumpctl', 'dump', str(p.pid), '-o', filename])
subprocess.check_output(['xz', '--compress', filename])

for exe in [ 'snooze', 'snooze32' ]:
  p = subprocess.Popen(['./{}'.format(exe)] + args, env=std_env)
  time.sleep(1)
  subprocess.check_output(['gcore', str(p.pid)])
  filename = 'core.{}.{}.{}'.format(exe, arch, p.pid)
  os.rename('core.{}'.format(p.pid), filename)
  subprocess.check_output(['xz', '--compress', filename])

