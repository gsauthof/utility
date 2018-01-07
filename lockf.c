static const char help_str[] =
"Call: %s -d FD [OPT..] COMMAND [ARG..]\n"
"         -f LOCKFILE [OPT..] COMMAND [ARG..]\n"
"         ...\n"
"\n"
"Serialize execution of COMMAND using POSIX lockf() locking\n"
"(or other methods).\n"
"\n"
"Usecases:\n"
"\n"
" - Make sure that only one instance of a cron job is running\n"
" - Coordinate command executions via NFS\n"
"\n"
"Options:\n"
"\n"
"-b           let lockf()/fcntl()/flock block and wait on a locked file\n"
"-c LOCKFILE  open LOCKFILE with O_CREAT before calling lockf()\n"
"-d FD        lock the already open file descriptor FD\n"
"-e LOCKFILE  open LOCKFILE with O_CREAT and O_EXCL for locking\n"
"-f LOCKFILE  open LOCKFILE for locking (O_WRONLY-only) with lockf()\n"
"-h,--help    this screen\n"
"-i LOCKFILE  use link() for locking (cf. -s)\n"
"-k,-K        enable/disable suicide on parent exit (default: disabled)\n"
"             On Linux, a parent death signal is installed in the child\n"
"             that execs COMMAND, otherwise the TERM signal handler kills\n"
"             the child.\n"
"-l           use flock() instead of lockf()\n"
"-n           use fcntl instead of lockf()\n"
"-m LOCKDIR   use mkdir for locking\n"
"-r LOCKFILE  use rename() for locking; LOCKFILE is moved to SOURCE (cf. -s);\n"
"             -u moves the file back\n"
"-s SOURCE    use SOURCE for hardlinking source/rename destination\n"
"             (if not specified one is created via mkstemp())\n"
"-u           unlink LOCKFILE on exit\n"
"\n"
"With lockf()/fcntl()/flock(), the lock is automatically removed\n"
"when the program terminates.\n"
"\n"
"Not all methods are necessarily reliable over NFS. Especially in\n"
"heterogenous environments. See the README.md for details. lockf()\n"
"and open(... O_CREAT|O_EXCL) should be a relative good bet, though.\n"
"\n"
"2016, Georg Sauthoff <mail@georg.so>, GPLv3+\n"
"cf. https://github.com/gsauthof/utility\n"
"\n";

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <fcntl.h>

#if defined(__linux__)
  #include <sys/prctl.h>
#endif

#include "utility.h"

#ifndef USE_PRCTL
  #if defined(__linux__)
    #define USE_PRCTL 1
  #else
    #define USE_PRCTL 0
  #endif
#endif
#define USE_KILL 1

static void help(FILE *f, const char *argv0)
{
  fprintf(f, help_str      , argv0);
}

enum Method {
  LOCKF,
  MKDIR,
  OPEN,
  FLOCK,
  FCNTL,
  LINK,
  RENAME
};
typedef enum Method Method;

struct Arguments {
  Method method;
  bool unlink;
  bool unlink_source;
  bool block;
  bool suicide;
  int fd;
  const char *filename;
  const char *source;
  char **childs_argv;
};
typedef struct Arguments Arguments;

#define verify_exclusive(x, msg) do { if (x) { fprintf(stderr, (msg)); exit(1); } ++(x); } while(0)

static void post_process_arguments(Arguments *a)
{
  if ((a->method == LINK || a->method == RENAME) && !a->source) {
    const char suffix[] = "_XXXXXX";
    size_t n = strlen(a->filename);
    char *s = calloc(n+sizeof(suffix), 1);
    if (!s)
      exit(1);
    memcpy(s, a->filename, n);
    memcpy(s + n, suffix, sizeof(suffix));
    int fd = mkstemp(s);
    check_exit(fd, "creating temp file");
    int r = close(fd);
    check_exit(r, "closing temp file");
    a->unlink_source = true;
    a->source = s;
  }
}

static void parse_arguments(int argc, char **argv, Arguments *a)
{
  *a = (Arguments) {0};
  char c = 0;
  unsigned x = 0;
  const char excl_str[] = "only one of -c/-d/-e/-f/-m/-r/-s allowed\n";
  const char opt_str[] = "+bc:d:e:f:hi:lm:nr:s:u";
  while ((c = getopt(argc, argv, opt_str)) != -1) {
    switch (c) {
      case '?': help(stderr, argv[0]); exit(1); break;
      case 'b':
        a->block = true;
        break;
      case 'c':
        verify_exclusive(x, excl_str);
        a->filename = optarg;
        a->fd = open(a->filename, O_CREAT | O_WRONLY, 0666);
        check_exit(a->fd, "opening lockfile");
        break;
      case 'd':
        verify_exclusive(x, excl_str);
        {
          errno = 0;
          char *s =0;
          a->fd = strtol(optarg, &s, 10);
          if (errno || s == optarg) {
            perror("converting -d argument");
            exit(1);
          }
          if (!a->fd) {
            fprintf(stderr, "Cannot lock file descriptor 0\n");
            exit(1);
          }
        }
        break;
      case 'e':
        verify_exclusive(x, excl_str);
        a->filename = optarg;
        a->method = OPEN;
        break;
      case 'f':
        verify_exclusive(x, excl_str);
        a->filename = optarg;
        a->fd = open(a->filename, O_WRONLY);
        check_exit(a->fd, "opening lockfile");
        break;
      case 'h': help(stdout, argv[0]); exit(0); break;
      case 'i':
        verify_exclusive(x, excl_str);
        a->method = LINK;
        a->filename = optarg;
        break;
      case 'k': a->suicide = true ; break;
      case 'K': a->suicide = false; break;
      case 'l': a->method = FLOCK; break;
      case 'm':
        verify_exclusive(x, excl_str);
        a->method = MKDIR;
        a->filename = optarg;
        break;
      case 'n': a->method = FCNTL; break;
      case 'r':
        verify_exclusive(x, excl_str);
        a->method = RENAME;
        a->filename = optarg;
        break;
      case 's': a->source = optarg; break;
      case 'u': a->unlink = true; break;
    }
  }
  if (optind == argc || !x) {
    help(stderr, argv[0]);
    exit(1);
  }
  a->childs_argv = argv + optind;
  post_process_arguments(a);
}

#if USE_KILL

static pid_t child_pid_ = 0;

static void kill_child(int sig)
{
  if (child_pid_)
    kill(child_pid_, SIGTERM);
  // we don't exit because we want to still wait on the
  // child to release the lock after it has finished
}

#endif

static void supervise_child(pid_t pid, const Arguments *a)
{
  // we ignore QUIT/INT because when issued via Ctrl+\/Ctrl+C in the terminal,
  // UNIX sends them both to the parent and the child
  // (cf. http://unix.stackexchange.com/questions/176235/fork-and-how-signals-are-delivered-to-processes)
  // we ignore those such that we reliably terminate after the
  // child (assuming default action there), thus, lock is
  // released after the child's exit
  struct sigaction ignore_action = { .sa_handler = SIG_IGN };
  struct sigaction old_int_action;
  int r = sigaction(SIGINT, &ignore_action, &old_int_action);
  check_exit(r, "ignoring SIGINT");
  struct sigaction old_quit_action;
  r = sigaction(SIGQUIT, &ignore_action, &old_quit_action);
  check_exit(r, "ignoring SIGQUIT");
#if USE_KILL
  child_pid_ = pid;
  struct sigaction term_action = { .sa_handler = kill_child };
  struct sigaction old_term_action;
  if (a->suicide) {
    int r = sigaction(SIGTERM, &term_action, &old_term_action);
    check_exit(r, "installing SIGTERM handler");
  }
#endif
  siginfo_t siginfo;
  r = waitid(P_PID, pid, &siginfo, WEXITED);
  check_exit(r, "waiting on child");
#if USE_KILL
  if (a->suicide) {
    r = sigaction(SIGTERM, &old_term_action, 0);
    check_exit(r, "restoring SIGTERM");
  }
#endif
  r = sigaction(SIGINT, &old_int_action, 0);
  check_exit(r, "restoring SIGINT");
  r = sigaction(SIGQUIT, &old_quit_action, 0);
  check_exit(r, "restoring SIGQUIT");
  if (a->unlink) {
    if (a->method == RENAME) {
      int r = rename(a->source, a->filename);
      check_exit(r, "move lockfile back");
    } else {
      int r = unlink(a->filename);
      check_exit(r, "unlinking lockfile");
    }
  }
  int code = siginfo.si_code == CLD_EXITED
      ? siginfo.si_status : 128 + siginfo.si_status;
  exit(code);
}

static void exec_child(char **argv)
{
  int r = execvp(*argv, argv);
  if (r == -1) {
    perror("executing command");
    // cf. http://tldp.org/LDP/abs/html/exitcodes.html
    exit(errno == ENOENT ? 127 : 126);
  }
}

static void aquire_lock(const Arguments *a)
{
  int r = 0;
  switch (a->method) {
    case LOCKF:
      r = lockf(a->fd, a->block ? F_LOCK : F_TLOCK, 0);
      check_exit(r, "lockf locking");
      break;
    case FLOCK:
      r = flock(a->fd, LOCK_EX | (a->block ? 0 : LOCK_NB ));
      check_exit(r, "flock locking");
      break;
    case FCNTL:
      {
        struct flock l = { .l_type = F_WRLCK,
          .l_start = 0,
          .l_whence = SEEK_SET,
          .l_len = 0  };
        r = fcntl(a->fd, a->block ? F_SETLKW : F_SETLK, &l);
      }
      check_exit(r, "fcntl locking");
      break;
    case OPEN:
      r = open(a->filename, O_CREAT | O_EXCL | O_RDONLY, 0666);
      check_exit(r, "excl open locking");
      break;
    case MKDIR:
      r = mkdir(a->filename, 0777);
      check_exit(r, "mkdir locking");
      break;
    case LINK:
      r = link(a->source, a->filename);
      check_exit(r, "link locking");
      if (a->unlink_source) {
        r = unlink(a->source);
        check_exit(r, "unlinking source");
      }
      break;
    case RENAME:
      r = rename(a->filename, a->source);
      check_exit(r, "rename locking");
      break;
    default:
      break;
  }
}

int main(int argc, char **argv)
{
  Arguments a = {0};
  parse_arguments(argc, argv, &a);
  aquire_lock(&a);
#if USE_PRCTL
  pid_t ppid_before_fork = getpid();
#endif
  pid_t pid = fork();
  check_exit(pid, "forking child");
  if (pid) {
    // we don't need stdin
    int r = close(0);
    check_exit(r, "closing stdin in parent");
    supervise_child(pid, &a);
  } else {
#if USE_PRCTL
    if (a.suicide) {
      // is valid through the exec, but is not inherited into children
      int r = prctl(PR_SET_PDEATHSIG, SIGTERM);
      check_exit(r, "installing parent death signal");
      // cf. http://stackoverflow.com/a/36945270/427158
      if (getppid() != ppid_before_fork)
        exit(1);
    }
#endif
    if (a.fd) {
      int r = close(a.fd);
      check_exit(r, "closing fd before exec");
    }
    exec_child(a.childs_argv);
  }
  return 0;
}
