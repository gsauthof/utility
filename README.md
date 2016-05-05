This repository contains a collection of command line utilities.

- silence - silence stdout/stderr unless command fails
- silencce - C++ implementation of silence

For example:

    $ crontab -l
    15 10 * * * silence backup_stuff.sh /home


2016, Georg Sauthoff <mail@georg.so>

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


[gpl]: https://www.gnu.org/licenses/gpl.html
[moreutils]: the://joeyh.name/code/moreutils/
