#!/usr/bin/python3

import argparse
from decimal import Decimal
import re
import subprocess
import sys

def mk_arg_parser():
  p = argparse.ArgumentParser(description='Print current CPU frequency in MHz',
          epilog=('This tool reads two CPU performance counters (a.k.a. MSRs)'
              ' two times. The first counter is incremented in proportion'
              ' to the current CPU frequency while the other is incremented'
              ' in proportion to the TSC frequency. Thus, this tool also'
              ' determines the TSC frequency to compute the current CPU frequency'
              ' of each core.'
              " In case the CPU frequency isn't stable between the two reads"
              " the resulting CPU frequency is the average frequency over"
              " the measurement period.")
          )
  p.add_argument('--cpu', '-c',
          help='Only measure on given CPU/Core (default: all)')
  p.add_argument('--delta', '-d', default='1',
          help='Duration between the two MSR reads (default: %(default)s)')
  p.add_argument('--int', '-i', action='store_true',
          help='Just print the integer part of the frequency')
  p.add_argument('--verbose', '-v', action='store_true',
          help='Display header')
  return p

def parse_args(*a):
  arg_parser = mk_arg_parser()
  args = arg_parser.parse_args(*a)
  return args

def read_msr(args):
    aperf = {}
    mperf = {}
    cpus = '--cpu=' + args.cpu if args.cpu else '-a' 
    o = subprocess.check_output(
            ['perf', 'stat', '-e', 'msr/aperf/', '-e', 'msr/mperf/', '-A',
                cpus, '-x', ' ', '--', 'sleep', args.delta],
            stderr=subprocess.STDOUT, universal_newlines=True)
    for l in o.splitlines():
        cpu, v, msr = l.split()[0:3]
        cpu = int(cpu[3:])
        if 'not' in v:
            raise RuntimeError("Can't read MSR - not running as root?")
        v   = int(v)
        if msr.endswith('/aperf/'):
            aperf[cpu] = v
        elif msr.endswith('/mperf/'):
            mperf[cpu] = v
    xs = [ (cpu, aperf[cpu], mperf[cpu]) for cpu in sorted(aperf.keys()) ]
    return xs

tsc_re = re.compile(r'^.* ([0-9]+)\.([0-9]{3}) MHz')

def read_tsc_khz_cmd(cmd):
    khz = None
    with subprocess.Popen(cmd, stdout=subprocess.PIPE,
            universal_newlines=True) as p:
        for line in p.stdout:
            if ' tsc:' in line and ' MHz' in line:
                x, y = tsc_re.match(line).groups()
                khz = x + y
    if not khz:
        raise LookupError('tsc not found')
    return khz


# cf. https://github.com/gsauthof/osjitter/util.c
def read_tsc_khz():
    try:
        return int(open('/sys/devices/system/cpu/cpu0/tsc_freq_khz').read())
    except FileNotFoundError:
        pass
    try:
        return read_tsc_khz_cmd(['journalctl', '-k'])
    except LookupError:
        pass
    return read_tsc_khz_cmd(['dmesg'])


# Sources:
# Len Brown (@intel.com): [PATCH] x86: Calculate MHz using APERF/MPERF
# for cpuinfo and scaling_cur_freq. 2016-04-01, LKML, message id
# 52f711be59539723358bea1aa3c368910a68b46d.1459485198.git.len.brown@intel.com,
# patch was not applied
# https://lore.kernel.org/lkml/52f711be59539723358bea1aa3c368910a68b46d.1459485198.git.len.brown@intel.com/
# Jonathan Corbet (LWN): Frequency-invariant utilization tracking for x86. 2020-04-02, LWN.net
# https://lwn.net/Articles/816388/
def compute_freq(tsc, aperf, mperf):
    return Decimal(tsc) * Decimal(aperf) / Decimal(mperf)

def main():
    args = parse_args()
    tsc_khz = read_tsc_khz()
    xs = read_msr(args)
    q = Decimal('0') if args.int else Decimal('0.00')
    if args.verbose:
        print('CPU MHz')
    for x in xs:
        khz = compute_freq(tsc_khz, *x[1:])
        mhz = khz/Decimal(1000)
        print(f'{x[0]} {mhz.quantize(q)}')

if __name__ == '__main__':
    sys.exit(main())

