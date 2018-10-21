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
"-h,--help   this screen\n"
"-k,-K       enable/disable suicide on parent exit (default: disabled)\n"
"            On Linux, a parent death signal is installed in the child\n"
"            that execs COMMAND, otherwise the TERM signal handler kills\n"
"            the child.\n"
"\n"
"It honors the TMPDIR environment and defaults to /tmp in case\n"
"it isn't set.\n"
"\n"
"This is a C++ reimplementation of chronic from moreutils\n"
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

#if defined(__linux__)
  #include <sys/prctl.h>
#endif

#include <ixxx/ixxx.hh>
#include <ixxx/util.hh>
#include <vector>
#include <algorithm>

#ifndef USE_PRCTL
  #if defined(__linux__)
    #define USE_PRCTL 1
  #else
    #define USE_PRCTL 0
  #endif
#endif

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

using namespace ixxx;
using namespace std;

static void help(FILE *f, const char *argv0)
{
  fprintf(f, help_str      , argv0);
}

struct Arguments {
  const char *tmpdir { "/tmp" };
  bool suicide { false};
  vector<int> success_codes;
};

static char **parse_arguments(int argc, char **argv, Arguments &a)
{
  const char *tmpdir = getenv("TMPDIR");
  if (tmpdir)
    a.tmpdir = tmpdir;
  if (argc > 1 && !strcmp(argv[1], "--help")) {
    help(stdout, argv[0]);
    exit(0);
  }
  char c = 0;
  const char opt_str[] = "+e:hkK";
  size_t success_codes_size = 0;
  while ((c = getopt(argc, argv, opt_str)) != -1) {
    switch (c) {
      case '?': help(stderr, argv[0]); exit(1); break;
      case 'h': help(stdout, argv[0]); exit(0); break;
      case 'e': ++success_codes_size;
      case 'k': a.suicide = true ; break;
      case 'K': a.suicide = false; break;
    }
  }
  if (optind == argc) {
    help(stderr, argv[0]);
    exit(1);
  }
  if (success_codes_size) {
    a.success_codes.reserve(success_codes_size);
    optind = 1;
    while ((c = getopt(argc, argv, opt_str)) != -1) {
      switch (c) {
        case 'e': a.success_codes.push_back(ansi::strtol(optarg, 0, 10)); break;
      }
    }
  }
  return argv + optind;
}


static int create_unlinked_temp_file(const char *tmpdir)
{
#if USE_TMPFILE
  int fd = posix::open(tmpdir, O_RDWR | O_TMPFILE | O_EXCL, 0600);
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
  int fd = posix:: mkstemp(s);
  umask(old_mask);
  posix::unlink(s);
  return fd;
#endif
}

static void dump(int fd, int d)
{
  posix::lseek(fd, 0, SEEK_SET);
  char buffer[128 * 1024];
  for (;;) {
    ssize_t n = ixxx::util::read_retry(fd, buffer, sizeof(buffer));
    if (!n)
      break;
    ixxx::util::write_all(d, buffer, n);
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

static bool is_successful(int code, const vector<int> &codes)
{
  if (count(codes.begin(), codes.end(), code))
    return true;
  return !code;
}

static void supervise_child(int fd_o, int fd_e, pid_t pid, const Arguments &a)
{
  // we ignore QUIT/INT because when issued via Ctrl+\/Ctrl+C in the terminal,
  // UNIX sends them both to the parent and the child
  // (cf. http://unix.stackexchange.com/questions/176235/fork-and-how-signals-are-delivered-to-processes)
  // ignoring them in the parent thus makes sure that any
  // collected output is printed after the child terminates
  // because of those signals (the default action)
  struct sigaction ignore_action = {};
  ignore_action.sa_handler = SIG_IGN;
  struct sigaction old_int_action;
  posix::sigaction(SIGINT, &ignore_action, &old_int_action);
  struct sigaction old_quit_action;
  posix::sigaction(SIGQUIT, &ignore_action, &old_quit_action);
#if !USE_PRCTL
  child_pid_ = pid;
  struct sigaction term_action = {};
  term_action.sa_handler = kill_child;
  struct sigaction old_term_action;
  if (a.suicide)
    posix::sigaction(SIGTERM, &term_action, &old_term_action);
#endif
  siginfo_t siginfo;
  posix:: waitid(P_PID, pid, &siginfo, WEXITED);
#if !USE_PRCTL
  if (a.suicide)
    posix::sigaction(SIGTERM, &old_term_action, 0);
#endif
  posix::sigaction(SIGINT, &old_int_action, 0);
  posix::sigaction(SIGQUIT, &old_quit_action, 0);
  int code = siginfo.si_code == CLD_EXITED
      ? siginfo.si_status : 128 + siginfo.si_status;
  if (is_successful(code, a.success_codes)) {
    exit(0);
  } else {
    dump(fd_o, 1);
    dump(fd_e, 2);
    exit(code);
  }
}

static void exec_child(int fd_o, int fd_e, char **argv)
{
  posix::dup2(fd_o, 1);
  posix::dup2(fd_e, 2);
  int r = execvp(*argv, argv);
  if (r == -1) {
    perror("executing command");
    // cf. http://tldp.org/LDP/abs/html/exitcodes.html
    exit(errno == ENOENT ? 127 : 126);
  }
}

int main(int argc, char **argv)
{
  try {
    Arguments a;
    char **childs_argv = parse_arguments(argc, argv, a);
    int fd_o = create_unlinked_temp_file(a.tmpdir);
    int fd_e = create_unlinked_temp_file(a.tmpdir);
#if USE_PRCTL
    pid_t ppid_before_fork = getpid();
#endif
    pid_t pid = posix::fork();
    if (pid) {
      supervise_child(fd_o, fd_e, pid, a);
    } else {
#if USE_PRCTL
    if (a.suicide) {
      linux::prctl(PR_SET_PDEATHSIG, SIGTERM);
      if (getppid() != ppid_before_fork)
        exit(1);
    }
#endif
      exec_child(fd_o, fd_e, childs_argv);
    }
  } catch (const ixxx::sys_error &e) {
    fprintf(stderr, "%s\n", e.what());
    return 1;
  }
  return 0;
}
