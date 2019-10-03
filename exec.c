// 2019, Georg Sauthoff <mail@gms.tf>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void help(FILE *f, const char *argv0)
{
    fprintf(f, "call: %s CMD ARGV0 [ARGV1]...\n"
            "\n"
            "Execute command with a non-default ARGV[0] value.\n"
            "\n"
            "Standalone replacement for e.g.:\n"
            "\n"
            "    bash -c 'exec -a ARGV0 CMD ARGV1'\n"
            "\n"
            , argv0
          );
}

int main(int argc, char **argv)
{
    if (argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
        help(stderr, argv[0]);
        return 0;
    }
    if (argc < 3) {
        help(stderr, argv[0]);
        return 1;
    }
    int r = execvp(argv[1], argv+2);
    if (r == -1) {
        perror("execvp");
        return 127;
    }
    return 0;
}
