[![Build Status](https://travis-ci.org/gsauthof/utility.svg?branch=master)](https://travis-ci.org/gsauthof/utility)

This repository contains a collection of command line utilities.

- arsort       - topologically sort static libraries
- ascsii.py    - pretty print the ASCII table
- benchmark.sh - run a command multiple times and report stats
- benchmark.py - run a command multiple times and report stats (more features)
- check-cert   - check approaching expiration/validate certs of remote servers
- check-dnsbl  - check if mailservers are DNS blacklisted/have rDNS records
- check2junit  - convert libcheck XML to Jenkins/JUnit compatible XML
- chromium-addons - list installed Chromium extensions
- firefox-addons  - list installed Firefox addons
- gs-ext       - list and manage installed Gnome Shell Extensions
- latest-kernel-running - is the latest installed kernel actually running?
- lockf        - protect command execution with a lock
- lsata.sh     - map ataX kernel log ids to /dev/sdY devices
- macgen       - randomly generate a private/internal MAC address
- macgen.py    - Python implementation of macgen
- pwhatch      - generate secure and easy to communicate passwords
- silence      - silence stdout/stderr unless command fails
- silencce     - C++ implementation of silence
- swap         - atomically exchange names of two files on Linux
- train-spam   - feed spam maildir messages into bogofilter and remove them
- user-installed.py - list manually installed packages on major distributions

For example:

    $ crontab -l
    15 10 * * * silence backup_stuff.sh /home


2016, Georg Sauthoff <mail@georg.so>

## ASCII

This utility pretty-prints the [ASCII][ascii] table. By default, the table
has 4 columns. With 4 columns the table is still compact and some
properties are easy to spot. For example how upper and lower case
conversion is just a matter of toggling one bit. Or the
relationship between control characters and pressing Ctrl and a
printable character on the same row (think: ESC vs. `Ctrl+[`,
`TAB` vs. `Ctrl+I`, `CR` vs. `Ctrl+M` etc.)

The number of columns can be changed with the `-c` option. For
example `-c8` yields a 8 column table, which is how the ASCII
table is usually printed in (old) manuals. Such a table
highlights other properties. For example how ASCII somewhat
simplifies the conversion of [Binary Coded Decimals (BCD)][bcd]
(think: bitwise-or the BCD nibble with `0b011` to get the ASCI
decimal character  and bitwise-and with `0b1111` for the other
direction).

The `-x` option is useful for looking up lesser used control
character abbreviations, e.g.:

    $ /ascii.py -x EOT
    EOT = End of Transmission (4)

Example 32x4 table:

       00   01   10   11
      NUL  SPC    @    `  00000
      SOH    !    A    a  00001
      STX    "    B    b  00010
      ETX    #    C    c  00011
      EOT    $    D    d  00100
      ENQ    %    E    e  00101
      ACK    &    F    f  00110
      BEL    '    G    g  00111
       BS    (    H    h  01000
       HT    )    I    i  01001
       LF    *    J    j  01010
       VT    +    K    k  01011
       FF    ,    L    l  01100
       CR    -    M    m  01101
       SO    .    N    n  01110
       SI    /    O    o  01111
      DLE    0    P    p  10000
      DC1    1    Q    q  10001
      DC2    2    R    r  10010
      DC3    3    S    s  10011
      DC4    4    T    t  10100
      NAK    5    U    u  10101
      SYN    6    V    v  10110
      ETB    7    W    w  10111
      CAN    8    X    x  11000
       EM    9    Y    y  11001
       SS    :    Z    z  11010
      ESC    ;    [    {  11011
       FS    <    \    |  11100
       GS    =    ]    }  11101
       RS    >    ^    ~  11110
       US    ?    _  DEL  11111

Placing the column headers on top and the row headers at the
right makes it clear how the resulting code is constructed for a
character, i.e. the column header is the prefix and the row
header is the suffix. Example: the R character has the binary
value `0b1010010`.

## Check-Cert

This script calls [`gnutls-cli`][gnutls] for the specified remote
services.

Example:

    $ check-cert.py imap.example.org_993 example.org_443

Any validation errors (including OCSP ones) reported by
[GnuTLS][gnutls] are printed by the script, which finally returns
with exit status unequal zero. The script also warns and exits
unsuccessfully if a cert expires in less than 20 days.

If everything is fine the script is silent and exits
successfully, thus, check-cert is suitable for a Cron scheduled
execution.

Checking a service that does TLS after a STARTTLS command like in

   $ check-cert.py mail.example.org_25_smtp

requires GnuTLS version 3.4.x or later (e.g. 3.4.15). For example,
RHEL/CentOS 7 comes with GnuTLS 3.3.8, while Fedora 23 provides
gnutls 3.4.15.

It may make sense to create a `gnutls-cli` wrapper script and put
it into `$PATH` such that the right version is called with the
right CA bundle, e.g.:

    #!/bin/bash
    exec /nix/var/nix/profiles/default/bin/gnutls-cli \
      --x509cafile=/etc/pki/tls/cert.pem "$@"


The script doesn't use the comparable Openssl command (i.e. `openssl
s_client`) because it doesn't conveniently present the
expiration dates and it doesn't even exit with a status unequal
zero in case of verification errors.

## Check-DNSBL

This utility checks a list of mail servers against some [well
known blacklists
(DNSBL)](https://en.wikipedia.org/wiki/Comparison_of_DNS_blacklists)
By default, 30 or so lists are queried, but other/additional ones
can be specified via command line arguments or a CSV file.

It also checks, by default, if there are [reverse
DNS](https://en.wikipedia.org/wiki/Reverse_DNS_lookup) records
and if they match the forward ones.

The mail server can be specified as a list of IPv4 or IPv6
addresses and/or domain names. MX records are followed, by
default. Of course, an outgoing mail server doesn't necessarily
have to double as MX - in those cases its domain/address has to
be additionally specified.  In any case, if domains are
specified, at last, they are resolved via their A or AAAA records
to IPv4 or IPv6 addresses.

If everything is ok, `check-dnsbl.py` doesn't generate any output
(unless `--debug` is specified). Otherwise, it prints errors to
stderr and exits with a status unequal zero. Thus, it can be
used as Cron job for monitoring purposes.

Examples:

Something is listed:

    $ ./check-dnsbl.py 117.246.201.146
    2016-11-05 19:01:13 - ERROR    - There is no reverse DNS record for 117.246.201.146
    2016-11-05 19:01:13 - ERROR    - OMG, 117.246.201.146 is listed in DNSBL zen.spamhaus.org: 127.0.0.11 ("https://www.spamhaus.org/query/ip/117.246.201.146")
    2016-11-05 19:01:19 - ERROR    - OMG, 117.246.201.146 is listed in DNSBL virbl.dnsbl.bit.nl: 127.0.0.2 ("See: http://virbl.bit.nl/lookup/index.php?ip=117.246.201.146")
    2016-11-05 19:01:19 - ERROR    - 117.246.201.146 is listed in 2 blacklists

Everything is ok:

    $ ./check-dnstbl.py mail1.example.org mail2.example.org example.net
    $ echo $?
    0


## Check2junit

[Jenkins][jenkins] comes with the [JUnit plugin][junit] that
draws some nice graphs and creates reports from XML generated by
JUnit testsuite runs. For example, there is a graph with the
number of successful/failed testcases over the builds and there
are graphs that display the duration of single test cases over
the builds.

The [libcheck][check] unittest C library also supports the
generation of an XML report.

This scripts converts the libcheck XML format into the XML format
supported by the Jenkins JUnit plugin.

Example (e.g. part of a build script):

    CK_XML_LOG_FILE_NAME=test1.xml ./check_network
    CK_XML_LOG_FILE_NAME=test2.xml ./check_backend
    check2junit.py test1.xml test2.xml > junit.xml

The JUnit plugin can be configured in the Jenkins job
configuration, basically it has to be added as another post-build
action (where the input filename can be set, e.g.
to `junit.xml`).

### Related

- The C++ unittest library [Catch](https://github.com/philsquared/Catch/blob/master/docs/build-systems.md#junit-reporter) has JUnit XML output support built in.
- Related to the integration of unittest results is also the
  integration of coverage reports into Jenkins. A good solution
  for this is to use the Jenkins [Cobertura plugin](https://wiki.jenkins-ci.org/display/JENKINS/Cobertura+Plugin), generate the coverage report with lcov and convert it with the [lcov-to-cobertura-xml](https://github.com/eriwen/lcov-to-cobertura-xml.git) script. The lcov HTML reports can also be included with the Jenkins [HTML publisher plugin](https://wiki.jenkins-ci.org/display/JENKINS/HTML+Publisher+Plugin)
- Easiest to integrate with C/C++ builds is the Jenkins [Warnings plugin](https://wiki.jenkins-ci.org/display/JENKINS/Warnings+Plugin) as it natively suports GCC warnings.


## Firefox Addons

It lists the installed Firefox addons. Useful for disaster
recovery purposes. It also solves this problem: you have a bunch of
Firefox addons installed that you want to replicate to another
user account.

Example:

As user X on computer A:

    $ ./firefox-addons.py -o addons.csv

Transfer the file to user Y on computer B and execute:

    $ cut -f1 -d, addon.csv | tail -n +2 | xargs firefox

After that, you 'just' have to click a bunch of 'Add to Firefox'
buttons and close some tabs.


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

## Macgen

When creating network interfaces (dummy, bridge, tap, macvlan,
macvtap, veth, ...) on usually can omit a MAC address because the
Linux kernel automatically generates and assigns one. Those MAC
addresses come from the private/internal unicast namespace which
is described by the following regular expression:

    .[26ae]:..:..:..:..:..

(cf. [locally administered
addresses](https://en.wikipedia.org/wiki/MAC_address#Universal_vs._local)

The macgen scripts also randomly generate MAC addresses from
this namespace. Those can be used for explicitly specifying
MAC addresses in network setup scripts. For example, to get
reproducible results or to avoid having to query MAC addresses
of just newly created virtual interfaces.


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

## Swap

[Since](https://github.com/torvalds/linux/commit/bd42998a6bcb9b1708dac9ca9876e3d304c16f3d)
[2014
(3.15)](https://github.com/torvalds/linux/commit/da1ce0670c14d8380e423a3239e562a1dc15fa9e)
([cf. the development](https://lwn.net/Articles/569134/)) Linux
implements the `RENAME_EXCHANGE` flag with the `renameat2(2)`
system call for atomically exchanging the filenames of two files.
The `swap` utility exposes this functionality on the command
line.

Example:

    $ echo bar > bar; echo foo > foo
    $ ls -i bar foo
    1193977 bar  1193978 foo
    $ cat bar foo
    bar
    foo
    $ ./swap bar foo
    $ ls -i bar foo
    1193978 bar  1193977 foo
    $ cat bar foo
    foo
    bar
    $ ./swap bar foo
    $ ls -i bar foo
    1193977 bar  1193978 foo
    $ cat bar foo
    bar
    foo

Beside the use cases mentioned in the [`renameat(2)` man
page](http://man7.org/linux/man-pages/man2/rename.2.html),
atomically filename swapping can be handy for e.g. log file
rotation. There, any time window where the log filename is
missing is eliminated.

The `swap.c` source code also functions as example of how a
system call can be called when glibc doesn't provide a wrapper for
it.

Not every filesystem necessarily supports `RENAME_EXCHANGE`.
First supported by EXT4 in 2014, e.g. Btrfs supports it [since 2016
(Linux 4.7)](https://kernelnewbies.org/Linux_4.7#head-0b57342c7fb5702b7741afbd6cd55410f84c4b34).
AUFS (a union FS) doesn't support `RENAME_EXCHANGE`, but Overlay
FS does. AUFS isn't part of the Linux kernel (in contrast to
Overlay FS) but [used by some Docker
versions](https://docs.docker.com/engine/userguide/storagedriver/aufs-driver/#configure-docker-with-aufs),
by default. Docker supports several storage backends and there is
also a [backend that uses Overlay
FS](https://docs.docker.com/engine/userguide/storagedriver/overlayfs-driver/#overlayfs-and-docker-performance).
Some versions use that by default. On Linux, one can verify the
type of the filesystem a file or directory is part of via:

    $ stat -f -c %T somefile


## User-Installed

`user-installed.py` lists all the packages that were manually
selected, i.e. that are marked as user-installed in the local
package database because a user explicitly installed them. That
means packages that were installed by the system installer or as
automatic dependencies aren't listed.

It supports different distributions:
Fedora, CentOS, RHEL, Termux, Debian and Ubuntu

Such a package list can be used for:

- preparing a kickstart file
- 'cloning' a good package selection of one system
- restoring the package selection after a vanilla install (e.g.
  because of a major distribution version upgrade or a system
  recovery)

Excluding the automatically installed packages from the list
protects against:

- installing old dependency packages that are now obsolete on the
  new version of the distribution
- wrongly marking the old dependency packages as user-installed
  on the new system
- and thus making auto-cleaning after a future package removal of
  then unneeded dependency packages ineffective
- failed installs due to dependency packages that are removed in
  the new distribution version

Example for restoring a package list on a Fedora system:

    # dnf install $(cat example-org.pkg.lst)

Ignoring any unavailable packages:

    # dnf install --setopt=strict=0 $(cat example-org.pkg.lst)

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
[gnutls]: https://gnutls.org/
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
[jenkins]: https://jenkins.io/
[junit]: https://wiki.jenkins-ci.org/display/JENKINS/JUnit+Plugin
[libcheck]: https://libcheck.github.io/check/
[ascii]: https://en.wikipedia.org/wiki/ASCII
[bcd]: https://en.wikipedia.org/wiki/Binary-coded_decimal

