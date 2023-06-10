#!/usr/bin/env python3

import sys

def main():
    ofilename = sys.argv[1]

    xs = []
    with open('/usr/include/linux/if_link.h') as f:
        state = 0
        for line in f:
            if line.startswith('struct rtnl_link_stats64'):
                state = 1
            elif line.startswith('};'):
                state = 0
            elif state == 1 and ';' in line:
                ks = line.split()
                xs.append(ks[1].rstrip(';'))
    with open(ofilename, 'w') as f:
        header = ','.join(xs)
        print(f'''static void pp_csv_header(FILE *o)
{{
    fputs("epoch,name,{header}\\n", o);
}}
''', file=f)
        n = len(xs)
        fstr = '",%" PRIu64 ' * n
        fstr = '"%zu,%s"' + fstr + '"\\n"'
        fstr = fstr.replace('""', '')
        args = ', '.join(f'(uint64_t) s->{x}' for x in xs)
        print(f'''static void pp_csv_row(FILE *o, time_t epoch, const char *name, const struct rtnl_link_stats64 *s)
{{
    fprintf(o, {fstr},
            (size_t) epoch, name, {args});
}}
''', file=f)

if __name__ == '__main__':
    sys.exit(main())
