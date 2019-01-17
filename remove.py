#!/usr/bin/env python3

# 2018, Georg Sauthoff <mail@gms.tf>
# SPDX-License-Identifier: GPL-3.0-or-later

import argparse
import os
import time
import subprocess
import sys

# cf. https://unix.stackexchange.com/q/444611/1131
def remove(dev):
    filename = '/sys/block/{}/device/../../../../remove'.format(dev)
    with open(filename, 'wb', buffering=0) as f:
        f.write(b'1\n')

# probably also tirggered by the remove, just being paranoid here
# the flushbufs generally makes sense when there were some raw writes
# to the device
def flush(dev):
    subprocess.check_output(['blockdev', '--flushbufs', dev])

def detach(filename):
    devname = os.path.realpath(filename)
    dev = os.path.basename(devname)
    flush(devname)
    # in case the remove isn't properly implemented/paranoia
    time.sleep(13)
    remove(dev)
    try:
        os.stat(devname)
        raise RuntimeError(
                'Device {} still present after removal'.format(devname))
    except FileNotFoundError:
        pass
    time.sleep(3)

def main():
    p = argparse.ArgumentParser(
            description='Sync USB drive cache, power-off and remove device ')
    p.add_argument('filenames', metavar='DEVICE_PATH', nargs='+',
            help='path to USB disk device (e.g. /dev/sdb)')
    args = p.parse_args()
    try:
        for filename in args.filenames:
            detach(filename)
    except Exception as e:
        print('error: {}'.format(e), file=sys.stderr)
        return 1

if __name__ == '__main__':
    sys.exit(main())
