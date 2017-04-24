#!/usr/bin/env python3

# Pretty print the ASCII table
#
# 2017, Georg Sauthoff <mail@gms.tf>, GPLv3+

import argparse
import collections
import sys

def mk_arg_parser():
  p = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description='Pretty-print ASCII table',
        epilog='...')
  p.add_argument('--cols', '-c', type=int, default=4,
      help='number of columns')
  p.add_argument('--explain', '-x', nargs='?', const='all',
      help='lookup some abbreviation Example: HT')
  return p

def parse_args(*a):
  arg_parser = mk_arg_parser()
  args = arg_parser.parse_args(*a)
  if 128 % args.cols != 0:
    raise RuntimeError('#columns not a factor of #chars - try e.g. 4 or 8')
  return args

# Source: https://en.wikipedia.org/wiki/ASCII
abbr = {
    0   :  ( 'NUL' , 'Null'                          ),
    1   :  ( 'SOH' , 'Start of Heading'              ),
    2   :  ( 'STX' , 'Start of Text'                 ),
    3   :  ( 'ETX' , 'End of Text'                   ),
    4   :  ( 'EOT' , 'End of Transmission'           ),
    5   :  ( 'ENQ' , 'Enquiry'                       ),
    6   :  ( 'ACK' , 'Acknowledgement'               ),
    7   :  ( 'BEL' , 'Bell'                          ),
    8   :  ( 'BS'  , 'Backspace'                     ),
    9   :  ( 'HT'  , 'Horizontal Tab'                ),
    10  :  ( 'LF'  , 'Line Feed'                     ),
    11  :  ( 'VT'  , 'Vertical Tab'                  ),
    12  :  ( 'FF'  , 'Form Feed'                     ),
    13  :  ( 'CR'  , 'Carriage Return'               ),
    14  :  ( 'SO'  , 'Shift Out'                     ),
    15  :  ( 'SI'  , 'Shift In'                      ),
    16  :  ( 'DLE' , 'Data Link Escape'              ),
    17  :  ( 'DC1' , 'Device Control 1 - often XON'  ),
    18  :  ( 'DC2' , 'Device Control 2'              ),
    19  :  ( 'DC3' , 'Device Control 3 - often XOFF' ),
    20  :  ( 'DC4' , 'Device Control 4'              ),
    21  :  ( 'NAK' , 'Negative Acknowledgement'      ),
    22  :  ( 'SYN' , 'Synchronous Idle'              ),
    23  :  ( 'ETB' , 'End of Transmission Block'     ),
    24  :  ( 'CAN' , 'Cancel'                        ),
    25  :  ( 'EM'  , 'End of Medium'                 ),
    26  :  ( 'SS'  , 'Substitute'                    ),
    27  :  ( 'ESC' , 'Escape'                        ),
    28  :  ( 'FS'  , 'File Separator'                ),
    29  :  ( 'GS'  , 'Group Separator'               ),
    30  :  ( 'RS'  , 'Record Separator'              ),
    31  :  ( 'US'  , 'Unit Separator'                ),
    32  :  ( 'SPC' , 'Space'                         ),
    127 :  ( 'DEL' , 'Delete'                        )
}

desc = collections.OrderedDict((v[0], (v[1], k)) for k, v in abbr.items())

def pp_char(p):
  if p < 33 or p == 127:
    return abbr[p][0]
  else:
    return chr(p)

def pp_head(i, w):
  return ('{:0' + str(w) + 'b}').format(i)

def pp_row_head(i, w):
  return ('{:0' + str(w) + 'b}').format(i)

def pp_table(cols=4):
  x = int(128/cols)
  rhw = cols.bit_length()-1
  chw = 7 - cols.bit_length() + 1
  if cols > 1:
    for j in range(cols):
      print('{:>5}'.format(pp_head(j, rhw)), end='')
    print()
  for i in range(x):
    for j in range(cols):
      p = i+j*x
      print('{:>5}'.format(pp_char(p)), end='')
    print('  {}'.format(pp_row_head(i, chw)))

def explain(s):
  if s == 'all':
    for k, (v, i) in desc.items():
      print('{:>3} = {} ({})'.format(k, v, i))
  else:
    print('{:>3} = {} ({})'.format(s, *desc[s]))

def main():
  args = parse_args()
  if args.explain:
    explain(args.explain)
  else:
    pp_table(args.cols)
  return 0

if __name__ == '__main__':
  sys.exit(main())

