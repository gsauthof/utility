#!/usr/bin/env python3

# detect-size - detect and set real height/width of a terminal
#
# replacement for xterm's resize command for systems that
# don't package it separately (e.g. in xterm-resize as Fedora does)
#
# useful for terminals that are attached to a serial line,
# including emulated ones (e.g. on a VM)
#
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: © 2021 Georg Sauthoff <mail@gms.tf>

import fcntl
import os
import struct
import sys
import termios
import tty

def main():
    # source: https://sources.debian.org/src/xterm/366-1/resize.c/?hl=137#L139
    print('\x1b' '7' '\x1b' '[r' '\x1b' '[9999;9999H' '\x1b' '[6n',
            flush=True, end='')
    
    # cf. https://stackoverflow.com/q/40931467/427158

    bak = termios.tcgetattr(sys.stdin.fileno())
    tty.setcbreak(sys.stdin.fileno())
    r = b''.join(iter(lambda : os.read(sys.stdin.fileno(), 1), b'R'))
    termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, bak)

    i = r.rindex(b'[')
    j = r.index(b';')

    h = int(r[i+1:j])
    w = int(r[j+1:])

    x = struct.pack('HHHH', h, w, 0, 0)

    print(f'\rheight: {h}, width: {w}')

    fcntl.ioctl(sys.stdout, termios.TIOCSWINSZ, x)

    # alternatively: ioctl() on /dev/tty

if __name__ == '__main__':
    sys.exit(main())

