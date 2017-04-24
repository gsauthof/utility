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

if __name__ != '__main__':
  import io
  import unittest.mock as mock

def test_default_table():
  with mock.patch('sys.stdout', new=io.StringIO()) as fake_out:
    main([])
    assert fake_out.getvalue() == '''   00   01   10   11
  NUL  SPC    @    `  00000
  SOH    !    A    a  00001
  STX    "    B    b  00010
  ETX    #    C    c  00011
  EOT    $    D    d  00100
  ENQ    %    E    e  00101
  ACK    &    F    f  00110
  BEL    '    G    g  00111
   BS    (    H    h  01000
   HT    )    I    i  01001
   LF    *    J    j  01010
   VT    +    K    k  01011
   FF    ,    L    l  01100
   CR    -    M    m  01101
   SO    .    N    n  01110
   SI    /    O    o  01111
  DLE    0    P    p  10000
  DC1    1    Q    q  10001
  DC2    2    R    r  10010
  DC3    3    S    s  10011
  DC4    4    T    t  10100
  NAK    5    U    u  10101
  SYN    6    V    v  10110
  ETB    7    W    w  10111
  CAN    8    X    x  11000
   EM    9    Y    y  11001
   SS    :    Z    z  11010
  ESC    ;    [    {  11011
   FS    <    \    |  11100
   GS    =    ]    }  11101
   RS    >    ^    ~  11110
   US    ?    _  DEL  11111
'''

def test_8col_table():
  with mock.patch('sys.stdout', new=io.StringIO()) as fake_out:
    main(['-c', '8'])
    assert fake_out.getvalue() == '''  000  001  010  011  100  101  110  111
  NUL  DLE  SPC    0    @    P    `    p  0000
  SOH  DC1    !    1    A    Q    a    q  0001
  STX  DC2    "    2    B    R    b    r  0010
  ETX  DC3    #    3    C    S    c    s  0011
  EOT  DC4    $    4    D    T    d    t  0100
  ENQ  NAK    %    5    E    U    e    u  0101
  ACK  SYN    &    6    F    V    f    v  0110
  BEL  ETB    '    7    G    W    g    w  0111
   BS  CAN    (    8    H    X    h    x  1000
   HT   EM    )    9    I    Y    i    y  1001
   LF   SS    *    :    J    Z    j    z  1010
   VT  ESC    +    ;    K    [    k    {  1011
   FF   FS    ,    <    L    \    l    |  1100
   CR   GS    -    =    M    ]    m    }  1101
   SO   RS    .    >    N    ^    n    ~  1110
   SI   US    /    ?    O    _    o  DEL  1111
'''

def test_explain():
  with mock.patch('sys.stdout', new=io.StringIO()) as fake_out:
    main(['-x', 'EOT'])
    assert fake_out.getvalue() == 'EOT = End of Transmission (4)\n'

def test_explain_all():
  with mock.patch('sys.stdout', new=io.StringIO()) as fake_out:
    main(['-x'])
    o = fake_out.getvalue()
    assert 'EOT = End of Transmission (4)\n' in o
    assert ' RS = Record Separator (30)' in o
    assert  o.splitlines().__len__() == 34

def main(*a):
  args = parse_args(*a)
  if args.explain:
    explain(args.explain)
  else:
    pp_table(args.cols)
  return 0

if __name__ == '__main__':
  sys.exit(main())

