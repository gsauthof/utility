#!/usr/bin/env python3

# Send ICMP ping requests out of an interface
# with arbitrary destination ethernet adress
# and arbitrary IP source and destination addresses.
#
# Useful for example for testing switch unicast flooding.
#
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: © 2023 Georg Sauthoff <mail@gms.tf>

import dpkt
import socket
import sys

def mk_icmp_reply(eth_src, eth_dst, ip_src, ip_dst):
    ic = dpkt.icmp.ICMP(
            type = dpkt.icmp.ICMP_ECHO,
            data = dpkt.icmp.ICMP.Echo(
                id  = 23,
                seq = 42,
                data = b'Hello World!'
                )
            )
    ip = dpkt.ip.IP(
            id = 0,
            src = ip_src,
            dst = ip_dst,
            p = dpkt.ip.IP_PROTO_ICMP,
            data = ic
            )
    e = dpkt.ethernet.Ethernet(
            src = eth_src,
            dst = eth_dst,
            type = dpkt.ethernet.ETH_TYPE_IP,
            data = ip
            )
    return bytes(e)


def encode_eth_addr(s):
    xs = s.split(':')
    b = bytes( int(x, 16) for x in xs)
    return b

def encode_ip_addr(s):
    xs = s.split('.')
    b = bytes( int(x) for x in xs)
    return b

def if_eth_addr(name):
    s = open(f'/sys/class/net/{name}/address').read().strip()
    bs = encode_eth_addr(s)
    return bs

def send_frame(iface, frame):
    s = socket.socket(socket.AF_PACKET, socket.SOCK_RAW)
    s.bind((iface, 0))
    s.send(frame)

def main():
    iface = sys.argv[1]
    eth_src = if_eth_addr(iface)
    eth_dst = encode_eth_addr(sys.argv[2])
    ip_src = encode_ip_addr(sys.argv[3])
    ip_dst = encode_ip_addr(sys.argv[4])
    frame = mk_icmp_reply(eth_src, eth_dst, ip_src, ip_dst)
    print(frame.hex())

    send_frame(iface, frame)


if __name__ == '__main__':
    sys.exit(main())
