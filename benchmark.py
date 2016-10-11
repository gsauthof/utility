#!/usr/bin/env python3

# 2016, Georg Sauthoff <mail@georg.so>, GPLv3+

import argparse
import collections
import csv
import datetime
import itertools
import logging
# importing it conditionally iff svg generation is selected
# otherwise, it may fail on a system with minimal matplotlib
# install, i.e. where one of the backends loaded by default
# throws
#import matplotlib.pyplot as plt
# importing it conditionally iff csv or not quiet
#import numpy as np
import os
import subprocess
import sys
import tempfile
import time

try:
  import colorlog
  have_colorlog = True
except ImportError:
  have_colorlog = False

def mk_arg_parser():
  p = argparse.ArgumentParser(
      formatter_class=argparse.RawDescriptionHelpFormatter,
      description='run command multiple times and gather stats',
      epilog='''Examples:

Run 3 programs 20 times each and write stats to stdout and the raw
data to a file:

    $ benchmark --cmd ./find_memchr ./find_find --raw raw.dat -n 20 \\
        ./find_unroll2 3000 in

Create boxplot SVG (and nicely format the stdout and also write
the stats to a CSV file):

    $ benchmark --input raw.dat --svg rss.svg --csv rss.csv \\
       | column -t -s, -o ' | '

In case the benchmarked program needs some options the `--` delimiter
has its usual meaning (also explicitly specifiying a tag):

    $ benchmark --tags mode2 -n 1000 -- ./find_unroll2 --mode 2

# 2016, Georg Sauthoff <mail@georg.so>, GPLv3+
'''
      )
  p.add_argument('argv', nargs='*', help='ARG0.. of the child')
  p.add_argument('--cmd', '--cmds', nargs='+', default=[],
      help='extra commands to run')
  p.add_argument('--cols', nargs='+', default=[1,2,3,4],
      help='columns to generate stats for')
  p.add_argument('--csv', nargs='?', const='benchmark.csv',
      help='also write results as csv')
  p.add_argument('--debug', nargs='?', metavar='FILE',
      const='benchmark.log', help='log debug messages into file')
  p.add_argument('--graph-item', help='item to plot in a graph')
  p.add_argument('--height', type=float, help='height of the graph (inch)')
  p.add_argument('--input', '-i', metavar='FILE',
      help='include raw data from a previous run')
  p.add_argument('--items', nargs='+', default=['wall', 'user', 'sys', 'rss'],
      help='names for the selected columns')
  p.add_argument('--null-out', type=bool, default=True,
      help='redirect stdout to /dev/null')
  p.add_argument('--pstat', action=InitPstat,
      help='set options for `perf stat` instead of GNU time')
  p.add_argument('--precision', type=int, default=3,
      help='precision for printing values')
  p.add_argument('--quiet', '-q', action='store_true', default=False,
      help='avoid printing table to stdout')
  p.add_argument('--raw', nargs='?', metavar='FILE', const='data.csv',
      help='write measurement results to file')
  p.add_argument('--repeat', '-n', type=int, default=2,
      help='number of times to repeat the measurement')
  p.add_argument('--sleep', type=float, default=0.0, metavar='SECONDS',
      help='sleep between runs')
  p.add_argument('--svg', nargs='?', const='benchmark.svg',
      help='write boxplot')
  p.add_argument('--tags', nargs='+', default=[],
      help='alternative names for the different commands')
  p.add_argument('--time', default='/usr/bin/time',
      help='measurement program (default: GNU time)')
  p.add_argument('--time-args', nargs='+',
      default=[ '--append', '--format', '%e,%U,%S,%M', '--output', '$<' ],
      help='default arguments to measurement program')
  p.add_argument('--timeout', help='timeout for waiting on a child')
  p.add_argument('--title', help='title of the graph')
  p.add_argument('--width', type=float, help='width of the graph (inch)')
  p.add_argument('--xlabel', default='experiment', help='x-axis label')
  p.add_argument('--xrotate', type=int,
      help='rotate x-labels (default: 75 degrees if more than 4 present')
  p.add_argument('--ylabel', default='time (s)', help='y-axis label')
  p.add_argument('--ymax', type=float,
      help='set upper y-axis limit')
  p.add_argument('--ymin', type=float, default=0.0,
      help='set lower y-axis limit')
  return p

class InitPstat(argparse.Action):
  def __init__(self, option_strings, dest, **kwargs):
    super(InitPstat, self).__init__(
      option_strings, dest, nargs=0, **kwargs)

  def __call__(self, parser, args, values, option_string=None):
    args.time = 'perfstat.sh'
    args.time_args = [ '-o', '$<' ]
    args.cols = list(range(1,12))
    args.items = [ 'nsec','cswitch','cpu_migr','page_fault','cycles','ghz','ins','ins_cyc','br','br_mis','br_mis_rate' ]
    args.graph_item = 'ins_cyc'
    args.title = 'Counter ({})'.format(args.graph_item)
    args.ylabel = 'rate'

def parse_args(xs = None):
  arg_parser = mk_arg_parser()
  if xs or xs == []:
    args = arg_parser.parse_args(xs)
  else:
    args = arg_parser.parse_args()
  if not args.argv and not args.input:
    raise ValueError('Neither cmd+args nor --input option present')
  if args.debug:
    setup_file_logging(args.debug)
  if args.argv:
    args.cmd = [ args.argv[0] ] + args.cmd
    args.argv = args.argv[1:]
  args.cols = [ int(x) for x in args.cols ]
  if args.tags and args.tag.__len__() != args.cmd.__len__():
    raise ValueError('not enough tags specified')
  if not args.tags:
    args.tags = [ os.path.basename(x) for x in args.cmd ]
  if not args.graph_item:
    args.graph_item = args.items[0]
  if not args.title:
    args.title = 'Runtime ({})'.format(args.graph_item)
  if args.svg:
    #import matplotlib.pyplot as plt
    global matplotlib
    global plt
    matplotlib = __import__('matplotlib.pyplot', globals(), locals())
    plt = matplotlib.pyplot
  if args.csv or not args.quiet or args.svg:
    global np
    numpy = __import__('numpy', globals(), locals())
    np = numpy
    #import numpy as np
  return args


log_format      = '%(asctime)s - %(levelname)-8s - %(message)s'
log_date_format = '%Y-%m-%d %H:%M:%S'

def mk_formatter():
  f = logging.Formatter(log_format, log_date_format)
  return f

def mk_logger():
  log = logging.getLogger() # root logger
  log.setLevel(logging.DEBUG)
  #log.setLevel(logging.INFO)

  if have_colorlog:
    cformat   = '%(log_color)s' + log_format
    cf = colorlog.ColoredFormatter(cformat, log_date_format,
        log_colors = { 'DEBUG': 'reset', 'INFO': 'reset',
            'WARNING' : 'bold_yellow' , 'ERROR': 'bold_red',
            'CRITICAL': 'bold_red'})

  else:
    cf = logging.Formatter(log_format, log_date_format)

  ch = logging.StreamHandler()
  ch.setLevel(logging.WARNING)
  if os.isatty(2):
    ch.setFormatter(cf)
  else:
    ch.setFormatter(f)
  log.addHandler(ch)

  return logging.getLogger(__name__)

log = mk_logger()

def setup_file_logging(filename):
  log = logging.getLogger()
  fh = logging.FileHandler(filename)
  fh.setLevel(logging.DEBUG)
  f = logging.Formatter(log_format + ' - [%(name)s]', log_date_format)
  fh.setFormatter(f)
  log.addHandler(fh)

# Reasons for using an external `time` command instead of
# calling e.g. `getrusage()`:
# - the forked child will start
#   with the RSS of the python parent - thus, it will be reported
#   too high if child actually uses less memory
# - same code path as for other measurement tools
# - elapsed time would have to be measured separately, otherwise
def measure(tag, cmd, args):
  errors = 0
  if args.null_out:
    stdout = subprocess.DEVNULL
  else:
    stdout = None
  with tempfile.NamedTemporaryFile(mode='w+', newline='') as temp_file:
    time_args = args.time_args.copy()
    time_args[time_args.index('$<')] = temp_file.name
    a = [ args.time ] + time_args + [cmd] + args.argv
    rc = -1
    with subprocess.Popen(a, stdout=stdout) as p:
      rc = p.wait(timeout=args.timeout)
      if rc != 0:
        log.error('Command {} failed with rc: {}'.format(cmd, rc))
        errors = errors + 1
    reader = csv.reader(temp_file)
    r = [tag] + next(reader)
    r.append(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))
    r.append(rc)
    r.append(cmd)
    r.append(str(args.argv))
    return (r, errors)

def execute(args):
  xs = []
  esum = 0
  for (tag, cmd) in zip(args.tags, args.cmd):
    rs = []
    for i in range(args.repeat):
      try:
        m, errors = measure(tag, cmd, args)
        if args.sleep > 0:
          time.sleep(args.sleep)
        rs.append(m)
        esum = esum + errors
      except StopIteration:
        esum = esum + 1
        log.error("Couldn't read measurements from teporary file"
                + '- {} - {}'.format(tag, i))
    xs.append( (tag, rs) )
  return (xs, esum)

def read_raw(filename):
  with open(filename, 'r', newline='') as f:
    reader = csv.reader(f)
    rs = []
    next(reader)

    xs = [ (k, list(l))
            for (k, l) in itertools.groupby(reader, lambda row: row[0])]
    # is equivalent to:
#    prev = None
#    xs = []
#    l = []
#    for row in reader:
#      if prev != row[0]:
#        l = []
#        xs.append( (row[0], l) )
#      l.append(row)
#      prev = row[0]

    return xs

def write_raw(rrs, args, filename):
  with open(filename, 'a', newline='') as f:
    writer = csv.writer(f)
    writer.writerow(['tag'] + args.items +  ['date', 'rc', 'cmd', 'args' ])
    for rs in rrs:
      for row in rs[1]:
        writer.writerow(row)

def write_svg(ys, args, filename):
  tags, items_l = zip(*ys)
  xrotate = args.xrotate
  if not xrotate and tags.__len__() > 4:
    xrotate = 75
  if args.width and args.height:
    plt.figure(figsize=(args.width, args.height))
  r = plt.boxplot( [ items[args.graph_item] for items in items_l ],
        labels=tags )
  ymax = args.ymax
  if not args.ymax:
    m = np.amax([np.amax(items[args.graph_item]) for items in items_l ])
    ymax = np.ceil(m + (m - args.ymin) / 10)
  plt.ylim(ymin=args.ymin, ymax=ymax)
  plt.title(args.title)
  if xrotate:
    plt.xticks(rotation=xrotate) # 70 # 90
  plt.xlabel(args.xlabel)
  plt.ylabel(args.ylabel)
  plt.tight_layout()
  plt.savefig(filename)

# normally, we would just use a csv.writer() but
# we want to control the number of significant figures
def write_csv(zs, args, f):
  if not zs:
    return
  header = ['tag'] + list(zs[0][1]._fields)
  fstr = '{:1.'+str(args.precision)+'f}'
  print(','.join(header), file=f)
  for (tag, stat) in zs:
    row = [tag] + list(stat)
    srow = []
    for r in row:
      if type(r) is float or type(r) is np.float64:
        srow.append(fstr.format(r))
      else:
        srow.append(str(r))
    print(','.join(srow), file=f)

def get_items(rs, args):
  m = np.zeros(rs.__len__(), dtype=[(x, 'float64') for x in args.items ] )
  i = 0
  for row in rs:
    j = 0
    for c in args.cols:
      m[i][j] = row[c]
      j = j + 1
    i = i + 1
  return m

Stat = collections.namedtuple('Stat',
        ['n', 'min', 'Q1', 'median', 'Q3', 'max', 'mean', 'dev', 'item' ])

def gen_stats(items, args):
  #for name in items.dtype.names:
  name = args.graph_item
  c = items[name]
  ps = np.percentile(c, [25, 50, 75] )
  # there is also np.median()
  s = Stat(n=c.__len__(), min=np.amin(c), Q1=ps[0], median=ps[1],
           Q3=ps[2], max=np.amax(c),
           mean=np.mean(c), dev=np.std(c), item=name)
  return s

def run(args):
  xs = []
  errors = 0
  if args.input:
    xs = xs + read_raw(args.input)
  if args.cmd:
    rxs, errors = execute(args)
    xs = xs + rxs
  if args.csv or not args.quiet or args.svg:
    ys = [ (tag, get_items(rs, args)) for (tag, rs) in xs ]
  if args.csv or not args.quiet:
    zs = [ (tag, gen_stats(items, args)) for (tag, items) in ys ]
  if args.csv:
    with open(args.csv, 'w') as f:
      write_csv(zs, args, f)
  if not args.quiet:
    write_csv(zs, args, sys.stdout)
  if args.raw:
    write_raw(xs, args, args.raw)
  if args.svg:
    write_svg(ys, args, args.svg)
  return int(errors != 0)

def main():
  args = parse_args()
  return run(args)


if __name__ == '__main__':
  sys.exit(main())

