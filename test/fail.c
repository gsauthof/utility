#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv)
{
  if (argc < 2)
    abort();
  else
    puts((const char*)0);
  return 0;
}
