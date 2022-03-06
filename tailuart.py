#!/usr/bin/env python3

# Read and timestamp lines from a serial device

# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: © 2022 Georg Sauthoff <mail@gms.tf>


import argparse
import serial
import sys
import time

def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument('--device', '-d', default='/dev/ttyUSB0',
            help='UART device (default: %(default)s)')
    p.add_argument('--baud', '-b', type=int, default=9600,
            help='baud (default: %(default)d)')
    p.add_argument('--flush', '-f', type=bool, default=True,
            help='flush each line (default: %(default)s)')
    return p.parse_args()

def main():
    args = parse_args()
    with serial.Serial(args.device, baudrate=args.baud) as p:
        start = time.time()
        while True:
            line = p.readline().decode().strip()
            t = time.time()
            delta = t - start
            print(f'{delta} {line}', flush=args.flush)

if __name__ == '__main__':
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        pass
