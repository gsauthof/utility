#!/usr/bin/env python3

# Disable/Inhibit Gnome-Shell screen blanking on idle until this program
# terminates.
#
# SPDX-FileCopyrightText: © 2020 Georg Sauthoff <mail@gms.tf>
# SPDX-License-Identifier: GPL-3.0-or-later

import argparse
import pydbus
import sys
import time


# cf. /usr/include/gtk-3.0/gtk/gtkapplication.h
INHIBIT_LOGOUT  = 1
INHIBIT_SWITCH  = 2
INHIBIT_SUSPEND = 4
INHIBIT_IDLE    = 8


def reasons(x):
    rs = []
    if x & INHIBIT_LOGOUT:
        rs.append('LOGOUT')
    if x & INHIBIT_SWITCH:
        rs.append('SWITCH')
    if x & INHIBIT_SUSPEND:
        rs.append('SUSPEND')
    if x & INHIBIT_IDLE:
        rs.append('IDLE')
    return rs

def parse_args(*a):
    p = argparse.ArgumentParser(description='Inhibit screen blanking in Gnome-Shell')
    p.add_argument('--list', '-l', action='store_true',
            help='list active inhibitors')
    p.add_argument('--check', '-c', type=int, metavar='FLAGS',
            help='check for active inhibitors and set exit status accordingly (1|2|4|8 => LOGOUT|SWITCH|SUSPEND|IDLE)')
    p.add_argument('--flags', type=int, metavar='FLAGS', default=INHIBIT_IDLE,
            help='flags to use for inhibit action (1|2|4|8 => LOGOUT|SWITCH|SUSPEND|IDLE) (default: %(default)i)')
    p.add_argument('--time', '-t', type=int, metavar='SECONDS', default=2**32-1,
            help='sleep time, i.e. time the inhibitor is active (default: %(default)i)')
    return p.parse_args(*a)


# def inhibit_old():
#     import dbus
#     bus = dbus.SessionBus()
#     sm = bus.get_object('org.gnome.SessionManager','/org/gnome/SessionManager')
#     sm.Inhibit("coffee", dbus.UInt32(0), "keeps you busy", dbus.UInt32(8),
#             dbus_interface='org.gnome.SessionManager')
#     time.sleep(2**32-1)

def inhibit(flags, secs):
    bus = pydbus.SessionBus()
    # we could leave out object_path because it defaults to bus_name
    # with . replaced by / ...
    sm = bus.get(bus_name='org.gnome.SessionManager',
            object_path='/org/gnome/SessionManager')
    # 'org.gnome.SesssionManager' is also the interface name ...
    xid = 0
    # again, we could leave out interface name, as it defaults to that
    sm['org.gnome.SessionManager'].Inhibit('inhibit.py', xid,
            'user requested idle-off from terminal', flags)
    time.sleep(secs)

def check_inhibited(flags):
    bus = pydbus.SessionBus()
    sm = bus.get(bus_name='org.gnome.SessionManager')
    r = sm.IsInhibited(flags)
    return r

def list_inhibitors():
    bus = pydbus.SessionBus()
    sm = bus.get(bus_name='org.gnome.SessionManager')
    xs = sm.GetInhibitors()
    for x in xs:
        p = bus.get(bus_name='org.gnome.SessionManager', object_path=x)
        print(f'{p.GetAppId()} {p.GetReason()} flags={"|".join(reasons(p.GetFlags()))}')


def main():
    args = parse_args()
    if args.list:
        list_inhibitors()
    elif args.check:
        return(not check_inhibited(args.check))
    else:
        try:
            inhibit(args.flags, args.time)
        except KeyboardInterrupt:
            pass

if __name__ == '__main__':
    sys.exit(main())


