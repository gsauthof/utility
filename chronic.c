static const char help_str[] =
"Call: %s COMMAND [ARG..]\n"
"\n"
"Silence stdout/stderr of COMMAND unless its return code is unequal to 0.\n"
"\n"
"Usecases:\n"
"\n"
" - wrap commands that are called from a job scheduler like cron\n"
" - increase the signal-to-noise-ratio in the terminal\n"
"\n"
"The program doesn't have any options besides -h/--help.\n"
"\n"
"It honors the TMPDIR environment and defaults to /tmp in case\n"
"it isn't set.\n"
"\n"
"This is a C reimplementation of chronic from moreutils\n"
"(which is a Perl script). See also the README.md for details\n"
"on the differences.\n"
"\n"
"\n"
"2016, Georg Sauthoff <mail@georg.so>\n"
"GPLv3+\n"
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


#define check_exit(r, s) do { if (r == -1) { perror(s); exit(1); } } while (0)

static void help(FILE *f, const char *argv0)
{
  fprintf(f, help_str      , argv0);
}

static int create_unlinked_temp_file(const char *tmpdir)
{
#if defined(__linux__)
  int fd = open(tmpdir, O_RDWR | O_TMPFILE | O_EXCL, 0600);
  check_exit(fd, "opening temp file");
  return fd;
#else
  // POSIX.1-2001 doesn't specify the mode of mkstemp(),
  // POSIX.1-2008 does specify 0600
  mode_t old_mask = umask(0177);
  const char suffix[] = "/chronic_XXXXXX";
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
static ssize_t write_all(int fd, const void *buffer, size_t n_)
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

static void supervise_child(int fd_o, int fd_e, pid_t pid)
{
  // we ignore QUIT/INT because when issued via Ctrl+\/Ctrl+C in the terminal,
  // UNIX sends them both to the parent and the child
  // (cf. http://unix.stackexchange.com/questions/176235/fork-and-how-signals-are-delivered-to-processes)
  // ignoring them in the parent thus makes sure that any collected output is printed
  // after the child terminates because of those signals (the default action)
  struct sigaction ignore_action = { .sa_handler = SIG_IGN };
  struct sigaction old_int_action;
  int r = sigaction(SIGINT, &ignore_action, &old_int_action);
  check_exit(r, "ignoring SIGINT");
  struct sigaction old_quit_action;
  r = sigaction(SIGQUIT, &ignore_action, &old_quit_action);
  check_exit(r, "ignoring SIGQUIT");
  int status = 0;
  r = waitpid(pid, &status, 0);
  check_exit(r, "waiting on child");
  r = sigaction(SIGINT, &old_int_action, 0);
  check_exit(r, "restoring SIGINT");
  r = sigaction(SIGQUIT, &old_quit_action, 0);
  check_exit(r, "restoring SIGQUIT");
  int code = WIFEXITED(status) ? WEXITSTATUS(status)
    : (WIFSIGNALED(status) ?  128 + WTERMSIG(status) : 1);
  if (code) {
    dump(fd_o, 1);
    dump(fd_e, 2);
  }
  exit(code);
}

static void exec_child(int fd_o, int fd_e, char **argv)
{
  int r = dup2(fd_o, 1);
  check_exit(r, "redirecting stdout");
  r = dup2(fd_e, 2);
  check_exit(r, "redirecting stderr");
  r = execvp(argv[1], argv+1);
  if (r == -1) {
    perror("executing command");
    // cf. http://tldp.org/LDP/abs/html/exitcodes.html
    exit(errno == ENOENT ? 127 : 126);
  }
}

/*

## Properties

    exit_code(x) == exit_code(chronic x) # where x terminates due to an signal
    exit_code(x) == exit_code(chronic x)
    chronic x ; x is long running; kill chronic; => x is still running
    chronic x ; x is long running; kill x ; => chronic exits with exit_code == 128+SIGTERM
    chronic x ; in terminal ; Ctrl+C ; => Ctrl+C  (SIGINT) is both sent to chronic and x; chronic ignores it while waiting
    chronic x ; in terminal ; Ctrl+\ ; => Ctrl+\  (SIGQUIT) is both sent to chronic and x; chrnoic ignores it while waiting
    chronic x ; x not found ; => exit_code == 127
    chronic x ; exec fails ; => exit_code = 128
    chronic ; => help is display, exit_code = 1
    chronic ... ; any other syscall fails ; => exit_code = 1
    chronic ... ; read/write syscall is interrupted ; => it is called again (i.e. resumed)
    chronic ... ; write does not write the complete buffer ; => it is called again with the remaining buffer

   */
int main(int argc, char **argv)
{
  if (argc == 1) {
    help(stderr, argv[0]);
    return 1;
  }
  if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
    help(stdout, argv[0]);
    return 0;
  }
  const char *tmpdir = getenv("TMPDIR");
  if (!tmpdir)
    tmpdir = "/tmp";
  int fd_o = create_unlinked_temp_file(tmpdir);
  int fd_e = create_unlinked_temp_file(tmpdir);
  pid_t pid = fork();
  check_exit(pid, "forking child");
  if (pid)
    supervise_child(fd_o, fd_e, pid);
  else
    exec_child(fd_o, fd_e, argv);
  return 0;
}
