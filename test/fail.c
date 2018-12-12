#include <stdlib.h>
#include <stdio.h>

#if __GNUC__
    #pragma GCC diagnostic push
    // we want to deliberately crash puts()
    #pragma GCC diagnostic ignored "-Wnonnull"
#endif

int main(int argc, char **argv)
{
  (void)argv;
  if (argc < 2)
    abort();
  else
    puts((const char*)0);
  return 0;
}

#if __GNUC__
    #pragma GCC diagnostic pop
#endif

