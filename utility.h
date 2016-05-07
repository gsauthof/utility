#ifndef UTILITY_H
#define UTILITY_H

#define check_exit(r, s) do { if (r == -1) { perror(s); exit(1); } } while (0)

#endif // UTILITY_H
