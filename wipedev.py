#!/usr/bin/env python3

# SPDX-FileCopyrightText: © 2020 Georg Sauthoff <mail@gms.tf>
# SPDX-License-Identifier: GPL-3.0-or-later

import argparse
import fcntl
import logging
import os
import random
import stat
import struct
import subprocess
import sys


log = logging.getLogger(__name__)

def parse_args(*a):
    p = argparse.ArgumentParser(
            formatter_class=argparse.RawDescriptionHelpFormatter,
            description='Clear device data fast and thoroughly',
            epilog='''
Example:

    wipedev -av /dev/sdz

## How it Works

The pre-wipe step just removes all filesystem, LUKS-encrypted
volume, partition table (etc.) signatures and invokes the drive's
DISCARD command.

Although the main wipe step overwrites everything, the pre-wipe
step gives some protection in case the main wipe step doesn't
complete for some reason. Such as when your device finally dies
or the computer is suddenly stopped. It thus can be seen as part
of a defense-in-depth approach.

The main wipe step overwrites everything with random data. We are
using random data (instead of zeroes) because nowadays some
storage devices (such as some SSDs) internally compress and/or
deduplicate the written data. Since random data doesn't compress
well (if at all) and can't be deduplicated those writes likely
overwrite (almost all) disk blocks/storage cellls.

Note that modern devices often also contain some internal pool of
extra cells to deal with failing cells and optimizing writes.
Thus, even a full rewrite of a device (or even multiple full
rewrites) doesn't guarantee that no data remains in some
internal and usually inaccessible storage cell (which might be
accessed by some proprietary and device specific method). But a
DISCARD followed by a full device write followed by a DISCARD
arguably minimizes the chances that an adversary might extract
any useful from that device, anymore.

The post-wipe step invokes DISCARD again which hides the fact
that random garbage was written to the device, possibly discards
some internal storage cells and possibly speeds up following
writes.

In general, also as part of an defense-in-depth approach, it's
recommended to encrypt fileysstem which may contain any
sensitive data, e.g. with LUKS. The primary goal of the wipe is
then to make the key-slots inaccessible; in case the
user-selected password turns out being not strong enough.

There is some folklore around (magnetic) disk wiping which tells
users to wipe disk drives multiple times in a row (with different
patterns), because after one wipe the previous magnetization
levels might still be recoverable with pecial equipment. This was
probably never true; however since the last decades the
high-densitiy of magnetic storage devices it's certainly
superfluous.

2020, Georg Sauthoff <mail@gms.tf>, GPV3+

'''
            )
    p.add_argument('dev', metavar='DEVICE', nargs=1, help='device to wipe - e.g. /dev/sda')
    p.add_argument('--blocksize', '-b', type=int, default=8*1024*1024, help='write blocksize in bytes (default: %(default)s)')

    p.add_argument('--pre-wipe', '-x', action='store_true',
            help='quickly remove signatures of all partitions and discard everything before overwriting everything')
    p.add_argument('--wipe', '-w', action='store_true',
            help='main wipe, i.e. overwrite everything with random garbage')
    p.add_argument('--post-wipe', '-z', action='store_true',
            help='discard everything after the main wipe')
    p.add_argument('--all', '-a', action='store_true',
            help='apply pre/main/post wipe steps')
    p.add_argument('--verbose', '-v', action='store_true',
            help='verbose output')
    args = p.parse_args(*a)
    args.dev = args.dev[0]
    if args.all:
        args.pre_wipe  = True
        args.wipe      = True
        args.post_wipe = True
    if not (args.wipe or args.pre_wipe or args.post_wipe):
        raise RuntimeError('Specify one, more or all wipe steps')
    return args


def setup_logging(verbose):
    log_format      = '%(asctime)s - %(levelname)-8s - %(message)s [%(name)s]'
    log_date_format = '%Y-%m-%d %H:%M:%S'

    logging.basicConfig(format=log_format, datefmt=log_date_format,
        level=(logging.DEBUG if verbose else logging.INFO) )

def get_size(fd):
    st = os.fstat(fd)

    if stat.S_ISREG(st.st_mode):
        return st.st_size
    elif stat.S_ISBLK(st.st_mode):
        BLKGETSIZE64 = 0x80081272
        b = bytearray(8)
        r = fcntl.ioctl(fd, BLKGETSIZE64, b)
        if r != 0:
            raise RuntimeError('ioctl failed')
        return struct.unpack('Q', b)[0]
    else:
        raise RuntimeError('Unknown file mode')


def get_parts(dev):
    xs = subprocess.check_output(['lsblk', '-o', 'path', '-l', '-n', dev],
            universal_newlines=True).splitlines()
    # make sure that partitions themselves are wiped before the
    # device's partition table itself ...
    xs.reverse()
    return xs

def get_luks(devs):
    if not devs:
        raise RuntimeError('device list must not be empty')
    cmd = ['blkid', '--match-token', 'TYPE=crypto_LUKS', '-o', 'device'] + devs
    p = subprocess.run(cmd, stdout=subprocess.PIPE, universal_newlines=True)
    if p.returncode not in (0, 2):
        raise RuntimeError(
            f'Command {",".join(cmd)} failed with exit status {p.returncode}')
    xs = p.stdout.splitlines()
    return xs

def erase_luks(devs):
    if not devs:
        raise RuntimeError('device list must not be empty')
    log.debug(f'LUKS-erasing {devs} ...')
    subprocess.run(['cryptsetup', '--batch-mode', 'luksErase'] + devs, check=True)

def wipefs(devs):
    if not devs:
        raise RuntimeError('device list must not be empty')
    log.debug(f'Wipefsing {devs} ...')
    subprocess.check_output(['wipefs', '-a'] + devs)

def discard(dev):
    log.debug(f'Discarding {dev} ...')
    subprocess.run(['blkdiscard', dev], check=True)

def pre_wipe(dev):
    parts = get_parts(dev)
    luks = get_luks(parts) if parts else []

    if luks:
        erase_luks(luks)
    wipefs(parts)

    discard(dev)


def main(*a):
    args = parse_args(*a)
    setup_logging(args.verbose)
    if  args.pre_wipe:
        pre_wipe(args.dev)
    if args.wipe:
        with open(args.dev, 'wb', buffering=0) as f:
            n = get_size(f.fileno())
            log.debug(f'Writing {n/1024/1024/1024} GiB random data to {args.dev} ...')
            blocks = n // args.blocksize
            rest   = n %  args.blocksize
            for _ in range(blocks):
                # randbytes() churns buffers (i.e. python
                # objects) but this is (of course) still faster
                # than e.g. copying /dev/urandom to the device
                # ...
                f.write(random.randbytes(args.blocksize))
            f.write(random.randbytes(rest))
    if  args.post_wipe:
        discard(args.dev)

if __name__ == '__main__':
    sys.exit(main())

