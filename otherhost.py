#!/usr/bin/env drgn

# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: © 2023 Georg Sauthoff <mail@gms.tf>

# hlist_for_each_entry(), netdev_get_by_name(), ...
from drgn.helpers.linux import *
import drgn.helpers.linux.net


# cf. dev_get_stats() in net/core/dev.c:
# https://elixir.bootlin.com/linux/v6.2.15/source/net/core/dev.c#L10431
def rx_otherhost_dropped(dev):
    r = 0
    for i in for_each_present_cpu(prog):
        r += per_cpu_ptr(dev.core_stats, i).rx_otherhost_dropped.value_()
    return r


def for_each_netdev(prog):
    net = prog['init_net']
    for i in range(drgn.helpers.linux.net._NETDEV_HASHENTRIES):
        h = net.dev_index_head[i]
        for d in hlist_for_each_entry("struct net_device", h, "index_hlist"):
            yield d


for d in for_each_netdev(prog):
    name = d.name.string_().decode()
    x = rx_otherhost_dropped(d)
    print(f'{name:15} {x}')


# d = netdev_get_by_name(prog, 'enp4s0u2u1u2')
# print(rx_otherhost_dropped(d))
