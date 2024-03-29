#!/bin/bash


ofilename="$1"

awk 'BEGIN                       { print "// autogenerated"; }
     /^struct rtnl_link_stats64/ {x=1; next}
     /^};/                       {x=0; next}
     x && /;/                    {
             sub(";", "", $2);
             printf("if (dump_all || s->%s)\n    fprintf(o, \"%%s.%s: %%\" PRIu64 \"\\n\", name, (uint64_t)s->%s);\n", $2, $2, $2);
     }' /usr/include/linux/if_link.h > "$ofilename"
