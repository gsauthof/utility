#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
  if (argc < 2) {
    fputs("Not enough arguments\n", stderr);
    return 2;
  }
  sleep(atoi(argv[1]));
  return 0;
}
