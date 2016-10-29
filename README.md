This repository contains a collection of command line utilities.

- arsort       - topologically sort static libraries
- benchmark.sh - run a command multiple times and report stats
- benchmark.py - run a command multiple times and report stats (more features)
- latest-kernel-running - is the latest installed kernel actually running?
- lockf        - protect command execution with a lock
- pwhatch      - generate secure and easy to communicate passwords
- silence      - silence stdout/stderr unless command fails
- silencce     - C++ implementation of silence

For example:

    $ crontab -l
    15 10 * * * silence backup_stuff.sh /home


2016, Georg Sauthoff <mail@georg.so>

## Latest Kernel

This script checks if the system actually runs the latest
installed kernel. This might not be the case if something like
yum-cron automatically installs updates or if somebody forgot to
restart the system after a kernel update.

A mismatch in kernel versions is reported via the exit status.
With option `-v` a diagnostic message is printed, as well.

Thus, the script can be used to send out notification mails (e.g.
when running it as cron job).

Alternatively, the result  can be used to initiate a restart of a
machine that already runs yum-cron. If the machine provides a
clustered service, the restart can be coordinated with something
like etcd.

This check complements what [tracer][tracer] does.
Tracer checks if outdated applications or libraries are loaded
but (currently) doesn't check the kernel (cf. [Issue 45][tracer45]).


## Lockf

`lockf` only executes a provided command if a lock can be
aquired. Thus, it is able to serialize command execution and
guard access to exclusive actions. It provides several
locking methods:

- [`lockf()`][lockf] - hence the name
- [`fcntl()`][fcntl]
- [`flock()`][flock] - BSD API, also supported by e.g. Linux
- [`open(..., ... O_CREAT | O_EXCL)`][open] - exclusive file creation
- [`link()`][link] - hardlinking
- [`mkdir()`][mkdir] - not necessarily atomic everywhere
- [`rename()`][rename] - rename is also atomic

All of the methods are available on Linux. They also specified by
POSIX, except `flock()` which comes from BSD.

The methods `lockf()`, `fcntl()` and `flock()` support waiting on
a lock without polling (`-b` option).

Whether different lock methods do interact depends is system
specific. For example, on recent Linux, `fcntl()` and `flock()`
don't interact unless they are on NFS. And POSIX allows
interaction between `lockf()` and `fcntl()` but doesn't require
it.

Not all methods are necessarily supported and work reliable over
NFS. Especially in a heterogenous environment. Existing
implementation may chose to return success even if they implement
a locking API as null operation. Also, with some NFS
implementations some methods may become unreliable in case of
packet loss or a rebooting NFS server. For NFS, the methods worth
looking into are `lockf()`, `fcntl()`, `open()` and `link()`.
`mkdir()` is not specified by NFS to be atomic. Linux supports
`flock()` over NFS since kernel 2.6.37, but only because it is
emulated via `fcntl()` then.

Similar utilties:

- [Linux-Util flock][lu-flock], uses `flock()`
- [BSD lockf][bsd-lockf], uses `flock()` despite the name
- [lockrun][lockrun], uses `lockf()` where available, `flock()`
  otherwise
- [Procmail lockfile][lockfile], uses `link()` and supports polling

See also:

- [Correct locking in shell scripts?][1]

## Silence

`silence` is command wrapper that executes a command with its
arguments such that its stdout/stderr are written to unlinked
temporary files. In case the command exits with return code
unequal zero, the temporary files are streamed to stdout and
stderr of `silence`. Otherwise, the temporary files (under `TMPDIR`
or `/tmp`) vanish when both `silence` and the called command exit.

This is useful e.g. for job schedulers like cron, where
the output is only of interest in the event of failure. With
cron, the output of a program also triggers a notification mail
(another trigger is the return code).

`silence` provides the -k option for terminating the child in case
it is terminated before the child has exited. On Linux, this is
implemented via installing SIGTERM as parent death signal in its
child before it executes the supplied command. On other systems
the parent death signal mechanism  is approximated via installing
a signal handler for SIGTERM that kills the child.

The utility is a C reimplementation of [moreutils
chronic][moreutils] that is written in Perl. Thus, it has less
runtime overhead, especially less startup overhead.  The
unittests actually contain 2 test cases that fail for moreutils
chronic because of the startup overhead. Another difference is
that moreutils chronic buffers stdout and stderr lines in memory,
where `silence` writes them to temporary files, thus avoiding
memory issues with noisy long running commands. Also, moreutils
chronic doesn't provide means to get the child killed when it is
terminated. Other differences are documented in the unittest
cases (cf.  `test/chronic.py`).

## Silencce

`silencce` is a C++ implementation of `silence`. The main difference
is the usage of exceptions, thus simplifying the error reporting.

## Build Instructions

Get the source:

    $ git clone https://github.com/gsauthof/utility.git

Out of source builds are recommended, e.g.:

    $ mkdir utility-bin && cd utility-bin
    $ cmake ../utility
    $ make

Or to use ninja instead of make and create a release build:

    $ mkdir utility-bin-o && cd utility-bin-o
    $ cmake -G Ninja -D CMAKE_BUILD_TYPE=Release ../utility
    $ ninja-build

## Unittests

    $ make check

or

    $ ninja-build check

## License

[GPLv3+][gpl]

[1]: http://unix.stackexchange.com/questions/22044/correct-locking-in-shell-scripts
[bsd-lockf]: https://www.freebsd.org/cgi/man.cgi?query=lockf
[gpl]: https://www.gnu.org/licenses/gpl.html
[fcntl]: http://man7.org/linux/man-pages/man2/fcntl.2.html
[flock]: http://man7.org/linux/man-pages/man2/flock.2.html
[link]: http://man7.org/linux/man-pages/man2/link.2.html
[lockf]: http://man7.org/linux/man-pages/man3/lockf.3.html
[lockfile]: http://linux.die.net/man/1/lockfile
[lockrun]: http://www.unixwiz.net/tools/lockrun.html
[lu-flock]: http://linux.die.net/man/1/flock
[mkdir]: http://man7.org/linux/man-pages/man2/mkdir.2.html
[moreutils]: the://joeyh.name/code/moreutils/
[open]: http://man7.org/linux/man-pages/man2/open.2.html
[rename]: http://man7.org/linux/man-pages/man2/rename.2.html
[tracer]: http://tracer-package.com/
[tracer45]: https://github.com/FrostyX/tracer/issues/45

