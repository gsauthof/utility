#!/usr/bin/env python3

# Collect metering data from a Netio power sockets into CSV files.
#
# SPDX-FileCopyrightText: © 2020 Georg Sauthoff <mail@gms.tf>
# SPDX-License-Identifier: GPL-3.0-or-later

import argparse
import collections
import configparser
import json
import logging
import os
import requests
import signal
import sys
import time

log = logging.getLogger(__name__)


def parse_args(*xs):
    p = argparse.ArgumentParser()
    p.add_argument('--config', '-c', default=os.getenv('HOME', '.') + '/.config/netio.ini',
            help='config file for netio settings (default: %(default)s)')
    p.add_argument('--socket', '-s', dest='sockets', type=int, nargs='*', default=[1],
            help='power socket number(s) (default: %(default)s)')
    p.add_argument('--interval', '-i', type=int, default=1,
            help='collection interval (default: %(default)d)')
    p.add_argument('-n', type=int, default=-1,
            help='collect n times (default: %(default)d)')
    p.add_argument('--output', '-o', default='output.csv',
            help='output filename (default: %(default)s)')
    args = p.parse_args(*xs)
    return args

def parse_conf(filename):
    conf = configparser.ConfigParser()
    conf.read(filename)
    Config = collections.namedtuple('Config', ['user', 'password', 'host'])
    return Config(conf['netio'].get('user', None), conf['netio'].get('password', None),
            conf['netio']['host'])

def set_proc_name(name):
    if not os.path.exists('/proc/self/comm'):
        return
    with open(f'/proc/self/comm', 'w') as f:
        f.write(name)

def measure_one(url, session, auth, args, f):
    r = requests.get(url, **auth)

    d = json.loads(r.text, parse_float=str, parse_int=str)

    xs = [ str(int(time.time())), d['Agent']['DeviceName'] ]
    xs.extend(d['GlobalMeasure'][n] for n in ('Voltage', 'Frequency'))

    for k in args.sockets:
        xs.extend(d['Outputs'][k-1][n] for n in ('State', 'Current', 'PowerFactor'))

    print(','.join(xs), file=f)


def measure(url, session, auth, args, f):
    i = 0

    hs = [ 's', 'name', 'V', 'Hz' ]
    for k in args.sockets:
        # pf == true power factor -> https://en.wikipedia.org/wiki/Power_factor
        hs.extend(f'{n}_{k}' for n in ('state', 'mA', 'pf'))
    print(','.join(hs), file=f)

    while i != args.n:

        try:
            measure_one(url, session, auth, args, f)
        except Exception as e:
            log.error(f'measurement failed: {e}')

        time.sleep(args.interval)
        if args.n > 0:
            i += 1

class Sig_Term(Exception):
    pass

class Sig_Hup(Exception):
    pass

def terminate(signo, frame):
    raise Sig_Term()

def hup(signo, frame):
    raise Sig_Hup()

def main():
    args = parse_args()
    conf = parse_conf(args.config)
    set_proc_name('netio2csv')
    signal.signal(signal.SIGTERM, terminate)
    signal.signal(signal.SIGHUP, hup)

    session = requests.Session()
    auth = {}
    if conf.password:
        auth['auth'] = (conf.user, conf.password)
    url = f'{conf.host}/netio.json'

    while True:
        try:
            with open(args.output, 'w') as f:
                measure(url, session, auth, args, f)
            break
        except Sig_Hup:
            pass

if __name__ == '__main__':
    try:
        sys.exit(main())
    except (KeyboardInterrupt, Sig_Term):
        print()

