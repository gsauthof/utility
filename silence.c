static const char help_str[] =
"Call: %s [OPT..] COMMAND [ARG..]\n"
"\n"
"Silence stdout/stderr of COMMAND unless its return code is unequal to 0.\n"
"\n"
"Usecases:\n"
"\n"
" - wrap commands that are called from a job scheduler like cron\n"
" - increase the signal-to-noise-ratio in the terminal\n"
"\n"
"Options:\n"
"\n"
"-e N        interpret other return codes besides 0 as success\n"
"-h,--help   this screen\n"
"-k,-K       enable/disable suicide on parent exit (default: disabled)\n"
"            On Linux, a parent death signal is installed in the child\n"
"            that execs COMMAND, otherwise the TERM signal handler kills\n"
"            the child.\n"
"\n"
"It honors the TMPDIR environment and defaults to /tmp in case\n"
"it isn't set.\n"
"\n"
"This is a C reimplementation of chronic from moreutils\n"
"(which is a Perl script). See also the README.md for details\n"
"on the differences.\n"
"\n"
"\n"
"2016, Georg Sauthoff <mail@georg.so>, GPLv3+\n"
"cf. https://github.com/gsauthof/utility\n"
"\n";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>

#if defined(__linux__)
  #include <sys/prctl.h>
#endif

#include "utility.h"

#ifndef USE_TMPFILE
  #if defined(__linux__)
    #if defined(O_TMPFILE)
      #define USE_TMPFILE 1
    #else
      // not available e.g. on CentOS/RHEL 7
      #define USE_TMPFILE 0
      #warning "this Linux is so old it doesn't even have O_TMPFILE ..."
    #endif
  #else
    #define USE_TMPFILE 0
  #endif
#endif

#ifndef USE_PRCTL
  #if defined(__linux__)
    #define USE_PRCTL 1
  #else
    #define USE_PRCTL 0
  #endif
#endif

static void help(FILE *f, const char *argv0)
{
  fprintf(f, help_str      , argv0);
}

struct Arguments {
  const char *tmpdir;
  bool suicide;
  int *success_codes;
  size_t success_codes_size;
};
typedef struct Arguments Arguments;

static char **parse_arguments(int argc, char **argv, Arguments *a)
{
  *a = (Arguments) {
    .tmpdir = "/tmp",
    .suicide = false
  };
  const char *tmpdir = getenv("TMPDIR");
  if (tmpdir)
    a->tmpdir = tmpdir;
  if (argc > 1 && !strcmp(argv[1], "--help")) {
    help(stdout, argv[0]);
    exit(0);
  }
  char c = 0;
  const char opt_str[] = "+e:hkK";
  while ((c = getopt(argc, argv, opt_str)) != -1) {
    switch (c) {
      case '?': help(stderr, argv[0]); exit(1); break;
      case 'h': help(stdout, argv[0]); exit(0); break;
      case 'e': ++a->success_codes_size; break;
      case 'k': a->suicide = true ; break;
      case 'K': a->suicide = false; break;
    }
  }
  if (optind == argc) {
    help(stderr, argv[0]);
    exit(1);
  }
  if (a->success_codes_size) {
    a->success_codes = calloc(a->success_codes_size, sizeof(int));
    if (!a->success_codes)
      exit(1);
    int *v = a->success_codes;
    optind = 1;
    while ((c = getopt(argc, argv, opt_str)) != -1) {
      switch (c) {
        case 'e':
          {
            errno = 0;
            char *s = 0;
            *v++ = strtol(optarg, &s, 10);
            if (errno || s == optarg) {
              perror("converting -e argument");
              exit(1);
            }
          }
          break;
      }
    }
  }
  return argv + optind;
}

static int create_unlinked_temp_file(const char *tmpdir)
{
#if USE_TMPFILE
  int fd = open(tmpdir, O_RDWR | O_TMPFILE | O_EXCL, 0600);
  check_exit(fd, "opening temp file");
  return fd;
#else
  // POSIX.1-2001 doesn't specify the mode of mkstemp(),
  // POSIX.1-2008 does specify 0600
  mode_t old_mask = umask(0177);
  const char suffix[] = "/silence_XXXXXX";
  size_t n = strlen(tmpdir);
  char s[n+sizeof(suffix)];
  memcpy(s, tmpdir, n);
  memcpy(s+n, suffix, sizeof(suffix));
  int fd = mkstemp(s);
  check_exit(fd, "creating temp file");
  umask(old_mask);
  int r = unlink(s);
  check_exit(r, "unlinking temp file");
  return fd;
#endif
}

static ssize_t write_auto_resume(int fd, const void *buffer, size_t n)
{
  ssize_t m = 0;
  do {
    m = write(fd, buffer, n);
  } while (m == -1 && errno == EINTR);
  return m;
}
static ssize_t read_auto_resume(int fd, void *buffer, size_t n)
{
  ssize_t m = 0;
  do {
    m = read(fd, buffer, n);
  } while (m == -1 && errno == EINTR);
  return m;
}
static ssize_t write_all(int fd, const char *buffer, size_t n_)
{
  ssize_t n = n_;
  do {
    ssize_t m = write_auto_resume(fd, buffer, n);
    if (m == -1)
      return -1;
    buffer += m;
    n -= m;
  } while (n);
  return n_;
}

static void dump(int fd, int d)
{
  int r = lseek(fd, 0, SEEK_SET);
  check_exit(r, "seeking output file");
  char buffer[128 * 1024];
  for (;;) {
    ssize_t n = read_auto_resume(fd, buffer, sizeof(buffer));
    check_exit(n, "reading output file");
    if (!n)
      break;
    ssize_t m = write_all(d, buffer, n);
    check_exit(m, "writing output");
  }
}

#if !USE_PRCTL

static pid_t child_pid_ = 0;

static void kill_child(int sig)
{
  if (child_pid_)
    kill(child_pid_, SIGTERM);
  exit(128 + SIGTERM);
}

#endif

static bool is_successful(int code, const Arguments *a)
{
  for (size_t i = 0; i < a->success_codes_size; ++i)
    if (code == a->success_codes[i])
      return true;
  return !code;
}

static void supervise_child(int fd_o, int fd_e, pid_t pid, const Arguments *a)
{
  // we ignore QUIT/INT because when issued via Ctrl+\/Ctrl+C in the terminal,
  // UNIX sends them both to the parent and the child
  // (cf. http://unix.stackexchange.com/questions/176235/fork-and-how-signals-are-delivered-to-processes)
  // ignoring them in the parent thus makes sure that any
  // collected output is printed after the child terminates
  // because of those signals (the default action)
  struct sigaction ignore_action = { .sa_handler = SIG_IGN };
  struct sigaction old_int_action;
  int r = sigaction(SIGINT, &ignore_action, &old_int_action);
  check_exit(r, "ignoring SIGINT");
  struct sigaction old_quit_action;
  r = sigaction(SIGQUIT, &ignore_action, &old_quit_action);
  check_exit(r, "ignoring SIGQUIT");
#if !USE_PRCTL
  child_pid_ = pid;
  struct sigaction term_action = { .sa_handler = kill_child };
  struct sigaction old_term_action;
  if (a->suicide) {
    r = sigaction(SIGTERM, &term_action, &old_term_action);
    check_exit(r, "installing SIGTERM handler");
  }
#endif
  int status = 0;
  r = waitpid(pid, &status, 0);
  check_exit(r, "waiting on child");
#if !USE_PRCTL
  if (a->suicide) {
    r = sigaction(SIGTERM, &old_term_action, 0);
    check_exit(r, "restoring SIGTERM");
  }
#endif
  r = sigaction(SIGINT, &old_int_action, 0);
  check_exit(r, "restoring SIGINT");
  r = sigaction(SIGQUIT, &old_quit_action, 0);
  check_exit(r, "restoring SIGQUIT");
  int code = WIFEXITED(status) ? WEXITSTATUS(status)
    : (WIFSIGNALED(status) ?  128 + WTERMSIG(status) : 1);
  if (is_successful(code, a)) {
    exit(0);
  } else {
    dump(fd_o, 1);
    dump(fd_e, 2);
    exit(code);
  }
}

static void exec_child(int fd_o, int fd_e, char **argv)
{
  int r = dup2(fd_o, 1);
  check_exit(r, "redirecting stdout");
  r = dup2(fd_e, 2);
  check_exit(r, "redirecting stderr");
  r = execvp(*argv, argv);
  if (r == -1) {
    perror("executing command");
    // cf. http://tldp.org/LDP/abs/html/exitcodes.html
    exit(errno == ENOENT ? 127 : 126);
  }
}


/*

## Properties

    exit_code(x) == exit_code(silence x) # where x terminates due to an signal
    exit_code(x) == exit_code(silence x)
    silence x ; x is long running; kill silence; => x is still running
    silence x ; x is long running; kill x ;
        => silence exits with exit_code == 128+SIGTERM
    silence x ; in terminal ; Ctrl+C ;
        => Ctrl+C  (SIGINT) is both sent to silence and x;
           silence ignores it while waiting
    silence x ; in terminal ; Ctrl+\ ;
        => Ctrl+\  (SIGQUIT) is both sent to silence and x;
           silence ignores it while waiting
    silence x ; x not found ; => exit_code == 127
    silence x ; exec fails ; => exit_code = 128
    silence ; => help is display, exit_code = 1
    silence ... ; any other syscall fails ; => exit_code = 1
    silence ... ; read/write syscall is interrupted ;
        => it is called again (i.e. resumed)
    silence ... ; write does not write the complete buffer ;
        => it is called again with the remaining buffer

   */
int main(int argc, char **argv)
{
  Arguments a = {0};
  char **childs_argv = parse_arguments(argc, argv, &a);
  int fd_o = create_unlinked_temp_file(a.tmpdir);
  int fd_e = create_unlinked_temp_file(a.tmpdir);
#if USE_PRCTL
  pid_t ppid_before_fork = getpid();
#endif
  pid_t pid = fork();
  check_exit(pid, "forking child");
  if (pid) {
    supervise_child(fd_o, fd_e, pid, &a);
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
    exec_child(fd_o, fd_e, childs_argv);
  }
  return 0;
}
