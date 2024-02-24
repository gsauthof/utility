#!/usr/bin/python3 -E

# health check mail delivery mail filter
#
# SPDX-FileCopyrightText: © 2020 Georg Sauthoff <mail@gms.tf>
# SPDX-License-Identifier: GPL-3.0-or-later

import argparse
import email.message
import email.policy
# provided by Fedora package: python3-pynacl
import nacl.encoding
import nacl.signing
import nacl.exceptions
import re
import smtplib
import sys
import time


result_header    = 'x-hc-result'
signature_header = 'x-hc-signature'
loop_header      = 'x-hc-loop'
hc_result_re     = re.compile(result_header    + ':', re.IGNORECASE)
hc_sig_re        = re.compile(signature_header + ':', re.IGNORECASE)
hc_loop_re       = re.compile(loop_header      + ':', re.IGNORECASE)


def verify(vkey, delta, s):
    try:
        m = vkey.verify(s, encoder=nacl.encoding.HexEncoder)
    except nacl.exceptions.BadSignatureError:
        return False
    now = int(time.time())
    ts  = int(m)
    d   = now - ts
    if d < -1 or d > delta:
        return False
    return True 

def process(f, vkey, delta):
    state    = 0
    verified = False
    loop     = 1
    for line in f:
        if state == 0:
            if line == '\n':
                state = 1
                print(f'{loop_header}: {loop}')
                if verified:
                    print(f'{result_header}: ok')
                print()
                continue
            elif hc_result_re.match(line):
                continue
            elif hc_loop_re.match(line):
                loop += 1
                continue
            elif hc_sig_re.match(line):
                _, v     = line.split(maxsplit=2)
                verified = verify(vkey, delta, v)
                print(line, end='')
            else:
                pass
                print(line, end='')
        else:
            print(line, end='')

def parse_args():
  p = argparse.ArgumentParser(description='Mail filter and verify signature')
  p.add_argument('--input', '-i', default='-',
                 help='input file (default: - for stdin)')
  p.add_argument('--sign', action='store_true', help='sign current time of day')
  p.add_argument('--mail', action='store_true', help='sign and mail')
  p.add_argument('--to', help='mail recipient')
  p.add_argument('--from', dest='frm', help='mail sender')
  p.add_argument('--subject', default='1337 health check beacon', help='mail subject (default: %(default)s)')
  p.add_argument('--mta', default='localhost', help='mail server to use with --mail (default: %(default)s)')
  p.add_argument('key', metavar='HEXKEY', help='signature verification key (or gen-key for key generation)')
  p.add_argument('--delta', '-d', type=int, default=10,
                 help='timestamp delta window in seconds during verification (default: %(default)d')
  return p.parse_args()

def gen_key():
    skey = nacl.signing.SigningKey.generate()
    vkey = skey.verify_key
    print(skey.encode(nacl.encoding.HexEncoder).decode())
    print(vkey.encode(nacl.encoding.HexEncoder).decode())

def sign(key):
    skey = nacl.signing.SigningKey(key, encoder=nacl.encoding.HexEncoder)
    m = skey.sign(str(int(time.time())).encode(), encoder=nacl.encoding.HexEncoder)
    return m.decode()

def mail(frm, to, subject, signature, host):
    # to simplify things we don't want our signature_header to get folded,
    # thus the policy change
    p = email.policy.EmailPolicy().clone(max_line_length=200)
    m = email.message.EmailMessage(policy=p)
    m.set_content('health check beacon')
    m['from']           = frm
    m['to']             = to
    m['subject']        = subject
    m[signature_header] = signature
    with smtplib.SMTP(host) as mta:
        mta.send_message(m)

def main():
    args = parse_args()
    if args.key in ('gen-key', 'genkey'):
        gen_key()
        return
    if args.sign:
        print(sign(args.key))
        return
    if args.mail:
        mail(args.frm, args.to, args.subject, sign(args.key), args.mta)
        return
    if args.input == '-':
        f = sys.stdin
    else:
        f = open(args.input)
    vkey = nacl.signing.VerifyKey(args.key, encoder=nacl.encoding.HexEncoder)
    process(f, vkey, args.delta)

if __name__ == '__main__':
    sys.exit(main())



