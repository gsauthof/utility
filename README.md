[![Build Status](https://travis-ci.org/gsauthof/utility.svg?branch=master)](https://travis-ci.org/gsauthof/utility)

This repository contains a collection of command line utilities.

- [addrof](#addrofdevof)
    -- list IP address(es) of network devices
- arsort
    -- topologically sort static libraries
- [ascii](#ascii)
    -- pretty print the ASCII table
- benchmark.sh
    -- run a command multiple times and report stats
- benchmark.py
    -- run a command multiple times and report stats (more features)
- [check2junit](#check2junit)
    -- convert libcheck XML to Jenkins/JUnit compatible XML
- [check-cert](#check-cert)
    -- check approaching expiration/validate certs of remote servers
- [check-dnsbl](#check-dnsbl)
    -- check if mailservers are DNS blacklisted/have rDNS records
- chromium-extensions
    -- list installed Chromium extensions
- [cpufreq](#cpufreq)
    -- print current CPU frequency using CPU counters
- [dcat](#dcat)
    -- decompressing cat (autodetects gzip/zstd/bz2/...)
- [dcheck](#dcheck)
    -- run a program under DBX's memory check mode
- devof
    -- list network device names given an address (prefix)
- disas
    -- disassemble a certain function
- [dtmemtime](#dtmemtime)
    -- measure high-water memory usage of a process and its descendants under Solaris
- exec
    -- change argv[0] of a command
- [firefox-addons](#firefox-addons)
    -- list installed Firefox addons
- gs-ext
    -- list and manage installed Gnome Shell Extensions
- isempty
    -- detect empty images (e.g. in batch scan results)
- [latest-kernel-running](#latest-kernel)
    -- is the latest installed kernel actually running?
- [lockf](#lockf)
    -- protect command execution with a lock
- lsata.sh
    -- map ataX kernel log ids to /dev/sdY devices
- [macgen](#macgen)
    -- randomly generate a private/internal MAC address
- macgen.py
    -- Python implementation of macgen
- [oldprocs](#oldprocs)
    -- list running (and possibly restart) old processes/services whose object
      files were updated
- [pargs](#pargs)
    -- display argv and other vectors of PIDs/core files
- [pdfmerge](#pdfmerge)
    -- vertically merge two PDF files (i.e. as two layers)
- [pldd](#pldd)
    -- list shared libraries linked into a running process
- pwhatch
    -- generate secure and easy to communicate passwords
- [remove](#remove)
    -- sync USB drive cache, power down and remove device
- reset-tmux
    -- reset a tmux session after binary data escaped to the console
- ripdvd
    -- copy each DVD track into a nicely named .vob file
- [searchb](#searchb)
    -- search a binary file in another
- [silence](#silence)
    -- silence stdout/stderr unless command fails
- [silencce](#silencce)
    -- C++ implementation of silence
- [swap](#swap)
    -- atomically exchange names of two files on Linux
- train-spam
    -- feed spam maildir messages into bogofilter and remove them
- unrpm
    -- extract an RPM file
- [user-installed](#user-installed)
    -- list manually installed packages on major distributions

For example:

    $ crontab -l
    15 10 * * * silence backup_stuff.sh /home


2016, Georg Sauthoff <mail@gms.tf>


## TOC Tail

To skip over the utility sections:

- [Build Instructions](#build-instructions)
- [Unittests](#unittests)
- [License](#license)

## Addrof/Devof

In the spirit of `pidof` these utilities list the IP address(es) of
a network devices or the name(s) of network device(s) that use an
IP address (prefix).

Example:

```
$ addrof enp0s31f6
203.0.113.23
2001:DB8:1337:2323::cafe
$ addrof -4 enp0s31f6
203.0.113.23
$ devof 203.0.113.0/24
enp0s31f6
```

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

Note that your default resolving nameserver might reply
incorrectly to some blacklist queries. It is thus advisable to
test some well known listed/non-listed addresses. See also the
`--ns` option. There are also some options for selecting some
predefined public DNS resolvers (e.g.  Google Public DNS,
Cloudflare, OpenDNS, Quad9). But again, some of those servers may
filter out some blacklists. For example, as of 2019-12-29, only
Cloudflare and OpenDNS return zen.spamhaus.org blacklisting
records for `117.246.201.146` and `116.103.227.39` while Google
and Quad9 don't. See also the [Spamhaus.org FAQ][spamhausfaq].

[spamhausfaq]: https://www.spamhaus.org/faq/section/DNSBL%20Usage#261

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

## disas

Disas is a small wrapper around objdump/gdb for disassembling a
given function.

Examples:

    $ disas a.out main
    $ disas a.out 10144 -a    # dump function that includes this addr
    $ disas a.out foobar -f   # also dump functions that call foobar
    $ disas a.out '.*xyz'     # dump all functions regex match
    # disas a.out cmpte --gdb # disassemble using gdb instead of objdump

Note that recent versions of `objdump` support the
`--disassemble=fn` option (e.g. on Fedora 31), but e.g. the
objdump on CentOS 7 doesn't. The wrapper doesn't require this
option and thus also runs on older systems.

The wrapper doesn't always use gdb, because objdump is more
widely available and is more flexible when it comes to dumping
multiple functions.

## CPUfreq

The `cpufreq` utility prints the current CPU frequency of each
CPU core. It computes the frequency from a set of CPU counters
(see the help text in `cpufreq.py` and further source code
comments there for details).

Thus, it doesn't rely on frequency-scaling support being enabled
in the Linux kernel. When frequency-scaling support is disabled
in the kernel and/or the BIOS the files
`/sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq` don't
exist, commands like `cpupower frequency-info` don't work, and
(as of 2020) the frequency  obtained via `/proc/cpuinfo` isn't
necessarily correct.

In any case, this tool can be used to cross-check CPU frequency
assumptions and frequency scaling results.

## dcat

The `dcat` utility automatically decompresses files with the
right algorithm, on-the-fly. Thus, it's a decompressing cat. It
detects the compression file format by looking at the first
[magic bytes][magic] and thus ignores any file extension.
Uncompressed files or files it doesn't recognize the `dcat`
concatenates as is.

Examples:

    $ echo 'Hello World' | zstd -c | ./dcat
    Hello World
    $ ./dcat foo.txt.gz bar.txt.zst baz.txt

For the actual decompressing, `dcat` execs a helper like `zcat`
or `bzcat`. Currently, it autodetects gzip, Zstandard, LZ4, bzip2
and XZ.

[magic]: https://en.wikipedia.org/wiki/Magic_number_(programming)#Magic_numbers_in_files

## DCheck

The dcheck.sh utility runs a program with the supplied arguments
inside the [DBX debugger][dbx] with [memory checking mode][dbxcheck] enabled.

Example:

    $ dcheck someprog arg1 arg2

For each memory access issue the problem details and a stacktrace
are printed and the execution is resumed. After the leak report
is printed the DBX is automatically exited. The main work does
[Ksh][ksh] function that is called by the wrapper (cf.
check.dbx). Yes, DBX embeds a Ksh 88 compatible interpreter (in
contrast to GDB which supports Guile and Python scripting).

The memory check feature of [Solaris Studio's DBX][ssdbx] detects reads of
uninitialized memory and heap based memory issues like buffer
overflowing reads and writes (i.e. out-of-bounds writes). It also
tracks allocations for detecting memory leaks. The DBX
dynamically install hardware watch points for the access
checking.

On [Solaris][solaris]/[SPARC][sparc] it is a good and relatively
widely available alternative to a subset of the functionality of
excellent open source tools like [Valgrind (memcheck)][valgrind]
or the [Address/Leak Sanitizers][asan] (which aren't available on
Solaris/SPARC).

## dtmemtime

This utility measures the high-water memory usage of a process
and all its descendants under [Solaris][solaris]. The main work
does a [DTrace][dtrace] script that hooks into some syscalls
(hence the name). It's designed to work under Solaris 10 (or
later).

Example:

    $ cat foo.sh
    #!/bin/bash
    find "$1" | sed 's@/[^/]\+$@@' | sort | uniq -c | sort -n -k 1,1 -r | head
    $ dtmemtime foo.sh /usr/share
    pid 23066 (#1) (gsed) runtime: 691 ms, highwater memory: 116208 bytes
    pid 23065 (#1) (gfind) runtime: 691 ms, highwater memory: 968176 bytes
    pid 23069 (#1) (gsort) runtime: 1584 ms, highwater memory: 8619504 bytes
    pid 23067 (#1) (gsort) runtime: 1560 ms, highwater memory: 8619504 bytes
    pid 23063 (#1) (foo.sh) runtime: 1601 ms, highwater memory: 97208 bytes
    pid 23070 (#1) (ghead) runtime: 1583 ms, highwater memory: 91632 bytes
    pid 23068 (#1) (guniq) runtime: 1559 ms, highwater memory: 91632 bytes
    total runtime: 1601 ms, highwater memory: 18571096 bytes

Say `mycmd.sh` executes some processes in parallel, e.g. with a
[shell-pipe command][pipeline], then the high-water memory
measurement takes the memory usages of all those processes into
account.

The [DTrace][dtrace] script installs some syscall and process
probes, e.g. a probe into the [`brk` syscall][brk] and probes
into syscalls used for managing anonymous memory mappings. Note
that the Solaris 10 libc memory management exclusively uses
`brk`. The easiest way to get a more modern memory allocator that
also creates anonymous memory mappings (like under Linux) is to
link against [`libumem`][umem] and set an environment variable.

[dtrace]: https://en.wikipedia.org/wiki/DTrace
[brk]: https://en.wikipedia.org/wiki/Sbrk
[umem]: https://en.wikipedia.org/wiki/Libumem
[pipeline]: https://en.wikipedia.org/wiki/Pipeline_(Unix)


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

## Oldprocs

The oldprocs utility lists processes and services that need to be
restarted because their executable or one of its libraries has
changed, e.g. after a `dnf update` and before the next system
reboot.

It detects if a process belongs to a systemd service and prints
the matching `systemctl` command line to restart it.
Optionally, with `--restart` the utility automatically restarts
the detected services.

Thus, it's well suited for automatic system updates. Think:
something like `dnf -y update` runs from a cron-job followed by
`oldprocs --restart`. This makes sense for systems where it's more
important to get a required security update as fast as possible
than avoiding a potential service breakage due to the new update.
Arguably, depending on you distribution and package selection
this risk is rather small, anyways.

Another use case is to verify that all processes run current
binaries and thus nobody forgot to restart some services or
reboot the system, when necessary.

Cases where a service can't be restarted (i.e. dbus), a restart
wasn't sufficient (e.g. old processes are still around), a
restart would eject a user from a graphical session or there are
other non-service processes are signaled via an exit status
unequal zero and diagnostic messages.

Example output when running with root privileges:

    # ./oldprocs
    You have to restart the following system services:

    systemctl restart libvirtd.service

    You have to restart the following user services:

    sudo -u '#1000' systemctl --user restart evolution-addressbook-factory.service
    sudo -u '#1000' systemctl --user restart evolution-calendar-factory.service
    sudo -u '#1000' systemctl --user restart evolution-source-registry.service
    sudo -u '#1000' systemctl --user restart gnome-terminal-server.service

    The following user processes must be restarted manually
    (or a session logoff/login might take care of them):

    /usr/bin/clementine (deleted) (uid 1000) - pids: 20826
    /usr/bin/clementine-tagreader (deleted) (uid 1000) - pids: 20831 20832 20833 20834
    /usr/lib64/firefox/firefox (deleted) (uid 1000) - pids: 2601 2696 2981 7154 21053
    /usr/libexec/gnome-shell-calendar-server (uid 1000) - pids: 2142
    /usr/libexec/goa-daemon (uid 1000) - pids: 2163
    # echo $?
    11

To automatically restart all old system processes:

    # ./oldprocs --restart

The auto-restart just includes processes belonging to systemd
services that belong to the systemd instance the user has direct
access to.

Example output when user with id 1000 executes it:

   $ ./oldprocs
    You have to restart the following user services:

    systemctl --user restart evolution-addressbook-factory.service
    systemctl --user restart evolution-calendar-factory.service
    systemctl --user restart evolution-source-registry.service
    systemctl --user restart gnome-terminal-server.service

    The following user processes must be restarted manually
    (or a session logoff/login might take care of them):

    /usr/bin/clementine (deleted) (uid 1000) - pids: 20826
    /usr/bin/clementine-tagreader (deleted) (uid 1000) - pids: 20831 20832 20833 20834
    /usr/lib64/firefox/firefox (deleted) (uid 1000) - pids: 2601 2696 2981 7154 21053
    /usr/libexec/gnome-shell-calendar-server (uid 1000) - pids: 2142
    /usr/libexec/goa-daemon (uid 1000) - pids: 2163 

The difference is that the restart commands for the systemd
user services are generated from the user's perspective - and the
old system processes are not included as permissions are lacking
to access the relevant directories and files under `/proc`.

How does it work: oldprocs scans through `/proc` to find out
which processes are running, which executables they were started
with, whether those were replaced, changed dates, which shared
objects are linked into the process, whether those were changed,
which systemd service the process is part of, if any, etc.

Related tools: There is
[tracer](https://github.com/FrostyX/tracer) which also lists
outdated processes and provides restart commands for some
services. It is written in Python and in my experience, it
sometimes runs very slow and the output sometimes contains
inaccuracies (e.g. wrong restart commands, false negatives, wrong
diagnostics such as a required system reboot which isn't really
necessary).  As of 2018, tracer doesn't support the automatic
restarting of outdated services.

In contrast to that, oldprocs is written in C++, runs very fast,
supports the automatic restarting of outdated services and
provides some fresh diagnostics.

The utility `latest-kernel-running.sh` complements `oldprocs` as
it checks whether a system reboot is necessary due to a previous
kernel update.

## Pargs

Pargs displays the argument vector (argv) of a running process or
the one that is included in the core file of a process, under
Linux.  In addition, it supports printing the environment vector
(envp) and the auxiliary vector (auxv) including some pretty
printing and dereferencing some interesting addresses (e.g. the
executable filename or the platform string).

Examples:

    $ pargs $pid
    $ pargs -l $pid
    $ pargs -aex $pid
    $ pargs -aexv some_core

It is inspired by [Solaris' pargs][solpargs] command. Similar to Linux,
Solaris also has a `/proc` filesystem that provides much
information about each process. In contrast to Linux, the pseudo
files all contain binary data, i.e. following some struct
definitions. Thus, it's natural that Solaris has a whole p-family
of commands to deal with processes. Some like `pgrep` and `pkill`
are also available on Linux, for a long time, but to the authors
knowledge, this `pargs` in this repository is the first Linux pargs.

The default mode, displaying the argument vector of running
process (`pargs $pid`) can be approximated under Linux like this:

    $ tr '\0' '\n' < /proc/$pid/cmdline

Displaying the environment vector works analogously, but the
auxiliary vector (`/proc/$pid/auxv`) is more complicated because
it's just an array of integers (64 or 32 bit depending on the
architecture/process) that also references addresses in the
processes address space.

The complexity grows when we want to obtain the same information
from a core file. Some can be displayed from gdb, but this
requires the availability of the executable file, as well.
`pargs` just requires the core file.

In comparison to Solaris, some parts arguably can be obtained
more easily (e.g.  `/proc/$pid/{cmdline,environ}`) while others
require more effort, on Linux. On Solaris, the core file contains
some structs that include copies of the argument and environment
vectors, thus, it's straight forward to access that information.
This is not the case on Linux, where one has to search for the
vectors in the right memory section.

Tested on:

- Fedora 26 x86-64, both with 32/64 bit executables/core files,
  and with core files from different byte-order architectures
- RHEL 6 (needs `-s` there)
- Debian 8 ppc64 (PowerPC), both with 32/64 bit executables/core files,
  and with core files from different byte-order architectures

In general, the code is portable, e.g. when reading core files,
pargs supports word sizes and [byte orders][endian] (i.e. little
vs. big endian) different from the native one. For example, pargs
running on x86-64 Linux is able to print the argument vector,
auxiliary vector etc. of a core file that was generated on a
big-endian PowerPC Linux system.

## PDFmerge

The `pdfmerge` utility merges two PDF files such that the pages
of the second PDF overlay the ones of the first one. In that
sense it's a vertical merge. Example:

    $ ./pdfmerge text-only.pdf image-only.pdf doc.pdf

Main use case for this is to merge a text-only (transparent) PDF
file (OCR result) with an image-only PDF file (scan result). See
also [`adf2pdf.py`][adf2pdf] for a complete workflow.

It supports both the [PyPDF2][pypdf2] and [pdfrw][pdfrw] packages
(cf. the `--pdfrw` option), thus, it's also a small case study of
the different PDF manipulation APIs. Fedora packages these
dependencies as `python3-PyPDF2` and `python3-pdfrw`.

Related PDF tools:

- PDFtk - supports vertical merging, as well (`pdftk
  text-only.pdf multibackground image-only.pdf output doc.pdf`),
  but it isn't widely available anymore. At least [Fedora has
  removed][pdftkfed] it from its main repository. Since parts of it are
  written Java it's arguably harder to install than this tiny
  Python utility.
- pdf-stapler - supports a small subset of pdftk functionality,
  but, currently, [doesn't support][staplernot] such a merge
  operation
- mutool - from the makers of mupdf, doesn't support such a merge
- poppler-utils - derived from the xpdf utils, doesn't support
  vertical merging (just horizontal merging with `pdfunite`)

[pypdf2]: https://pythonhosted.org/PyPDF2/
[pdfrw]: https://github.com/pmaupin/pdfrw
[adf2pdf]: https://github.com/gsauthof/adf2pdf
[pdftkfed]: https://ask.fedoraproject.org/en/question/65261/pdftk-not-in-f21/
[staplernot]: https://github.com/hellerbarde/stapler/issues/35

## PLDD

The `pldd` command lists all shared libraries loaded into a
running process. Often, the result is similar to what `ldd`
prints for the executable. But the running process may actually
end up with a different set of loaded shared libraries, e.g. due
to a modified environment (e.g. `LD_LIBRARY_PATH`) or some
dynamic logic (e.g. when the process calls `dlopen()`).

Example:

    $ ./pldd.py $$
    /usr/lib64/ld-2.26.so
    /usr/lib64/libc-2.26.so
    /usr/lib64/libdl-2.26.so
    /usr/lib64/libgdbm.so.4.0.0
    [..]

Both implementations `pldd.sh` and `pldd.py` basically yield the
same results. The difference is just that `pldd.sh` prints the
current in-process-memory shared object table as created and
maintained by the `ld.so` dynamic linker. Whereas `pldd.py`
prints the linked shared libraries from the kernel's point of
view. The main difference is then that the kernel resolves all
symbolic links, e.g. `pldd.sh` may report `/lib64/libc.so.6`
while `pldd.py` reports `/usr/lib64/libc-2.26.so` on systems
where `/lib64` symlinks to `/usr/lib64`.

Related tools: [Solaris 10 has `pldd`][pldd-sol] which works like
`pldd.sh` - but also supports reading the in-memory linker table
from a single core file. Glibc comes with a `pldd` command
(doesn't support core files) but [it's seriously broken for
years][pldd-glibc] (since glibc 2.19) - i.e. it goes into an
endless loop instead of printing any results.

[pldd-sol]: https://www.freebsd.org/cgi/man.cgi?query=pldd&apropos=0&sektion=0&manpath=SunOS+5.10&arch=default&format=html
[pldd-glibc]: https://manpages.debian.org/stretch/manpages/pldd.1.en.html#BUGS

## Remove

Synchronize the write cache of an external USB disk, power it
down and remove its device. Example:

    $ ./remove.py /dev/sdb

The main use cases for this is to power down an external disk
gracefully instead of suddenly removing the power (i.e. when it's
still running as it's unplugged) which should reduce mechanical
stress.  Also, the explicit flushing of the drive's cache
shouldn't hurt.  It should help after writing data directly to
the disk (e.g. with `dd`) or with low-quality USB enclosures that
don't flush the write cache on other synchronisation commands.

Related commands:

- `udisksctl power-off --block-device /dev/sdb` - similar
  effect, only available on systems where the `udisks2` service
  is available and running
- `eject /dev/sdb` - may work for some hardware, unclear what
  features it supports for USB disks and doesn't really support
  error reporting. Doesn't work for the author under Fedora 27,
  i.e. it doesn't flush and it doesn't power-down.

See also [Gracefully shutting down USB disk drives before
disconnect][remove-se] on Unix-SE.

[remove-se]: https://unix.stackexchange.com/q/444611/1131

## Searchb

The purpose of `searchb` is quite simple: search if a file is
included in another file and if it is report its offset. See also
[this Unix SE question][searchse] about this use case.

Example:

    $ searchb queryfile targetfile
    1337
    $ searchb queryfile0 targetfile
    $ echo $?
    1

The obvious implementation choice is to map both files into
memory and use a text-book text search algorithm such as
[Two-Way][twoway], [BMH][bmh] or [KMP][kmp] on it. Simple and at
the same time efficient. A tiny complication is that POSIX
`mmap()` doesn't allow mapping zero length files, thus, one has
to add a special case for this (as - say - searching for a
pattern in an empty file shouldn't be considered an error).

As a small case study, this repository contains several
equivalent implementations of this small utility written in
different languages: C, C++, Python, Go and Rust

Even with such a small example one can see the advantages,
disadvantages and trade-offs associated with the different
languages when it comes to system programming.

Observations:

- C: as always, some boilerplate error checking code necessary,
  otherwise straight forward. Unfortunately, POSIX doesn't
  specify the range equivalent to `strstr()`, but modern
  Unix-like operating systems like Linux provide `memmem()`. The
  Linux version of `memmem()` is highly optimized.
- C++: the C++ STL doesn't include a convenient API for memory
  mapping a file, thus one either has to use the low-level C API
  or another library. Boost has 2 mmap APIs (in iostreams and
  interprocess) but both don't allow empty mappings. Libixxxutil
  does allow them thus it's used. The STL includes a generic
  search algorithm, although it doesn't have to better than
  a naive implementation. Boost also includes BMH and KMP
  implementations.
- Python: just a few lines necessary to get the job done. Very
  elegant and the standard Python library contains all the needed
  pieces. Perhaps a tiny downer is that the search algorithm isn't
  available as orthogonal function, instead it's `mmap.find()`,
  `bytes.find()` etc. Likely, one implementation is shared,
  internally and the standard library is usually mature enough to
  expose all the obvious helper functions (like in this case).
- Go: Similar to C and C++, Go also allows for an orthogonal
  implementation of memory mapping and searching. The standard
  library comes with `bytes.Index()` that implements some
  special cases for different pattern lengths (including
  [Rabin-Karp][rabink]) and
  works on any byte slices, while the `Mmap()` syscall also
  returns a zero copy byte slice. Still, one cannot call this
  implementation extremely elegant: Since Go doesn't have exceptions
  one has to invest in some repetitive error checking, there is
  nothing similar to [RAII][raii] and the standard library
  doesn't include a high-level mmap API (heck, even the syscalls
  aren't part of the standard library). As a consequence, the Go
  version isn't much shorter than the C version
- Rust: the view-like slice syntax, move-semantics etc. are
  well suited for this job, e.g. to allow for an orthogonal
  combination of memory mapping and search. Unfortunately, Rust's
  standard library neither contains a search algorithm for `u8`
  byte slices (just for UTF strings) nor an mmap API. However,
  external [crates][crate] are available for both tasks. Using
  those, the implementation is very short and elegant, as well.
  Although Rust also doesn't have exceptions, at least it has
  some syntactic sugar to avoid some error checking boilerplate
  code (e.g. the `?` operator).
- In general, most of the mid- to high-level memory map APIs in
  the different languages don't improve upon the POSIX limitation
  to fail on zero length mappings. Just returning an empty range
  simplifies its use (cf. the `mmap()` helper in the C and go
  versions and libixxxutil) as some special error handling can be
  omitted.
- Performance: the C/C++/Python/Go/Rust versions are basically equally
  fast. The `memmem()` likely contains some SIMD code and the
  Rust search library has optional SIMD support, although it
  requires support for inline assembly, which isn't available in
  the current Rust stable (e.g. version 1.25).


[searchse]: https://unix.stackexchange.com/q/39728/1131
[twoway]: http://www-igm.univ-mlv.fr/~lecroq/string/node26.html
[bmh]: https://en.wikipedia.org/wiki/Boyer%E2%80%93Moore%E2%80%93Horspool_algorithm
[kmp]: https://en.wikipedia.org/wiki/Knuth%E2%80%93Morris%E2%80%93Pratt_algorithm
[raii]: https://en.wikipedia.org/wiki/Resource_acquisition_is_initialization
[crate]: https://doc.rust-lang.org/book/first-edition/crates-and-modules.html
[rabink]: https://en.wikipedia.org/wiki/Rabin%E2%80%93Karp_algorithm

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
    $ cd utility
    # git submodule update --init

Out of source builds are recommended, e.g.:

    $ mkdir utility-bin && cd utility-bin
    $ cmake ../utility
    $ make

Or to use ninja instead of make and create a release build:

    $ mkdir utility-bin-o && cd utility-bin-o
    $ cmake -G Ninja -D CMAKE_BUILD_TYPE=Release ../utility
    $ ninja-build

Install it (for packaging):

    $ mkdir build
    $ cd build
    $ cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release \
                 -DCMAKE_INSTALL_PREFIX=/usr/local
    $ DESTDIR=$PWD/out ninja install

If you want to directly install it into the final destination you
can drop the `DESTDIR=...` part.


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
[dbx]: https://en.wikipedia.org/wiki/Dbx_(debugger)
[dbxcheck]: https://docs.oracle.com/cd/E19205-01/819-5257/blahg/index.html
[ksh]: https://en.wikipedia.org/wiki/KornShell
[ssdbx]: https://docs.oracle.com/cd/E24457_01/html/E21993/index.html
[valgrind]: http://valgrind.org/
[asan]: https://github.com/google/sanitizers/wiki/AddressSanitizer
[solaris]: https://en.wikipedia.org/wiki/Solaris_(operating_system)
[sparc]: https://en.wikipedia.org/wiki/SPARC
[endian]: https://en.wikipedia.org/wiki/Endianness
[solpargs]: https://www.freebsd.org/cgi/man.cgi?query=pargs&apropos=0&sektion=0&manpath=SunOS+5.10&arch=default&format=html
