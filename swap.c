// 2017, Georg Sauthoff <mail@gms.tf>, GPLv3+

#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h> // for AT_FDCWD
#ifndef HAVE_RENAMEAT2
  #include <linux/fs.h> // for RENAME_EXCHANGE
  #include <sys/syscall.h>
#endif
#include <unistd.h>

#include "config.h"

static void help(FILE *f, const char *argv0)
{
  fprintf(f, "swap - atomically exchange two filenames\n"
      "\n"
      "Usage: %s [OPTION] SOURCE DEST\n"
      "\n"
      "  -h, --help this screen\n"
      "\n"
      "2017, Georg Sauthoff <mail@gms.tf>\n", argv0);
}

bool help_requested(int argc, char **argv)
{
  for (int i = 1; i < argc; ++i)
    if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
      return true;
  return false;
}

#ifndef HAVE_RENAMEAT2
  // cf. Glibc wrappers for (nearly all) Linux system calls
  // https://lwn.net/Articles/655028/
  static int renameat2(int olddirfd, const char *oldpath,
                       int newdirfd, const char *newpath, unsigned int flags)
  {
    long r = syscall(SYS_renameat2, olddirfd, oldpath, newdirfd, newpath,
        flags);
    return r;
  }

  // e.g. necessary on Ubuntu 14.04 (Trusty)
  #ifndef RENAME_EXCHANGE
    #define RENAME_EXCHANGE         (1 << 1)
  #endif
#endif

int main(int argc, char **argv)
{
  if (help_requested(argc, argv)) {
    help(stdout, argv[0]);
    return 0;
  }
  if (argc != 3) {
    fprintf(stderr, "expecting 2 arguments - cf. --help\n");
    return 2;
  }
  // cf. Exchanging two files
  // https://lwn.net/Articles/569134/
  int r = renameat2(AT_FDCWD, argv[1], AT_FDCWD, argv[2], RENAME_EXCHANGE);
  if (r == -1) {
    perror(0);
    return 1;
  }
  return 0;
}
