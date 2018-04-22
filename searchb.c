// 2018, Georg Sauthoff <mail@gms.tf>
// SPDX-License-Identifier: GPL-3.0-or-later

#define _GNU_SOURCE

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static void *mmap_file(const char *filename, size_t *n)
{
  static unsigned char empty[0];
  int fd = open(filename, O_RDONLY);
  if (fd == -1)
    return 0;
  struct stat st;
  int r = fstat(fd, &st);
  if (r == -1) {
    close(fd);
    return 0;
  }
  *n = st.st_size;
  if (!*n) {
    close(fd);
    return empty;
  }
  void *p = mmap(0, *n, PROT_READ, MAP_SHARED, fd, 0);
  if (p == (void*)-1) {
    close(fd);
    return 0;
  }
  r = close(fd);
  if (r == -1) {
    munmap(p, *n);
    return 0;
  }
  return p;
}

int main(int argc, char **argv)
{
  if (argc < 3) {
    fprintf(stderr, "call: %s queryfile file\n", argv[0]);
    return 1;
  }
  size_t m;
  const void *q = mmap_file(argv[1], &m);
  if (!q) {
    perror(0);
    return 1;
  }
  size_t n;
  const void *t = mmap_file(argv[2], &n);
  if (!q) {
    perror(0);
    return 1;
  }
  const void *p = memmem(t, n, q, m);
  if (!p)
    return 1;
  printf("%zd\n", p-t);
  return 0;
}
