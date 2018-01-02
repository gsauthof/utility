#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static bool is_expired(struct timespec *a, struct timespec *b, unsigned delta)
{
  return (b->tv_sec == a->tv_sec + delta && b->tv_nsec >= a->tv_nsec)
    || (b->tv_sec > a->tv_sec + delta);
}

int main(int argc, char **argv)
{
  if (argc < 2) {
    fputs("Argument missing.\n", stderr);
    return 2;
  }
  unsigned delta = atoi(argv[1]);
  struct timespec start, cur;
  int r = clock_gettime(CLOCK_MONOTONIC, &start);
  if (r == -1) {
    perror(0);
    return 1;
  }
  for (size_t i = 0; ;++i) {
    printf("%zu\n", i);
    if (i % 1000 == 0) {
      int r = clock_gettime(CLOCK_MONOTONIC, &cur);
      if (r == -1) {
        perror(0);
        return 1;
      }
      if (is_expired(&start, &cur, delta))
        break;
    }
  }
  return 0;
}
