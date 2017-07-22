#!/usr/bin/env python3

# 2017, Georg Sauthoff <mail@gms.tf>

import argparse
import json
import os
import requests
import shutil
import subprocess
import sys
import tempfile
import zipfile

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
  p.add_argument('--enable', metavar='UUIDs', nargs='+',
      help='enable an extension')
  p.add_argument('--disable', metavar='UUIDs', nargs='+',
      help='disable an extension')
  p.add_argument('--pref', metavar='UUID', help='call preferences dialog')
  p.add_argument('--install', metavar='UUIDs', nargs='+',
      help='install one or many extensions. They are downloaded from the official repository and unzipped. See --dest for the destination.')
  p.add_argument('--dest', help='install destination (default: ~/.local/share/gnome-shell/extensions)')
  p.add_argument('--version', '-v', action='store_true',
      help='Display GNOME shell version')
  p.add_argument('--uuid', metavar='URL/ID', help='translate an extensions.gnome.org URL/ID to a UUID')
  return p

def parse_args(*a):
  arg_parser = mk_arg_parser()
  args = arg_parser.parse_args(*a)
  if not args.dest:
    args.dest = os.environ['HOME'] + '/.local/share/gnome-shell/extensions'
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
  ls = ( (base, x) for base in ss for x in os.listdir(base) )
  for base, l in sorted(ls, key=lambda x:x[1]):
    if ( args.disabled and l not in enabled ) \
        or (not args.disabled and l in enabled):
      pp_row('{}/{}/metadata.json'.format(base, l))

def toggle_extension(uuids, on):
  ls = get_enabled()
  for uuid in uuids:
    if on:
      if uuid not in ls:
        ls.append(uuid)
    else:
      if uuid in ls:
        ls.remove(uuid)
  a = '[{}]'.format(', '.join("'{}'".format(x) for x in ls))
  s = subprocess.check_output(['gsettings', 'set', 'org.gnome.shell', 'enabled-extensions', a], universal_newlines=True)

def show_preferences(uuid):
  subprocess.check_output(['qdbus', 'org.gnome.Shell', '/org/gnome/Shell',
      'org.gnome.Shell.Extensions.LaunchExtensionPrefs', uuid])

def gnome_shell_version():
  o = subprocess.check_output(['qdbus', 'org.gnome.Shell',
      '/org/gnome/Shell', 'org.gnome.Shell.Extensions.ShellVersion'],
      universal_newlines=True)
  o = o.strip()
  if o.find('.') != o.rfind('.'):
    o = o[0:o.rfind('.')]
  return o

def download(uuid, version, s, f):
  r = s.get(f'https://extensions.gnome.org/download-extension/{uuid}.shell-extension.zip?&shell_version={version}', stream=True)
  r.raise_for_status()
  for chunk in r.iter_content(8*1024):
    f.write(chunk)
  f.flush()

def verify_zip(z):
  for i in z.infolist():
    if i.filename.startswith('/') or '..' in i.filename:
      raise RuntimeError('Weird filename: ' + i.filename)

def install(uuids, dest):
  v = gnome_shell_version()
  s = requests.Session()
  for uuid in uuids:
    with tempfile.TemporaryFile() as f:
      download(uuid, v, s, f)
      with zipfile.ZipFile(f) as z:
        verify_zip(z)
        d = f'{dest}/{uuid}'
        if os.path.exists(d):
          shutil.rmtree(d)
        os.makedirs(d)
        z.extractall(d)

def parse_id(url_or_id):
  if '/' in url_or_id:
    url = url_or_id
    q = '/extension/'
    a = url.index(q)
    i = url[a + q.__len__():]
    i = i[:i.index('/')]
  else:
    i = url_or_id
  return i

def test_parse_id():
  assert parse_id('https://extensions.gnome.org/extension/15/alternatetab/') \
      == '15'
  assert parse_id('23') == '23'

def get_uuid(url_or_id):
  i = parse_id(url_or_id)
  v = gnome_shell_version()
  r = requests.get(f'https://extensions.gnome.org/extension-info/?pk={i}&shell_version={v}')
  r.raise_for_status()
  d = json.loads(r.text)
  return d['uuid']

def main(*a):
  args = parse_args(*a)
  if args.enable:
    toggle_extension(args.enable, True)
  elif args.disable:
    toggle_extension(args.disable, False)
  elif args.pref:
    show_preferences(args.pref)
  elif args.install:
    install(args.install, args.dest)
  elif args.version:
    print(gnome_shell_version())
  elif args.uuid:
    print(get_uuid(args.uuid))
  else:
    list_ext(args)
  return 0

if __name__ == '__main__':
  sys.exit(main())
