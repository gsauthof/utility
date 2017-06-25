#!/usr/bin/env python3

# 2017, Georg Sauthoff <mail@gms.tf>, GPLv3+

import argparse
import csv
import glob
import json
import logging
import os
import requests
import sys

def setup_logging():
  log_format      = '%(levelname)-8s - %(message)s'
  logging.basicConfig(format=log_format, level=logging.INFO)

log = logging.getLogger(__name__)

def mk_arg_parser():
  p = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description='List installed Firefox addons',
        epilog='''Examples:

Generate the list of installed addons:

    $ ./firefox-addons.py -o addons.csv

Open all addon pages in a Firefox window (e.g. for bulk installation):

    $ cut -f1 -d, addon.csv | tail -n +2 | xargs firefox

2017, Georg Sauthoff <mail@gms.tf>, GPLv3+''')
  p.add_argument('--profile',
      metavar='DIRECTORY', help='Specify the default directory (default: use the newestet one unter ~/.mozilla/firefox that ends in .default')
  p.add_argument('--output', '-o', default='addon.csv',
      metavar='CSVFILE', help='information about the installed addons (default: addon.csv)')
  return p

def base():
  return os.environ['HOME'] + '/.mozilla/firefox'

def default_profile():
  b = base()
  return max(
      ( (fn, os.stat(fn).st_mtime)
           for fn in glob.glob(b + '/*.default') ),
      key=lambda x: x[1])[0]

def parse_args(*a):
  arg_parser = mk_arg_parser()
  args = arg_parser.parse_args(*a)
  if not args.profile:
    args.profile = default_profile()
    log.info('Using profile: {}'.format(args.profile))
  return args

def args_exts(profile):
  a = json.load(open(profile + '/addons.json'))
  a = a['addons']
  e = json.load(open(profile + '/extensions.json'))
  e = e['addons']
  e = dict((a['id'], a) for a in e)
  return (a, e)

# cf. http://addons-server.readthedocs.io/en/latest/topics/api/addons.html#detail
def details(slug, session):
  r = session.get('https://addons.mozilla.org//api/v3/addons/addon/{}/'
      .format(slug))
  r.raise_for_status()
  d = json.loads(r.text)
  return d

def run(args):
  session = requests.Session()
  addons, exts = args_exts(args.profile)
  with open(args.output, 'w', newline='') as f:
    w = csv.writer(f, lineterminator=os.linesep)
    w.writerow(['mozilla_url', 'slug', 'guid', 'name',
        'compatible_android', 'compatible_57', 'url' ])
    for a in addons:
      if not exts[a['id']]['active']:
        continue
      mozilla_url = a['learnmoreURL']
      mozilla_url = mozilla_url.replace('?src=api', '')
      slug = mozilla_url.split('/')[-2]
      guid = a['id']
      d = details(slug, session)
      compatible_android = str(
          'android' in d['current_version']['compatibility']).lower()
      compatible_57 = str('firefox57' in d['tags']).lower()
      url = a['homepageURL']
      url = url.replace('?src=api', '')
      w.writerow([ mozilla_url, slug, guid, a['name'],
          compatible_android, compatible_57, url ])

def imain(*argv):
  args = parse_args(*argv)
  return run(args)

def main(*argv):
  setup_logging()
  return imain(*argv)

if __name__ == '__main__':
  sys.exit(main())

