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
        epilog='''

Get even more extensions: https://extensions.gnome.org/

Map extensions.gnome.org extension web ID to UUID:

    $ curl -s "https://extensions.gnome.org/extension-info/?pk=$eid&shell_version=$gsv" | jq -r .uuid

(e.g. for eid=7 and gsv=3.24

Given a UUID trigger the install:

    $ qdbus org.gnome.Shell /org/gnome/Shell org.gnome.Shell.Extensions.InstallRemoteExtension $someuuid

''')
  p.add_argument('--disabled', '-d', action='store_true',
      help='list installed but disabled extensions')
  p.add_argument('--enable', metavar='UUID', help='enable an extension')
  p.add_argument('--disable', metavar='UUID', help='disable an extension')
  return p

def parse_args(*a):
  arg_parser = mk_arg_parser()
  args = arg_parser.parse_args(*a)
  return args

def pp_row(filename):
  if not os.path.exists(filename):
    return
  with open(filename) as f:
    d = json.load(f)
    sys_wide = 'true' if filename.startswith('/usr/share') else 'false'
    print('{},{},{},{}'.format(d['uuid'], d['name'], d['url'], sys_wide))

def get_enabled():
  s = subprocess.check_output(['gsettings', 'get', 'org.gnome.shell', 'enabled-extensions'], universal_newlines=True)
  ls = s[2:-3].split("', '")
  return ls

def list_ext(args):
  home = os.environ['HOME']
  ss = [ home + '/.local/share/gnome-shell/extensions',
      '/usr/share/gnome-shell/extensions' ]
  enabled = set(get_enabled())
  print('uuid,name,url,system')
  for base in ss:
    ls = os.listdir(base)
    for l in sorted(ls):
      if ( args.disabled and l not in enabled ) or (not args.disabled and l in enabled):
        pp_row('{}/{}/metadata.json'.format(base, l))

def toggle_extension(uuid, on):
  ls = get_enabled()
  if on:
    if uuid not in ls:
      ls.append(uuid)
  else:
    if uuid in ls:
      ls.remove(uuid)
  a = '[{}]'.format(', '.join("'{}'".format(x) for x in ls))
  s = subprocess.check_output(['gsettings', 'set', 'org.gnome.shell', 'enabled-extensions', a], universal_newlines=True)

def main(*a):
  args = parse_args(*a)
  if args.enable:
    toggle_extension(args.enable, True)
  elif args.disable:
    toggle_extension(args.disable, False)
  else:
    list_ext(args)
  return 0

if __name__ == '__main__':
  sys.exit(main())
