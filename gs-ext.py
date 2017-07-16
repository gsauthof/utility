#!/usr/bin/env python3

# 2017, Georg Sauthoff <mail@gms.tf>

import argparse
import json
import os
import subprocess
import sys

def mk_arg_parser():
  p = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description='Manage Gnome-Shell Extensions',
        epilog='...')
  p.add_argument('--disabled', '-d', action='store_true',
      help='list installed but disabled extensions')
  return p

def parse_args(*a):
  arg_parser = mk_arg_parser()
  args = arg_parser.parse_args(*a)
  return args

def pp_row(filename):
  with open(filename) as f:
    d = json.load(f)
    sys_wide = 'true' if filename.startswith('/usr/share') else 'false'
    print('{},{},{},{}'.format(d['uuid'], d['name'], d['url'], sys_wide))

def get_enabled():
  s = subprocess.check_output(['gsettings', 'get', 'org.gnome.shell', 'enabled-extensions'], universal_newlines=True)
  ls = s[2:-2].split("', '")
  return set(ls)

def list_ext(args):
  home = os.environ['HOME']
  ss = [ home + '/.local/share/gnome-shell/extensions',
      '/usr/share/gnome-shell/extensions' ]
  enabled = get_enabled()
  print('uuid,name,url,system')
  for base in ss:
    ls = os.listdir(base)
    for l in sorted(ls):
      if ( args.disabled and l not in enabled ) or (not args.disabled and l in enabled):
        pp_row('{}/{}/metadata.json'.format(base, l))

def main(*a):
  args = parse_args(*a)
  list_ext(args)
  return 0

if __name__ == '__main__':
  sys.exit(main())
