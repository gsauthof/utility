// pargs - print process arguments and other vectors
//
// inspired by Solaris' `pargs` command
//
// 2017, Georg Sauthoff <mail@gms.tf>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <ctype.h> // isdigit()
#include <errno.h>
#include <elf.h> // for AT_* defines
#include <fcntl.h> // open()
#include <inttypes.h>
#include <sys/mman.h> // mmap(), munmap()
#include <sys/ptrace.h> // ptrace()
#include <sys/stat.h> // stat(), open()
#include <sys/types.h> // waitid(), open()
#include <sys/wait.h> // waitid()
#include <stdbool.h>
#include <stdio.h> // perror()
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // close()
#include <stdarg.h> // va_start(), va_end()

static bool debug_enabled;

static void debug(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void debug(const char *fmt, ...)
{
  if (!debug_enabled)
    return;
  int r;
  va_list ap;
  va_start(ap, fmt);
  r = vfprintf(stderr, fmt, ap);
  (void)r;
  fputc('\n', stderr);
  va_end(ap);
}

struct Args {
  bool print_argv;
  bool print_auxv;
  bool print_cmdline;
  bool print_envp;
  bool verbose;
  bool stop_process;
};
typedef struct Args Args;

static bool is_all_num(const char *s)
{
  for (; *s; ++s) {
    if (!isdigit(*s))
      return false;
  }
  return true;
}

static void print_help(FILE *o, const char *argv0)
{
  fprintf(o, "Usage: %s [OPTION]... {PID|CORE}\n"
      "Display arguments and other vectors of a process.\n\n"
      "  -a         print process arguments (argv, argument vector)\n"
      "  -d         enable debug output (when reading core files)\n"
      "  -e         print environment variables (envp, environment vector)\n"
      "  -h,--help  display this help text and exit\n"
      "  -l         print command line\n"
      "  -s         attach/detach process to stop process during -x reads\n"
      "             (for Linux < 3.2)\n"
      "  -x         print auxillary vector\n"
      "  -v         verbose mode\n"
      "\n\n"
      "2017, Georg Sauthoff <mail@gms.tf>, GPLv3+\n"
      , argv0);
}

// return_value != 0: caller should exit() with (return_value - 1)
static int parse_args(int argc, char **argv, Args *args)
{
  *args = (Args){0};
  bool more_opts = true;
  for (int i = 1; i < argc; ++i) {
    if (more_opts && *argv[i] == '-') {
      const char *a = argv[i] + 1;
      if (*a == '-') {
        if (!a[1]) {
          more_opts = false;
          continue;
        } else if (!strcmp(a + 1, "help")) {
          print_help(stdout, *argv);
          return 1;
        } else {
          fprintf(stderr, "Unknown argument: %s\n", argv[i]);
          return 3;
        }
      }
      for (; *a; ++a) {
        switch (*a) {
          case 'a': args->print_argv    = true; break;
          case 'd': debug_enabled       = true; break;
          case 'e': args->print_envp    = true; break;
          case 'h': print_help(stdout, *argv);
                    return 1;
          case 'l': args->print_cmdline = true; break;
          case 's': args->stop_process  = true; break;
          case 'x': args->print_auxv    = true; break;
          case 'v': args->verbose       = true; break;
          default: fprintf(stderr, "Unknown argument: -%c\n", *a);
                   return 3;
        }
      }
    } else if (is_all_num(argv[i])) {
      if (strlen(argv[i]) > 20) {
        fputs("PID is too long.\n", stderr);
        return 3;
      }
      continue; // PID
    } else {
      continue; // core file
    }
  }
  if (args->print_cmdline && (args->print_auxv || args->print_envp)) {
    fputs("-l is incompatible with -x and -e\n", stderr);
    return 3;
  }
  if (!args->print_argv && !args->print_auxv && !args->print_cmdline
      && !args->print_envp)
    args->print_argv = true;
  return 0;
}

static int fput_proc_file(FILE *f,
    const char *prefix, const char *delim, FILE *o)
{
  char *line = 0;
  size_t n = 0;
  for (size_t i = 0;; ++i) {
    errno = 0;
    ssize_t r = getdelim(&line, &n, 0, f);
    if (r == -1)
      break;
    if (i)
      fputs(delim, o);
    if (prefix)
      fprintf(o, "%s[%zu]: ", prefix, i);
    fputs(line, o);
  }
  if (errno) {
    perror("read error");
    free(line);
    return -1;
  }
  free(line);
  return 0;
}

static int fput_proc_vector(const char *pid, const char *name,
    const char *prefix, const char *delim, FILE *o)
{
  char filename[64];
  sprintf(filename, "/proc/%s/%s", pid, name);
  FILE *f = fopen(filename, "r");
  if (!f) {
    perror(0);
    return -1;
  }
  int r = fput_proc_file(f, prefix, delim, o);
  if (r) {
    fclose(f);
    return r;
  }
  r = fclose(f);
  if (r) {
    perror(0);
    return -1;
  }
  return 0;
}

static int fput_header(const char *pid, FILE *o)
{
  fprintf(o, "%s: ", pid);
  return fput_proc_vector(pid, "cmdline", 0, " ", o);
}

struct Auxv_Type {
  const char *key;
  const char *desc;
};
typedef struct Auxv_Type Auxv_Type;
// AT info copied from glibc's /usr/include/elf.h
// cf. glibc-devel-2.25-12.fc26.x86_64
static Auxv_Type auxv_type_map[] = {
  /* 0  */ { "AT_NULL"           ,  "End of vector"                     },
  /* 1  */ { "AT_IGNORE"         ,  "Entry should be ignored"           },
  /* 2  */ { "AT_EXECFD"         ,  "File descriptor of program"        },
  /* 3  */ { "AT_PHDR"           ,  "Program headers for program"       },
  /* 4  */ { "AT_PHENT"          ,  "Size of program header entry"      },
  /* 5  */ { "AT_PHNUM"          ,  "Number of program headers"         },
  /* 6  */ { "AT_PAGESZ"         ,  "System page size"                  },
  /* 7  */ { "AT_BASE"           ,  "Base address of interpreter"       },
  /* 8  */ { "AT_FLAGS"          ,  "Flags"                             },
  /* 9  */ { "AT_ENTRY"          ,  "Entry point of program"            },
  /* 10 */ { "AT_NOTELF"         ,  "Program is not ELF"                },
  /* 11 */ { "AT_UID"            ,  "Real uid"                          },
  /* 12 */ { "AT_EUID"           ,  "Effective uid"                     },
  /* 13 */ { "AT_GID"            ,  "Real gid"                          },
  /* 14 */ { "AT_EGID"           ,  "Effective gid"                     },
  /* 15 */ { "AT_PLATFORM"       ,  "String identifying platform"       },
  /* 16 */ { "AT_HWCAP"          ,  "CPU capabilities hints"            },
  /* 17 */ { "AT_CLKTCK"         ,  "Frequency of times()"              },
  /* 18 */ { "AT_FPUCW"          ,  "Used FPU control word"             },
  /* 19 */ { "AT_DCACHEBSIZE"    ,  "Data cache block size"             },
  /* 20 */ { "AT_ICACHEBSIZE"    ,  "Instruction cache block size"      },
  /* 21 */ { "AT_UCACHEBSIZE"    ,  "Unified cache block size"          },
  /* 22 */ { "AT_IGNOREPPC"      ,  "Entry should be ignored"           },
  /* 23 */ { "AT_SECURE"         ,  "Boolean, was exec setuid-like?"    },
  /* 24 */ { "AT_BASE_PLATFORM"  ,  "String identifying real platforms" },
  /* 25 */ { "AT_RANDOM"         ,  "Address of 16 random bytes"        },
  /* 26 */ { "AT_HWCAP2"         ,  "More CPU capabilities hints"       },
  /* 27 */ { "unk_27"            ,  ""                                  },
  /* 28 */ { "unk_28"            ,  ""                                  },
  /* 29 */ { "unk_29"            ,  ""                                  },
  /* 30 */ { "unk_30"            ,  ""                                  },
  /* 31 */ { "AT_EXECFN"         ,  "Filename of executable"            },
  /* 32 */ { "AT_SYSINFO"        ,  ""                                  },
  /* 33 */ { "AT_SYSINFO_EHDR"   ,  ""                                  },
  /* 34 */ { "AT_L1I_CACHESHAPE" ,  ""                                  },
  /* 35 */ { "AT_L1D_CACHESHAPE" ,  ""                                  },
  /* 36 */ { "AT_L2_CACHESHAPE"  ,  ""                                  },
  /* 37 */ { "AT_L3_CACHESHAPE"  ,  ""                                  }
};

// copied from Linux's arch/x86/include/asm/cpufeatures.h
// i.e. Linux kernel 4.14 source
// cf. https://unix.stackexchange.com/questions/43539/what-do-the-flags-in-proc-cpuinfo-mean
// LWN: getauxval() and the auxiliary vector (2012)
// https://lwn.net/Articles/519085/
static Auxv_Type x86_hwcap_map[] = {
  /* 0  */ { "fpu"     ,  "Onboard FPU"                   },
  /* 1  */ { "vme"     ,  "Virtual Mode Extensions"       },
  /* 2  */ { "de"      ,  "Debugging Extensions"          },
  /* 3  */ { "pse"     ,  "Page Size Extensions"          },
  /* 4  */ { "tsc"     ,  "Time Stamp Counter"            },
  /* 5  */ { "msr"     ,  "Model-Specific Registers"      },
  /* 6  */ { "pae"     ,  "Physical Address Extensions"   },
  /* 7  */ { "mce"     ,  "Machine Check Exception"       },
  /* 8  */ { "cx8"     ,  "CMPXCHG8 instruction"          },
  /* 9  */ { "apic"    ,  "Onboard APIC"                  },
  /* 10 */ { "unk_10"  ,  ""                              },
  /* 11 */ { "sep"     ,  "SYSENTER/SYSEXIT"              },
  /* 12 */ { "mtrr"    ,  "Memory Type Range Registers"   },
  /* 13 */ { "pge"     ,  "Page Global Enable"            },
  /* 14 */ { "mca"     ,  "Machine Check Architecture"    },
  /* 15 */ { "cmov"    ,  "CMOV instructions"             },
  /* 16 */ { "pat"     ,  "Page Attribute Table"          },
  /* 17 */ { "pse36"   ,  "36-bit PSEs"                   },
  /* 18 */ { "pn"      ,  "Processor serial number"       },
  /* 19 */ { "clflush" ,  "CLFLUSH instruction"           },
  /* 20 */ { "unk_20"  ,  ""                              },
  /* 21 */ { "dts"     ,  "'ds' Debug Store"              },
  /* 22 */ { "acpi"    ,  "ACPI via MSR"                  },
  /* 23 */ { "mmx"     ,  "Multimedia Extensions"         },
  /* 24 */ { "fxsr"    ,  "FXSAVE/FXRSTOR, CR4.OSFXSR"    },
  /* 25 */ { "sse"     ,  "'xmm'"                         },
  /* 26 */ { "sse2"    ,  "'xmm2'"                        },
  /* 27 */ { "ss"      ,  "'selfsnoop' CPU self snoop"    },
  /* 28 */ { "ht"      ,  "Hyper-Threading"               },
  /* 29 */ { "tm"      ,  "'acc' Automatic clock control" },
  /* 30 */ { "ia64"    ,  "IA-64 processor"               },
  /* 31 */ { "pbe"     ,  "Pending Break Enable"          },
};

// It's not sufficient to look at the last 16 bytes of the /proc/$pid/auxv
// file, since they are all zero even for 32 bit processes.
// If we don't have read permissions for /proc/$pid/exe we likely
// don't have them for /proc/$pid/auxv, as well.
static int is_auxv_64_file(FILE *f, bool *result)
{
  unsigned char v[5];
  size_t r = fread(&v, 1, sizeof v, f);
  if (r != sizeof v) {
    if (ferror(f))
      perror(0);
    return -1;
  }
  static const unsigned char elf64_magic[] = { 0x7f, 0x45, 0x4c, 0x46, 0x02 };
  int x = memcmp(v, elf64_magic, sizeof v);
  *result = !x;
  return 0;
}

static int is_auxv_64(const char *pid, bool *result)
{
  char filename[64];
  sprintf(filename, "/proc/%s/exe", pid);
  FILE *f = fopen(filename, "rb");
  if (!f) {
    perror(0);
    return -1;
  }
  int r = is_auxv_64_file(f, result);
  if (r) {
    fclose(f);
    return r;
  }
  r = fclose(f);
  if (r) {
    perror(0);
    return -1;
  }
  return 0;
}

static void pp_hwcap(uint64_t val, FILE *o)
{
#if defined(__i386__) ||  defined(__i486__) ||  defined(__i586__) ||  \
  defined(__i686__) || defined(__x86_64__)

  for (size_t i = 0; i < sizeof x86_hwcap_map / sizeof x86_hwcap_map[0]; ++i) {
    if (val & ( ((uint64_t)1) << i)) {
      if (i)
        fputs(" |", o);
      fprintf(o, " %s", x86_hwcap_map[i].key);
    }
  }

#endif
}

static int read_mem_str(char **line, size_t *n, FILE *m, uint64_t off)
{
  int p = fseek(m, off, SEEK_SET);
  if (p == -1) {
    perror("fseek /proc/$pid/mem failed");
    return -1;
  }
  ssize_t r = getdelim(line, n, 0, m);
  if (r == -1) {
    perror(0);
    return -1;
  }
  return 0;
}

static int read_mem_rand(char **line, size_t *n, FILE *m, uint64_t off)
{
  int p = fseek(m, off, SEEK_SET);
  if (p == -1) {
    perror("fseek /proc/$pid/mem failed");
    return -1;
  }
  unsigned char v[16];
  if (*n < sizeof v * 3 + 1) {
    fprintf(stderr, "internal read mem error\n");
    return -1;
  }
  size_t r = fread(v, 1, sizeof v, m);
  if (r != sizeof v) {
    if (ferror(m))
      perror(0);
    return -1;
  }
  char *s = *line;
  for (size_t i = 0; i < sizeof v; ++i, s += 3)
    sprintf(s, "%.2x ", (unsigned) v[i]);
  s[-1] = 0;
  return 0;
}

static int pp_aux(uint64_t key, uint64_t val, FILE *o, const Args *args)
{
  switch (key) {
    case AT_HWCAP: pp_hwcap(val, o); break;
    case AT_PAGESZ: fprintf(o, " %" PRIu64 " KiB", (uint64_t)(val/1024)); break;
    case AT_CLKTCK: fprintf(o, " %" PRIu64 " Hz", val); break;
    case AT_UID:
    case AT_EUID:
    case AT_GID:
    case AT_EGID: fprintf(o, " %" PRIu64, val); break;
    case AT_SECURE: fprintf(o, " %s", val ? "true" : "false"); break;
    default: break;
  }
  return 0;
}

static int pp_aux_ref(uint64_t key, uint64_t val, char **line, size_t *n,
    FILE *m, FILE *o,
    const Args *args)
{
  switch (key) {
    case AT_BASE_PLATFORM:
    case AT_EXECFN:
    case AT_PLATFORM:
      if (read_mem_str(line, n, m, val))
        return -1;
      fprintf(o, " %s", *line);
      break;
    case AT_RANDOM:
      if (read_mem_rand(line, n, m, val))
        return -1;
      fprintf(o, " %s", *line);
    default: break;
  }
  return 0;
}

static int pp_aux_v(uint64_t key, uint64_t val, FILE *o, const Args *args)
{
  if (args->verbose && key < sizeof auxv_type_map / sizeof auxv_type_map[0]
      && *auxv_type_map[key].desc)
    fprintf(o, " (%s)", auxv_type_map[key].desc);
  return 0;
}

static int fput_proc_auxv_file(FILE *f, bool is_64, FILE *m, FILE *o,
    const Args *args)
{
  size_t n = 512;
  char *line = malloc(n);
  if (!line)
    return -1;
  *line = 0;
  for (size_t i = 0;; ++i) {
    uint64_t v[2] = {0};
    if (is_64) {
      size_t r = fread(&v, 1, sizeof v, f);
      if (r != sizeof v) {
        if (ferror(f))
          perror(0);
        free(line);
        return -1;
      }
    } else {
      uint32_t w[2] = {0};
      size_t r = fread(&w, 1, sizeof w, f);
      if (r != sizeof w) {
        if (ferror(f))
          perror(0);
        free(line);
        return -1;
      }
      v[0] = w[0];
      v[1] = w[1];
    }
    if (!v[0]) {
      free(line);
      return 0;
    }
    if (i)
      fputc('\n', o);
    if (v[0] < sizeof auxv_type_map / sizeof auxv_type_map[0])
      fprintf(o, "%-16s", auxv_type_map[v[0]].key);
    else
      fprintf(o, "unk_%" PRIu64, v[0]);
    fprintf(o, " 0x%.16" PRIx64, v[1]);

    int r = pp_aux(v[0], v[1], o, args);
    if (r) {
      free(line);
      return r;
    }
    r = pp_aux_ref(v[0], v[1], &line, &n, m, o, args);
    if (r) {
      free(line);
      return r;
    }
    r = pp_aux_v(v[0], v[1], o, args);
    if (r) {
      free(line);
      return r;
    }
  }
  free(line);
  return -1;
}

// https://unix.stackexchange.com/questions/6301/how-do-i-read-from-proc-pid-mem-under-linux
static FILE *fopen_mem(const char *pid, bool stop_process)
{
  // at least on Fedora 26, 4.14.8-200.fc26.x86_64
  // reading the auxv referenced memory also works while
  // the process is running ...
  // process stop/start is just necessary for Linux < 3.2:
  // https://unix.stackexchange.com/questions/6301/how-do-i-read-from-proc-pid-mem-under-linux#comment395763_6302
  if (stop_process) {
    pid_t p = atol(pid);
    // XXX for root, we just need to stop the process?
    long r = ptrace(PTRACE_ATTACH, p, 0, 0);
    if (r == -1) {
      perror(0);
      return 0;
    }
    siginfo_t info;
    r = waitid(P_PID, p, &info, WSTOPPED);
    if (r == -1) {
      perror(0);
      return 0;
    }
  }
  char filename[64];
  sprintf(filename, "/proc/%s/mem", pid);
  FILE *f = fopen(filename, "rb");
  if (!f) {
    perror(0);
  }
  return f;
}

static int fclose_mem(const char *pid, FILE *f, bool stop_process)
{
  int rc = 0;
  if (f) {
    int r = fclose(f);
    if (r) {
      perror(0);
      rc = -1;
    }
  }
  if (stop_process) {
    pid_t p = atol(pid);
    // XXX for root, we just need to start the process?
    long r = ptrace(PTRACE_DETACH, p, 0, 0);
    if (r == -1) {
      perror(0);
      return -1;
    }
  }
  return rc;
}

static int fput_proc_auxv(const char *pid, FILE *o, const Args *args)
{
  char filename[64];
  sprintf(filename, "/proc/%s/auxv", pid);
  FILE *f = fopen(filename, "rb");
  if (!f) {
    perror(0);
    return -1;
  }
  bool is_64;
  int r = is_auxv_64(pid, &is_64);
  if (r)
    return r;
  FILE *m = fopen_mem(pid, args->stop_process);
  if (!m) {
    fclose(f);
    return -1;
  }
  r = fput_proc_auxv_file(f, is_64, m, o, args);
  if (r) {
    fclose_mem(pid, m, args->stop_process); // RAII would simplify things ...
    fclose(f);
    return r;
  }
  r = fclose(f);
  if (r) {
    fclose_mem(pid, m, args->stop_process);
    return r;
  }
  r = fclose_mem(pid, m, args->stop_process);
  if (r)
    return r;
  return 0;
}

static int main_pid(const Args *args, const char *pid)
{
  int r;
  if (args->print_cmdline) {
    r = fput_proc_vector(pid, "cmdline", 0, " ", stdout);
    fputc('\n', stdout);
    if (r)
      return 1;
  } else {
    r = fput_header(pid, stdout);
    if (r)
      return 1;
    fputc('\n', stdout);
    if (args->print_argv) {
      r = fput_proc_vector(pid, "cmdline", "argv", "\n", stdout);
      if (r)
        return 1;
      fputc('\n', stdout);
    }
    if (args->print_envp) {
      if (args->print_argv)
        fputc('\n', stdout);
      r = fput_proc_vector(pid, "environ", "envp", "\n", stdout);
      if (r)
        return 1;
      fputc('\n', stdout);
    }
    if (args->print_auxv) {
      if (args->print_argv || args->print_envp)
        fputc('\n', stdout);
      r = fput_proc_auxv(pid, stdout, args);
      if (r)
        return 1;
      fputc('\n', stdout);
    }
  }
  return 0;
}

struct Range {
  const unsigned char *begin;
  const unsigned char *end;
};
typedef struct Range Range;

int mmap_core(const char *filename, Range *range)
{
  int fd = open(filename, O_RDONLY);
  if (fd == -1) {
    perror(0);
    return -1;
  }
  struct stat st;
  int r = fstat(fd, &st);
  unsigned char *p = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (p == (void*)-1) {
    perror(0);
    return -1;
  }
  range->begin = p;
  range->end = p + st.st_size;
  r = close(fd);
  if (r == -1) {
    perror(0);
    return -1;
  }
  return 0;
}

struct Landmarks {
  bool need_to_swap;
  uint32_t pid;
  const char *fname;
  const char *args;
  uint64_t execfn_addr; // substract vector_base_addr to get offset into vector_section
  uint8_t word_size;
  Range auxv_note; // read pairs of word_size until {0,0}
  Range vector_section;
  Range envp;
  Range argv;
  int argc;
  uint64_t vector_base_addr;
};
typedef struct Landmarks Landmarks;

static uint32_t align32_up_4(uint32_t i)
{
  i += 3;
  // binary literals are a GCC extensions in C11
  // they are available in C++14 and we want help to create a precedent
  i &= ~((uint32_t) 0b11);
  return i;
}

static const unsigned char *aligned_naive_memmemr(
    const unsigned char *haystack, size_t haystacklen,
    const unsigned char *needle, size_t needlelen, unsigned dec)
{
  if (needlelen > haystacklen)
    return 0;
  if (dec > needlelen)
    return 0;
  for (size_t i = haystacklen - needlelen + dec; i >= dec; i -= dec) {
    const unsigned char *p = haystack + (i - dec);
    if (!memcmp(p, needle, needlelen))
      return p;
  }
  return 0;
}

#define PARGS_ELF_CLASS 32
#include "pargs_tmpl.c"
#undef PARGS_ELF_CLASS
#define PARGS_ELF_CLASS 64
#include "pargs_tmpl.c"
#undef PARGS_ELF_CLASS

// a good reference and introduction into the ELF format:
// /usr/include/elf.h (Linux)
// elf(5) Linux man page
static int parse_landmarks(const Range *range, const char *filename,
    Landmarks *lm)
{
  const unsigned char *b = range->begin;
  const unsigned char *e = range->end;
  if (e - b < sizeof(Elf32_Ehdr)) {
    fprintf(stderr, "File %s even too small for ELF32 header.\n", filename);
    return -1;
  }
  static const unsigned char elf_magic[] = { 0x7f, 'E', 'L', 'F' };
  if (memcmp(b, elf_magic, sizeof elf_magic)) {
    fprintf(stderr, "Couldn't find ELF magic in %s.\n", filename);
    return -1;
  }
  debug("Reading core file: %s", filename);
  switch (b[EI_DATA]) {
    case ELFDATA2LSB:
      debug("Detected little endian byte order");
      lm->need_to_swap = __BYTE_ORDER == __BIG_ENDIAN;
      break;
    case ELFDATA2MSB:
      debug("Detected big endian byte order");
      lm->need_to_swap = __BYTE_ORDER == __LITTLE_ENDIAN;
      break;
    default:
      fprintf(stderr, "Unknown byte order in %s.\n", filename);
  }
  switch (b[EI_CLASS]) {
    case ELFCLASS32:
      debug("Reading 32 Bit ELF file");
      lm->word_size = 32;
      return parse_landmarks_32(range, filename, lm);
      break;
    case ELFCLASS64:
      debug("Reading 64 Bit ELF file");
      lm->word_size = 64;
      return parse_landmarks_64(range, filename, lm);
      break;
    default:
      fprintf(stderr, "Unknown ELF class in %s.\n", filename);
      return -1;
  }
  return 0;
}

static int fput_core_auxv(const Landmarks *lm, FILE *o, const Args *args)
{
  if (lm->word_size == 64)
    return fput_core_auxv_64(lm, o, args);
  else
    return fput_core_auxv_32(lm, o, args);
}

static int fput_core_vector(const Landmarks *lm, const Range *vec,
    const char *prefix, const char *delim, FILE *o)
{
  if (lm->word_size == 64)
    return fput_core_vector_64(lm, vec, prefix, delim, o);
  else
    return fput_core_vector_32(lm, vec, prefix, delim, o);
}

static int fput_core_header(const char *filename, const Landmarks *lm,
    FILE *o)
{
  fprintf(o, "core '%s' of %" PRIu32 ": ", filename, lm->pid);
  // on Fedora 26, lm->args, i.e. prpsinfo_t::pr_psargs just
  // contains argv[0] and a trailing space, thus, it's not sufficient
  // for our puposes
  return fput_core_vector(lm, &lm->argv, 0, " ", stdout);
}

static int main_core(const Args *args, const char *filename)
{
  Range range;
  int r = mmap_core(filename, &range);
  if (r)
    return r;
  Landmarks lm = {0};
  r = parse_landmarks(&range, filename, &lm);
  if (r)
    return r;

  if (args->print_cmdline) {
    r = fput_core_vector(&lm, &lm.argv, 0, " ", stdout);
    fputc('\n', stdout);
    if (r)
      return 1;
  } else {
    r = fput_core_header(filename, &lm, stdout);
    if (r)
      return 1;
    fputc('\n', stdout);
    if (args->print_argv) {
      r = fput_core_vector(&lm, &lm.argv, "argv", "\n", stdout);
      if (r)
        return 1;
      fputc('\n', stdout);
    }
    if (args->print_envp) {
      if (args->print_argv)
        fputc('\n', stdout);
      r = fput_core_vector(&lm, &lm.envp, "envp", "\n", stdout);
      if (r)
        return 1;
      fputc('\n', stdout);
    }
    if (args->print_auxv) {
      if (args->print_argv || args->print_envp)
        fputc('\n', stdout);
      r = fput_core_auxv(&lm, stdout, args);
      if (r)
        return 1;
      fputc('\n', stdout);
    }
  }

  r = munmap((void*)range.begin, range.end - range.begin);
  if (r == -1) {
    perror(0);
    return -1;
  }
  return 0;
}

int main(int argc, char **argv)
{
  Args args;
  int r = parse_args(argc, argv, &args);
  if (r)
    return r-1;
  size_t cnt = 0;
  bool more_opts = true;
  for (int i = 1; i < argc; ++i) {
    if (*argv[i] == '-' && argv[i][1] == '-') {
      more_opts = false;
    } else if (is_all_num(argv[i])) {
      if (cnt)
        fputc('\n', stdout);
      int r = main_pid(&args, argv[i]);
      if (r)
        return 1;
      ++cnt;
    } else if ((*argv[i] && *argv[i] != '-') || !more_opts) {
      int r = main_core(&args, argv[i]);
      if (r)
        return 1;
      ++cnt;
    }
  }
  if (!cnt) {
    print_help(stderr, *argv);
    return 2;
  }
  return 0;
}

