#!/usr/bin/env python3

# Randomly generate a locally administered unicast MAC address.
#
# 2017-01, Georg Sauthoff <mail@gms.tf>

from random import randrange, choice

print(hex(randrange(0, 16))[-1]
    + hex(choice(list(i for i in range(16) if i & 0b11 == 0b10)))[-1] + ':'
    + ':'.join('{:02x}'.format(randrange(0, 256))[-2:] for i in range(5)))
