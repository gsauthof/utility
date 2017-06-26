#!/usr/bin/env python3

# 2017, Georg Sauthoff <mail@gms.tf>, GPLv3+

import argparse
import glob
import json
import os
import sys

home = os.environ['HOME']

def mk_arg_parser():
  p = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description='List installed chromium extensions',
        epilog='''

2017, Georg Sauthoff <mail@gms.tf>, GPLv3+''')
  p.add_argument('--output', '-o',
      metavar='CSVFILE', help='store extension list to a file (default: STDOUT)')
  return p

def parse_args(*a):
  arg_parser = mk_arg_parser()
  args = arg_parser.parse_args(*a)
  args.o = open(args.output, 'w') if args.output else sys.stdout
  return args

def mk_slug(s):
  s = s.lower()
  s = s.replace(' ', '-')
  return s

def website_in_package(guid):
  ps = glob.glob(home
      + '/.config/chromium/Default/Extensions/{}/*/package.json'.format(
        guid))
  if ps:
    d = json.load(open(ps[-1]))
    if 'repository' in d:
      r = d['repository']
      if 'url' in r:
        return r['url']

def website_in_manifest(guid):
  ms = glob.glob(home
      + '/.config/chromium/Default/Extensions/{}/*/manifest.json'.format(
        guid))
  if ms:
    d = json.load(open(ms[-1]))
    if 'homepage_url' in d:
      return d['homepage_url']

def website(guid):
  url = website_in_package(guid)
  if not url:
    url = website_in_manifest(guid)
  if not url:
    url = ''
  return url

def run(args):
  prefs = json.load(open(home + '/.config/chromium/Default/Preferences'))
  settings = prefs['extensions']['settings']
  es = sorted(filter(lambda x:x[1]['was_installed_by_default'] == False,
         filter(lambda x:x[1]['path'].startswith('/') == False,
           filter(lambda x:'manifest' in x[1], settings.items()))),
         key=lambda x:x[1]['manifest']['name'])
  print('chrome_url,name,guid,url', file=args.o)
  for guid, v in es:
      name = v['manifest']['name']
      chrome_url = 'https://chrome.google.com/webstore/detail/{}/{}'\
          .format(mk_slug(name), guid)
      url = website(guid)
      print('{},{},{},{}'.format(chrome_url, name, guid, url),
        file=args.o)
  args.o.flush()

def main(*argv):
  args = parse_args(*argv)
  return run(args)

if __name__ == '__main__':
  sys.exit(main())

