// pq - query process/thread attributes
//
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: © 2020 Georg Sauthoff <mail@gms.tf>

#include <algorithm>     // search(), find()
#include <charconv>      // to_chars(), from_chars()
#include <functional>    // default_searcher
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>
#include <array>
#include <memory>        // unique_ptr
#include <optional>
#include <regex>         // requires GCC > 4.8


#include <ixxx/util.hh>
#include <ixxx/ansi.hh>
#include <ixxx/posix.hh>
#include <ixxx/linux.hh>
#include <ixxx/sys_error.hh>

#include <stdlib.h>      // exit()
#include <string.h>      // strlen(), memcmp(), memchr(), ...
#include <fcntl.h>       // O_RDONLY
#include <unistd.h>      // getopt()
#include <assert.h>

#include "syscalls.hh"


using namespace std;



// cf. https://gcc.gnu.org/bugzilla/show_bug.cgi?id=88545
template <typename Itr, typename T>
inline Itr fast_find(Itr b, Itr e, const T &v)
{
    auto t = memchr(&*b, v, e-b);
    if (t)
        return Itr(t);
    else
        return e;
}

template <typename Itr, typename T>
inline Itr fast_rfind(Itr b, Itr e, const T &v)
{
    auto t = memrchr(&*b, v, e-b);
    if (t)
        return Itr(t);
    else
        return e;
}


inline string_view nth_col(const string_view &v, unsigned x)
{
    auto p = v.begin();
    for (unsigned i = 0; i < x; ++i) {
        for ( ; p != v.end() && (*p != '\t' && *p != ' '); ++p)
            ;
        for ( ; p != v.end() && (*p == '\t' || *p == ' '); ++p)
            ;
    }
    auto ws = { '\t', ' ', '\n' };
    auto e = find_first_of(p, v.end(), begin(ws), end(ws));
    return string_view(p, e - p);
}

enum class Column {
    AFFINITY  , // /proc/$pid/status::Cpus_allowed_list
    CLS       , // scheduling class, proc/$pid/stat
    CMD       , // /proc/$pid/commandline
    COMM      , // /proc/comm or /proc/$pid/status::Name or /proc/$pid/stat
    CPU       , // last run on this CPU, /proc/$pid/stat::processor
    CWBYTE    , // /proc/$pid/io::cancelled_write_bytes
    CWD       , //
    ENV       , //
    EXE       , //
    FDS       , // ls /proc/$pid/fd | wc -l
    FDSIZE    , // /proc/$pid/status::FDSize
    FLAGS     , // process flags, /proc/$pid/stat
    GID       , // effective ...
    HELP      , // dummy, displays column help ...
    HUGEPAGES , // /proc/$pid/status::HugetlbPages
    LOGINUID  , // /proc/$pid/loginuid
    MAJFLT    , // major page faults /proc/$pid/status
    MINFLT    , // minor page faults /proc/$pid/status
    NICE      , // /proc/$pid/stat
    NUMAGID   , // NUMA group ID, /proc/$pid/status::Ngid
    NVCTX     , // non-voluntary context switches /proc/$pid/status
    PID       , //
    PPID      , //
    RBYTE     , // /proc/$pid/io::read_bytes
    RCHAR     , // /proc/$pid/io::rchar
    RSS       , //
    RTPRIO    , // /proc/$pid/stat
    SLACK     , // /proc/$pid/timerslack_ns
    STACK     , //
    STATE     , // /proc/$pid/status or /proc/$pid/stat
    STIME     , // start time /proc/$pid/stat
    SYSCALL   , // /proc/$pid/syscall
    SYSCR     , // /proc/$pid/io::syscr
    SYSCW     , // /proc/$pid/io::syscw
    THREADS   , //
    TID       , //
    UID       , // effective ...
    UMASK     , //
    USER      , //
    VCTX      , // voluntary context switches /proc/$pid/status
    VSIZE     , //
    WBYTE     , // /proc/$pid/io::write_bytes
    WCHAN     , // /proc/$pid/wchan
    WCHAR     , // /proc/$pid/io::wchar

    END_OF_ENUM // just a sentinel for this enum ...

    // TODO:
    //
    // /proc/$pid/status: THP_enabled, CoreDumping, VmSwap, ...
    // /proc/$pid/limits
    // /proc/$pid/cgroup
    // /proc/$pid/auxv
    // real uid/gid
};

static const string_view col2header[] = {
    "aff"       , // AFFINITY
    "cls"       , // CLS
    "cmd"       , // CMD
    "comm"      , // COMM
    "cpu"       , // CPU
    "cwbyte"    , // CWBYTE
    "cwd"       , // CWD
    "env"       , // ENV
    "exe"       , // EXE
    "fds"       , // FDS
    "fdsz"      , // FDSIZE
    "flags"     , // FLAGS
    "gid"       , // GID
    "XXXhelp"   , // HELP
    "hugepages" , // HUGEPAGES
    "loginuid"  , // LOGINUID
    "majflt"    , // MAJFLT
    "minflt"    , // MINFLT
    "nice"      , // NICE
    "nid"       , // NUMAGID
    "nvctx"     , // NVCTX
    "pid"       , // PID
    "ppid"      , // PPID
    "rbyte"     , // RBYTE
    "rchar"     , // RCHAR
    "rss"       , // RSS
    "pri"       , // RTPRIO
    "slack"     , // SLACK
    "stack"     , // STACK
    "state"     , // STATE
    "stime"     , // STIME
    "syscall"   , // SYSCALL
    "syscr"     , // SYSCR
    "syscw"     , // SYSCW
    "threads"   , // THREADS
    "tid"       , // TID
    "uid"       , // UID
    "umask"     , // UMASK
    "user"      , // USER
    "vctx"      , // VCTX
    "vsize"     , // VSIZE
    "wbyte"     , // WBYTE
    "wchan"     , // WCHAN
    "wchar"       // WCHAR
};
static_assert(sizeof col2header / sizeof col2header[0] == static_cast<size_t>(Column::END_OF_ENUM));

static const char * const col2help[] = {
    "CPU (core) affinity, i.e. task only runs on those cores"       , // AFFINITY
    "scheduling class", // CLS
    "command line, i.e. the argument vector"       , // CMD
    "process/thread name"      , // COMM
    "last ran on that CPU (core)"       , // CPU
    "write bytes, cancelled"       , // CWBYTE
    "current wording directory"       , // CWD
    "display an environment variable, e.g. env:MYID"       , // ENV
    "process' executable"       , // EXE
    "number of open files"       , // FDS
    "number of allocated file descriptor slots"       , // FDSIZE
    "process flags (e.g. PF_KTHREAD, PF_WQ_WORKER or PF_NO_SETAFFINITY)", // FLAGS
    "group ID"       , // GID
    "XXXhelp", // HELP
    "#hugepages" , // HUGEPAGES
    "login user ID or 2**32-1 if daemon etc."  , // LOGINUID
    "major page faults"    , // MAJFLT
    "minor page faults"    , // MINFLT
    "process niceness", // NICE
    "NUMA group ID"       , // NUMAGID
    "non-voluntary context switches"     , // NVCTX
    "process ID"       , // PID
    "parent process ID"      , // PPID
    "bytes read, actually", // RBYTE
    "bytes read", // RCHAR
    "resident size set in KiB"       , // RSS
    "realtime priority (1-99)", // RTPRIO
    "current timer slack value of a thread in ns", // SLACK
    "top of stack function the task is executing/blocked on (requires root)"     , // STACK
    "state the process is in, e.g. running, sleeping etc."     , // STATE
    "start time in ISO format"     , // STIME
    "current syscall the task is executing/blocked on, if any"   , // SYSCALL
    "number of read syscalls"   , // SYSCR
    "number of write syscalls"   , // SYSCW
    "number of threads of that process/the process the thread is part of"   , // THREADS
    "thread ID"       , // TID
    "(effective) user ID"       , // UID
    "user file creation mask"     , // UMASK
    "(effective) user name", // USER
    "number of voluntary context-switches"      , // VCTX
    "virtual memory usage in KiB"     , // VSIZE
    "bytes written, actually",       // WBYTE
    "kernel function the task waits for, cf. stack (some kernels doesn't support it - e.g. Fedora's doesn't)",       // WCHAN
    "bytes written"       // WCHAR
};
static_assert(sizeof col2header / sizeof col2header[0] == sizeof col2help / sizeof col2help[0]);

static const unsigned col2width[] = {
     3 , // AFFINITY
     3 , // CLS
    15 , // CMD
    15 , // COMM
     3 , // CPU
    11 , // CWBYTE
    15 , // CWD
     8 , // ENV
    10 , // EXE
     3 , // FDS
     3 , // FDSIZE
     5 , // FLAGS
     4 , // GID
     0 , // HELP
    10 , // HUGEPAGES
    10 , // LOGINUID
    10 , // MAJFLT
    10 , // MINFLT
     4 , // NICE
     3 , // NUMAGID
    10 , // NVCTX
     7 , // PID
     7 , // PPID
    11,  // RBYTE
    11,  // RCHAR
     8 , // RSS
     3 , // RTPRIO
     5 , // SLACK
    10 , // STACK
    10 , // STATE
    10 , // STIME
    10 , // SYSCALL
     8 , // SYSCR
     8 , // SYSCW
     7 , // THREADS
     7 , // TID
     4 , // UID
     4 , // UMASK
     8 , // USER
    10 , // VCTX
     8 , // VSIZE
    11 , // WBYTE
    10 , // WCHAN
    11   // WCHAR
};
static_assert(sizeof col2header / sizeof col2header[0] == sizeof col2width / sizeof col2width[0]);

static const unordered_map<string_view, Column> str2column = {
    { "pid"       , Column::PID       },
    { "tid"       , Column::TID       },
    { "comm"      , Column::COMM      },
    { "name"      , Column::COMM      },
    { "exe"       , Column::EXE       },
    { "affinity"  , Column::AFFINITY  },
    { "aff"       , Column::AFFINITY  },
    { "cores"     , Column::AFFINITY  },
    { "wchan"     , Column::WCHAN     },
    { "wchar"     , Column::WCHAR     },
    { "wbyte"     , Column::WBYTE     },
    { "syscall"   , Column::SYSCALL   },
    { "scall"     , Column::SYSCALL   },
    { "ecall"     , Column::SYSCALL   },
    { "syscr"     , Column::SYSCR     },
    { "syscw"     , Column::SYSCW     },
    { "state"     , Column::STATE     },
    { "cmd"       , Column::CMD       },
    { "cmdline"   , Column::CMD       },
    { "cmdline"   , Column::CMD       },
    { "cwd"       , Column::CWD       },
    { "cpu"       , Column::CPU       },
    { "psr"       , Column::CPU       },
    { "core"      , Column::CPU       },
    { "cwbyte"    , Column::CWBYTE    },
    { "gid"       , Column::GID       },
    { "egid"      , Column::GID       },
    { "uid"       , Column::UID       },
    { "euid"      , Column::UID       },
    { "help"      , Column::HELP      },
    { "hugepages" , Column::HUGEPAGES },
    { "hpages"    , Column::HUGEPAGES },
    { "threads"   , Column::THREADS   },
    { "slack"     , Column::SLACK     },
    { "stack"     , Column::STACK     },
    { "ppid"      , Column::PPID      },
    { "rbyte"     , Column::RBYTE     },
    { "rchar"     , Column::RCHAR     },
    { "stime"     , Column::STIME     },
    { "start"     , Column::STIME     },
    { "nvctx"     , Column::NVCTX     },
    { "nctx"      , Column::NVCTX     },
    { "vctx"      , Column::VCTX      },
    { "minfault"  , Column::MINFLT    },
    { "minflt"    , Column::MINFLT    },
    { "majfault"  , Column::MAJFLT    },
    { "majflt"    , Column::MAJFLT    },
    { "umask"     , Column::UMASK     },
    { "loginuid"  , Column::LOGINUID  },
    { "luid"      , Column::LOGINUID  },
    { "rss"       , Column::RSS       },
    { "vsize"     , Column::VSIZE     },
    { "vmem"      , Column::VSIZE     },
    { "fds"       , Column::FDS       },
    { "fdsize"    , Column::FDSIZE    },
    { "numagid"   , Column::NUMAGID   },
    { "numa"      , Column::NUMAGID   },
    { "ngid"      , Column::NUMAGID   },
    { "nid"       , Column::NUMAGID   },
    { "user"      , Column::USER      },
    { "usr"       , Column::USER      },
    { "rtprio"    , Column::RTPRIO    },
    { "prio"      , Column::RTPRIO    },
    { "pri"       , Column::RTPRIO    },
    { "cls"       , Column::CLS       },
    { "class"     , Column::CLS       },
    { "policy"    , Column::CLS       },
    { "sched"     , Column::CLS       },
    { "nice"      , Column::NICE      },
    { "flags"     , Column::FLAGS     },
    { "pf"        , Column::FLAGS     }
};

enum Show_Tasks {
    BOTH,
    KERNEL,
    USER
};

static time_t get_boot_time()
{
    array<char, 64> buf;
    ixxx::util::FD fd("/proc/uptime", O_RDONLY);
    size_t n = ixxx::util::read_all(fd, buf);
    char *b = buf.data();
    char *e = b + n;
    e = fast_find(b, e, '.');

    struct timespec tp;
    ixxx::posix::clock_gettime(CLOCK_REALTIME_COARSE, &tp);

    size_t off = 0;
    auto r = from_chars(b, e, off);
    if (r.ptr != e)
        throw runtime_error("uptime parse error");

    time_t boot_time_s = tp.tv_sec - off;
    return boot_time_s;
}

static size_t parse_uid(const char *s)
{
    size_t uid = 0;
    auto e = s + strlen(s);
    if (e == s)
        throw runtime_error("empty uid/user");
    if (*s < '0' || *s > '9') {
        struct passwd pass;
        struct passwd *res;
        array<char, 4 * 1024> buf;
        ixxx::posix::getpwnam_r(s, &pass, buf.data(), buf.size(), &res);
        if (!res)
            throw runtime_error("user not found");
        uid = pass.pw_uid;
    } else {
        auto r = from_chars(s, e, uid);
        if (r.ptr != e)
            throw runtime_error("uid parse error");
    }
    return uid;
}

struct Args {
    vector<size_t>   pids                                ;
    bool             all_pids         {false}            ;
    optional<size_t> uid                                 ;
    string           regex_str                           ;
    Show_Tasks       show_tasks       {Show_Tasks::BOTH} ;
    bool             traverse_threads {false}            ;
    bool             show_header      {true}             ;

    vector<Column>   columns                             ;
    vector<string>   env_vars                            ;

    time_t           boot_time_s      {0}                ;
    unsigned         clock_ticks      {0}                ;

    unsigned         interval_s       {0}                ;
    unsigned         count            {0}                ;


    void parse(int argc, char **argv);
};

static void help(FILE *o, const char *argv0)
{
    fprintf(o, "%s - query process and thread attributes\n"
            "Usage: %s [-o COL1 COL2..] [-p PID1 PID2..] [OPTS]\n"
            "\n"
            "Options:\n"
            "  -a         list all processes\n"
            "  -c N       repeat N times, if -i is set (default: unlimited)\n"
            "  -e REGEX   filter by regular expression (match against COMM)\n"
            "  -h         display this help\n"
            "  -H         omit header row\n"
            "  -i X       repeat output after X seconds\n"
            "  -k         only list kernel threads\n"
            "  -K         only list user tasks\n"
            "  -o COL..   columns to display (use `-o help` to get a list)\n"
            "  -p PID..   only list the specified processes/threads\n"
            "  -t         also list threads\n"
            "  -u USER    filter by user/uid\n"
            "\n"
            "2020, Georg Sauthoff <mail@gms.tf>, GPLv3+\n"
            ,
            argv0,
            argv0);
}

static void help_col(FILE *o)
{
    fprintf(o, "Available columns:\n"
            "\n");

    unordered_map<Column, vector<string>> aliases;
    for (auto &x : str2column) {
        auto &q = aliases[x.second];
        q.reserve(10);
        if (col2header[static_cast<unsigned>(x.second)] != x.first)
            q.emplace_back(x.first);
    }
    for (unsigned i = 0; i < sizeof col2header / sizeof col2header[0]; ++i) {
        if (i == static_cast<unsigned>(Column::HELP))
            continue;
        fprintf(o, "  ");
        fwrite(col2header[i].data(), 1, col2header[i].size(), o);
        fprintf(o, " - %s", col2help[i]);
        auto &v = aliases[Column(i)];
        if (!v.empty()) {
            if (v.size() == 1)
                fprintf(o, " (Alias: ");
            else
                fprintf(o, " (Aliases: ");
            auto a = v.begin();
            auto b = v.end();
            fprintf(o, "%s", (*a).c_str());
            ++a;
            for (; a != b; ++a)
                fprintf(o, ", %s", (*a).c_str());
            fputc(')', o);
        }
        fputc('\n', o);
    }
}

// Not using Boost Program Options because of its atrocious API
// and slow compile times.
// Not using cxxopts because of its API being similar to Boost PO.
// Not using CLI11 because of its ultra slow compile times.
// (cf. https://github.com/CLIUtils/CLI11/issues/194)
void Args::parse(int argc, char **argv)
{
    enum State { IN_PID_LIST, IN_COL_LIST };
    char c = 0;
    State state = IN_PID_LIST;
    // '-' prefix: no reordering of arguments, non-option arguments are
    // returned as argument to the 1 option
    // ':': preceding opting takes a mandatory argument
    while ((c = getopt(argc, argv, "-ae:c:Hhi:Kkoptu:")) != -1) {
        switch (c) {
            case '?':
                fprintf(stderr, "unexpected option character: %c\n", optopt);
                exit(1);
                break;
            case 'a':
                all_pids = true;
                break;
            case 'c':
                count = atoi(optarg);
                if (!interval_s)
                    interval_s = 1;
                break;
            case 'e':
                regex_str = optarg;
                break;
            case 'H':
                show_header = false;
                break;
            case 'h':
                help(stdout, argv[0]);
                exit(0);
                break;
            case 'i':
                interval_s = atoi(optarg);
                break;
            case 'K':
                show_tasks = Show_Tasks::USER;
                break;
            case 'k':
                show_tasks = Show_Tasks::KERNEL;
                break;
            case 'o':
                state = IN_COL_LIST;
                break;
            case 'p':
                state = IN_PID_LIST;
                break;
            case 't':
                traverse_threads = true;
                break;
            case 'u':
                all_pids = true;
                uid = parse_uid(optarg);
                break;
            case 1:
                switch (state) {
                    case IN_PID_LIST:
                        pids.push_back(atol(optarg));
                        break;
                    case IN_COL_LIST:
                        if (strlen(optarg) > 4 && !memcmp(optarg, "env:", 4)) {
                            columns.push_back(Column::ENV);
                            env_vars.emplace_back(optarg + 4);
                        } else {
                            try {
                                columns.push_back(str2column.at(string_view(optarg)));
                            } catch (const out_of_range &) {
                                fprintf(stderr, "Unknown column: %s\n", optarg);
                                exit(1);
                            }
                            env_vars.emplace_back();
                        }
                        if (columns.back() == Column::HELP) {
                            help_col(stdout);
                            exit(0);
                        }
                        if (columns.back() == Column::STIME) {
                            try {
                                boot_time_s = get_boot_time();
                                clock_ticks = ixxx::posix::sysconf(_SC_CLK_TCK);
                            } catch (...) {
                                fprintf(stderr, "Can't read /proc/uptime\n");
                                exit(1);
                            }
                        }
                        break;
                }
                break;
        }
    }
    if (pids.empty() && !all_pids) {
        if (regex_str.empty()) {
            fprintf(stderr, "Either specify one or more PIDs or -a or -e\n");
            exit(1);
        } else {
            all_pids = true;
        }
    }
    if (columns.empty()) {
        columns = { Column::PID, Column::TID, Column::PPID,
            Column::AFFINITY, Column::CPU, Column::CLS, Column::RTPRIO,
            Column::NICE, Column::SYSCALL, Column::RSS, Column::COMM };
        env_vars.resize(columns.size());
    }
}

struct Process;

typedef string_view (Process::*Process_Attr)();


struct Process {
    public:
        size_t                        pid         {0}          ;
        size_t                        tid         {0}          ;

        time_t                        boot_time_s              ;
        unsigned                      clock_ticks {0}          ;

    private:
        string                        fn          { "/proc/" } ;
        size_t                        fn_stem     {6}          ;
        char                          epsilon[1]  {0}          ;

        // we could also use std::vector, however we would need
        // to switch it from value to default initialization
        // to eliminate superfluous initializations
        // (such as in: https://github.com/gsauthof/libxfsx/blob/91979ec5f2bc56f3d0dd06ac0b8ff6658d889cfb/xfsx/raw_vector.hh#L7)
        // as a bonus we save some overheads in memory management
        array<char, 4*1024>           environ_arr              ;
        array<char, 4*1024>           stat_arr                 ;
        array<char, 4*1024>           status_arr               ;
        array<char, 4*1024>           misc_arr                 ;
        array<char, 4*1024>           io_arr                   ;
        string_view                   environ                  ;
        string_view                   stat                     ;
        string_view                   status                   ;
        string_view                   misc                     ;
        string_view                   io                       ;

        array<char, 1024>             buffer                   ;

        unordered_map<size_t, string> username_cache           ;

    public:
        Process() =default;
        Process(const Process &) =delete;
        Process &operator=(const Process &) =delete;

        void set_pid(size_t pid, size_t tid);
        const char *getenv(const string &s);

        unsigned flags();

        string_view comm();
        string_view exe();
        string_view wchan();
        string_view wchar();
        string_view wbyte();
        string_view cwbyte();
        string_view affinity();
        string_view syscall();
        string_view syscr();
        string_view syscw();
        string_view loginuid();
        string_view state();
        string_view cls();
        string_view cmd();
        string_view cpu();
        string_view cwd();
        string_view gid();
        string_view uid();
        string_view hugepages();
        string_view threads();
        string_view slack();
        string_view stack();
        string_view ppid();
        string_view rchar();
        string_view rbyte();
        string_view stime();
        string_view nvctx();
        string_view vctx();
        string_view minflt();
        string_view majflt();
        string_view umask();
        string_view user();
        string_view rss();
        string_view rtprio();
        string_view vsize();
        string_view fds();
        string_view fdsize();
        string_view numagid();
        string_view nice();
        string_view pflags();

        string_view column(Column c);

    private:
        template <size_t N>
        void read_proc(const char *q, array<char, N> &src,
                string_view &dst, bool prefix = false);
        string_view read_key_value(const string_view &status, const string_view &q);
        string_view read_status(const string_view &q);
        string_view read_io(const string_view &q);
        string_view read_stat(unsigned i);
        string_view read_link(const char *q);
};

Process_Attr process_attrs[] = {
    &Process::affinity  , // AFFINITY
    &Process::cls       , // CLS
    &Process::cmd       , // CMD
    &Process::comm      , // COMM
    &Process::cpu       , // CPU
    &Process::cwbyte    , // CWBYTE
    &Process::cwd       , // CWD
    nullptr             , // ENV
    &Process::exe       , // EXE
    &Process::fds       , // FDS
    &Process::fdsize    , // FDSIZE
    &Process::pflags    , // FLAGS
    &Process::gid       , // GID
    nullptr             , // HELP - dummy - never called
    &Process::hugepages , // HUGEPAGES
    &Process::loginuid  , // LOGINUID
    &Process::majflt    , // MAJFLT
    &Process::minflt    , // MINFLT
    &Process::nice      , // NICE
    &Process::numagid   , // NUMAGID
    &Process::nvctx     , // NVCTX
    nullptr             , // PID
    &Process::ppid      , // PPID
    &Process::rbyte     , // RBYTE
    &Process::rchar     , // RCHAR
    &Process::rss       , // RSS
    &Process::rtprio    , // RTPRIO
    &Process::slack     , // SLACK
    &Process::stack     , // STACK
    &Process::state     , // STATE
    &Process::stime     , // STIME
    &Process::syscall   , // SYSCALL
    &Process::syscr     , // SYSCR
    &Process::syscw     , // SYSCW
    &Process::threads   , // THREADS
    nullptr             , // TID
    &Process::uid       , // UID
    &Process::umask     , // UMASK
    &Process::user      , // USER
    &Process::vctx      , // VCTX
    &Process::vsize     , // VSIZE
    &Process::wbyte     , // WBYTE
    &Process::wchan     , // WCHAN
    &Process::wchar       // WCHAR
};

string_view Process::column(Column c)
{
    Process_Attr fn = process_attrs[static_cast<unsigned>(c)];
    return (this->*fn)();
}

void Process::set_pid(size_t pid, size_t tid)
{
    this->pid = pid;
    this->tid = tid;

    fn.resize(fn_stem);
    array<char, 20> buf;
    to_chars_result r = to_chars(buf.begin(), buf.end(), pid == tid ? pid : tid);
    fn.append(buf.begin(), r.ptr);
    fn.push_back('/');

    environ = string_view(environ_arr.begin(), 0);
    stat    = string_view(stat_arr.begin()   , 0);
    status  = string_view(status_arr.begin() , 0);
    io      = string_view(io_arr.begin()     , 0);
}

template <size_t N>
void Process::read_proc(const char *q, array<char, N> &src,
        string_view &dst, bool prefix)
{
    if (!dst.empty())
        return;

    size_t l = fn.size();
    fn.append(q);
    size_t n = 0;

    if (prefix) {
        ++n;
        src[0] = '\n';
    }
    try {
        ixxx::util::FD fd(fn, O_RDONLY);
        n += ixxx::util::read_all(fd, src.begin() + n, src.size() - n);
    } catch (...) {
        src[0] = ' ';
        n = 1;
    }

    dst = string_view(src.begin(), n);
    fn.resize(l);
}

string_view Process::read_key_value(const string_view &status, const string_view &q)
{
    auto p = search(status.begin(), status.end(),
            std::default_searcher(q.begin(), q.end()));

    if (p == status.end())
        return string_view();

    p += q.size();

    auto e = fast_find(p, status.end(), '\n');

    for ( ; p != e && (*p == ' ' || *p == '\t'); ++p)
        ;

    return string_view(&*p, e-p);
}

string_view Process::read_status(const string_view &q)
{
    read_proc("status", status_arr, status, true);
    return read_key_value(status, q);
}
string_view Process::read_io(const string_view &q)
{
    read_proc("io", io_arr, io, true);
    return read_key_value(io, q);
}


// NB: The stat fields are space delimited. However, the 2nd field (comm)
// is enclosed by parentheses because comm itself may contain spaces
// and parantheses. Since none of the following fields might contain a ')'
// it's sufficient to handle the 2nd field in a special way by
// scanning for the terminating ')' by searching from the right.
string_view Process::read_stat(unsigned k)
{
    read_proc("stat", stat_arr, stat);

    auto     p = stat.begin();
    unsigned i = 0;
    if (i < k) {
        p = fast_find(p, stat.end(), '(');
        if (p != stat.end())
            ++p;
        ++i;
        if (i < k) {
            p = fast_rfind(p, stat.end(), ')');
            if (p != stat.end())
                ++p;
            for (; i < k; ++i) {
                p = fast_find(p, stat.end(), ' ');
                if (p != stat.end())
                    ++p;
            }
        } else {
            auto e = fast_rfind(p, stat.end(), ')');
            return string_view(p, e-p);
        }
    }
    auto e = fast_find(p, stat.end(), ' ');
    return string_view(p, e-p);
}

const char *Process::getenv(const string &s)
{
    read_proc("environ", environ_arr, environ);

    auto p = search(environ.begin(), environ.end(),
                    std::default_searcher(s.begin(), s.end()));

    if (p == environ.end())
        return epsilon;

    auto m = p + s.size();
    if (m == environ.end() || *m != '=')
        return epsilon;

    ++m;
    return &*m;
}

string_view Process::read_link(const char *q)
{
    size_t l = fn.size();
    fn.append(q);

    size_t n = 0;
    try {
        n = ixxx::posix::readlink(fn, buffer);
    } catch (const ixxx::readlink_error &e) {
        // ignore
    }

    fn.resize(l);

    return string_view(buffer.data(), n);
}

// cf. https://elixir.bootlin.com/linux/v5.8.9/source/include/linux/sched.h#L1506
enum Process_Flags {
    PF_KTHREAD = 0x00200000
};

// cf. https://elixir.bootlin.com/linux/v5.8.9/source/include/linux/sched.h#L1483
static const string_view pf2str[] = {
    "0x0"               ,
    "PF_IDLE"           ,  // 0x00000002 /* I am an IDLE thread */
    "PF_EXITING"        ,  // 0x00000004 /* Getting shut down */
    "0x8"               ,
    "PF_VCPU"           ,  // 0x00000010 /* I'm a virtual CPU */
    "PF_WQ_WORKER"      ,  // 0x00000020 /* I'm a workqueue worker */
    "PF_FORKNOEXEC"     ,  // 0x00000040 /* Forked but didn't exec */
    "PF_MCE_PROCESS"    ,  // 0x00000080 /* Process policy on mce errors */
    "PF_SUPERPRIV"      ,  // 0x00000100 /* Used super-user privileges */
    "PF_DUMPCORE"       ,  // 0x00000200 /* Dumped core */
    "PF_SIGNALED"       ,  // 0x00000400 /* Killed by a signal */
    "PF_MEMALLOC"       ,  // 0x00000800 /* Allocating memory */
    "PF_NPROC_EXCEEDED" ,  // 0x00001000 /* set_user() noticed that RLIMIT_NPROC was exceeded */
    "PF_USED_MATH"      ,  // 0x00002000 /* If unset the fpu must be initialized before use */
    "PF_USED_ASYNC"     ,  // 0x00004000 /* Used async_schedule*(), used by module init */
    "PF_NOFREEZE"       ,  // 0x00008000 /* This thread should not be frozen */
    "PF_FROZEN"         ,  // 0x00010000 /* Frozen for system suspend */
    "PF_KSWAPD"         ,  // 0x00020000 /* I am kswapd */
    "PF_MEMALLOC_NOFS"  ,  // 0x00040000 /* All allocation requests will inherit GFP_NOFS */
    "PF_MEMALLOC_NOIO"  ,  // 0x00080000 /* All allocation requests will inherit GFP_NOIO */
    "PF_LOCAL_THROTTLE" ,  // 0x00100000 /* Throttle writes only against the bdi I write to, I am cleaning dirty pages from some other bdi. */
    "PF_KTHREAD"        ,  // 0x00200000 /* I am a kernel thread */
    "PF_RANDOMIZE"      ,  // 0x00400000 /* Randomize virtual address space */
    "PF_SWAPWRITE"      ,  // 0x00800000 /* Allowed to write to swap */
    "0x1000000"         ,
    "PF_UMH"            ,  // 0x02000000 /* I'm an Usermodehelper process */
    "PF_NO_SETAFFINITY" ,  // 0x04000000 /* Userland is not allowed to meddle with cpus_mask */
    "PF_MCE_EARLY"      ,  // 0x08000000 /* Early kill for mce process policy */
    "PF_MEMALLOC_NOCMA" ,  // 0x10000000 /* All allocation request will have _GFP_MOVABLE cleared */
    "PF_IO_WORKER"      ,  // 0x20000000 /* Task is an IO worker */
    "PF_FREEZER_SKIP"   ,  // 0x40000000 /* Freezer should not count it as freezable */
    "PF_SUSPEND_TASK"      // 0x80000000 /* This thread called freeze_processes() and should not be frozen */
};


unsigned Process::flags()
{
    auto x = read_stat(8);
    unsigned r = 0;
    from_chars(x.begin(), x.end(), r);
    return r;
}
string_view Process::pflags()
{
    unsigned f = flags();

    unsigned m = 1;

    char *p = misc_arr.data();
    for (unsigned i = 0; i < 32; ++i, m<<=1) {
        if (m & f) {
            if (p != misc_arr.data()) {
                *p++ = '|';
            }
            p = static_cast<char*>(mempcpy(p, pf2str[i].data(), pf2str[i].size()));
        }
    }
#if __cplusplus > 201703L
    misc = string_view(misc_arr.begin(), p);
#else
    misc = string_view(misc_arr.begin(), p - misc_arr.begin());
#endif
    return misc;
}
string_view Process::minflt()
{
    return read_stat(9);
}
string_view Process::majflt()
{
    return read_stat(11);
}
string_view Process::nice()
{
    return read_stat(18);
}
string_view Process::stime()
{
    auto x = read_stat(21);

    size_t a = 0;
    auto r = from_chars(x.begin(), x.end(), a);
    if (r.ptr != x.end())
        return string_view();

    a /= clock_ticks;
    time_t t = boot_time_s + a;

    struct tm l;
    ixxx::posix::localtime_r(&t, &l);

    size_t n = ixxx::ansi::strftime(buffer, "%F %H:%M:%S", &l);

    return string_view(buffer.data(), n);
}
string_view Process::cpu()
{
    return read_stat(38);
}
string_view Process::rtprio()
{
    return read_stat(39);
}
string_view Process::cls()
{
    auto x = read_stat(40);

    static const string_view clss[8] = {
        "OTH",
        "FIF",
        "RR",
        "BAT",
        "ISO",
        "IDL",
        "DED",
        "?"
    };

    unsigned char i = sizeof clss / sizeof clss[0] - 1;

    if (x.size() == 1) {
        unsigned char t = x[0];
        t -= static_cast<unsigned char>('0');
        if (t < i)
            i = t;
    }
    return clss[i];
}


string_view Process::exe()
{
    return read_link("exe");
}
string_view Process::cwd()
{
    return read_link("cwd");
}


string_view Process::wchan()
{
    misc = string_view(misc_arr.begin(), 0);
    read_proc("wchan", misc_arr, misc);
    return string_view(misc.data(), misc.size());
}
string_view Process::syscall()
{
    misc = string_view(misc_arr.begin(), 0);
    read_proc("syscall", misc_arr, misc);

    auto m = fast_find(misc.begin(), misc.end(), ' ');
    if (m == misc.end() || m == misc.begin())
        return string_view();
    unsigned no = 0;
    auto r = from_chars(&*misc.begin(), &*m, no);
    if (r.ptr != &*m)
        return string_view();

    return syscall2str_x86_64(no);
}
string_view Process::loginuid()
{
    misc = string_view(misc_arr.begin(), 0);
    read_proc("loginuid", misc_arr, misc);

    return string_view(misc.data(), misc.size());
}
string_view Process::slack()
{
    misc = string_view(misc_arr.begin(), 0);
    read_proc("timerslack_ns", misc_arr, misc);

    if (!misc.empty())
        misc.remove_suffix(1);

    return string_view(misc.data(), misc.size());
}
string_view Process::stack()
{
    misc = string_view(misc_arr.begin(), 0);
    read_proc("stack", misc_arr, misc);

    auto b = fast_find(misc.begin(), misc.end(), ' ');
    if (b != misc.end())
        ++b;
    auto e = fast_find(b, misc.end(), '+');

    return string_view(&*b, e-b);
}
string_view Process::cmd()
{
    misc = string_view(misc_arr.begin(), 0);
    read_proc("cmdline", misc_arr, misc);

    if (!misc.empty())
        misc = misc.substr(0, misc.size() - 1);

    // string_view is read-only ...
    replace(misc_arr.begin(), misc_arr.begin() + misc.size(), '\0', ' ');

    return string_view(&*misc.begin(), misc.size());
}
string_view Process::user()
{
    auto uv = uid();
    size_t u = 0;
    auto r = from_chars(uv.begin(), uv.end(), u);
    if (r.ptr != uv.end())
        return string_view();

    auto &name = username_cache[u];
    if (name.empty()) {
        struct passwd pass;
        struct passwd *res;
        ixxx::posix::getpwuid_r(uid_t(u), &pass, misc_arr.data(),
                misc_arr.size(), &res);
        if (!res)
            return string_view();
        name = pass.pw_name;
        // i.e. pw_name points into misc_arr
        return string_view(pass.pw_name);
    } else {
        return string_view(name);
    }
}


string_view Process::comm()
{
    return read_status("\nName:");
}
string_view Process::state()
{
    auto x = read_status("\nState:");

    auto a = fast_find(x.begin(), x.end(), '(');
    if (a != x.end())
        ++a;
    auto b = x.end();
    if (a != b && *(b-1) == ')')
        --b;
    return string_view(a, b-a);
}
string_view Process::gid()
{
    auto x = read_status("\nGid:");
    auto c = nth_col(x, 1); // effective gid
    return c;
}
string_view Process::uid()
{
    auto x = read_status("\nUid:");
    auto c = nth_col(x, 1); // effective uid
    return c;
}
string_view Process::hugepages()
{
    auto x = read_status("\nHugetlbPages:");
    auto c = nth_col(x, 0);
    return c;
}
string_view Process::threads()
{
    return read_status("\nThreads:");
}
string_view Process::ppid()
{
    return read_status("\nPPid:");
}
string_view Process::rchar()
{
    return read_io("\nrchar:");
}
string_view Process::rbyte()
{
    return read_io("\nread_bytes:");
}
string_view Process::wchar()
{
    return read_io("\nwchar:");
}
string_view Process::wbyte()
{
    return read_io("\nwrite_bytes:");
}
string_view Process::cwbyte()
{
    return read_io("\ncancelled_write_bytes:");
}
string_view Process::syscr()
{
    return read_io("\nsyscr:");
}
string_view Process::syscw()
{
    return read_io("\nsyscw:");
}
string_view Process::affinity()
{
    return read_status("\nCpus_allowed_list:");
}
string_view Process::nvctx()
{
    return read_status("\nnonvoluntary_ctxt_switches:");
}
string_view Process::vctx()
{
    return read_status("\nvoluntary_ctxt_switches:");
}
string_view Process::umask()
{
    return read_status("\nUmask:");
}
string_view Process::rss()
{
    auto r = read_status("\nVmRSS:");
    auto p = fast_find(r.begin(), r.end(), ' ');
#if __cplusplus > 201703L
    return string_view(r.begin(), p);
#else
    return string_view(r.begin(), p - r.begin());
#endif
}
string_view Process::vsize()
{
    auto r = read_status("\nVmSize:");
    auto p = fast_find(r.begin(), r.end(), ' ');
#if __cplusplus > 201703L
    return string_view(r.begin(), p);
#else
    return string_view(r.begin(), p - r.begin());
#endif
}
string_view Process::fdsize()
{
    return read_status("\nFDSize:");
}
string_view Process::numagid()
{
    return read_status("\nNgid:");
}

string_view Process::fds()
{
    size_t l = fn.size();
    fn.append("fd");

    size_t n = 0;
    try {
        ixxx::util::Directory fds{fn};
        fn.resize(l);
        for (const struct dirent *d = fds.read(); d; d = fds.read()) {
            if (*d->d_name == '.' && (!d->d_name[1] || (d->d_name[1] == '.' && !d->d_name[2])))
                continue;
            ++n;
        }
    } catch (const ixxx::opendir_error &e) {
        fn.resize(l);

        // ignore permission denied ...
        misc_arr[0] = '#';
        misc = string_view(misc_arr.begin(), 1);
        return misc;
    }


    auto r = to_chars(misc_arr.begin(), misc_arr.end(), n);
#if __cplusplus > 201703L
    misc   = string_view(misc_arr.begin(), r.ptr);
#else
    misc   = string_view(misc_arr.begin(), r.ptr - misc_arr.begin());
#endif
    return misc;
}

struct Proc_Traverser {
    virtual ~Proc_Traverser() = default;

    virtual size_t next() = 0;
    virtual void reset() = 0;
};


struct PID_Traverser : public Proc_Traverser {
    PID_Traverser(const vector<size_t> &pids);
    size_t next() override;
    void reset() override;

    const vector<size_t> &pids;
    vector<size_t>::const_iterator i;
};

PID_Traverser::PID_Traverser(const vector<size_t> &pids)
    : pids(pids),
    i(pids.begin())
{
}
size_t PID_Traverser::next()
{
    if (i == pids.end())
        return 0;
    return *i++;
}
void PID_Traverser::reset()
{
    i = pids.begin();
}


struct All_Traverser : public Proc_Traverser {
    size_t next() override;
    void reset() override;

    ixxx::util::Directory proc{"/proc"};
};
size_t All_Traverser::next()
{
    for (;;) {

        const struct dirent *d = proc.read();

        if (!d)
            return 0;

        if (d->d_type == DT_DIR && *d->d_name >= '0' && *d->d_name <= '9') {
            size_t pid = 0;
            auto e = d->d_name + strlen(d->d_name);
            auto r = from_chars(d->d_name, e, pid);
            if (r.ptr != e)
                return 0;
            return pid;
        }

    }
    return 0;
}
void All_Traverser::reset()
{
    rewinddir(proc);
}
struct Thread_Traverser {
    virtual ~Thread_Traverser() = default;

    virtual size_t next() = 0;
    virtual void set_pid(size_t pid) = 0;
};

struct Task_Traverser : public Thread_Traverser {
    Task_Traverser() =default;
    Task_Traverser(size_t pid);

    size_t next() override;
    void set_pid(size_t pid) override;

    private:
    size_t pid {0};
    ixxx::util::Directory proc;
};

Task_Traverser::Task_Traverser(size_t pid)
    : pid(pid)
{
    string s {"/proc/"};
    array<char, 20> buf;
    auto r = to_chars(buf.begin(), buf.end(), pid);
    s.append(buf.begin(), r.ptr);
    s.append("/task");
    try {
        proc = ixxx::util::Directory(s);
    } catch (...) {
        // create empty traverser then
    }
}
size_t Task_Traverser::next()
{
    for (;;) {
        auto d = proc.read();
        if (!d)
            return 0;
        if (*d->d_name < '0' || *d->d_name > '9')
            continue;
        size_t tid = 0;
        auto e = d->d_name + strlen(d->d_name);
        auto r = from_chars(d->d_name, e, tid);
        if (r.ptr != e)
            continue;
        if (pid != tid)
            return tid;
    }
    return 0;
}
void Task_Traverser::set_pid(size_t pid)
{
    *this = Task_Traverser(pid);
}

struct Single_Traverser : public Thread_Traverser {
    Single_Traverser() =default;
    Single_Traverser(size_t pid);

    size_t next() override;
    void set_pid(size_t pid) override;
    size_t pid {0};

    private:
};
Single_Traverser::Single_Traverser(size_t pid)
    : pid(pid)
{
}
size_t Single_Traverser::next()
{
    auto r = pid;
    pid = 0;
    return r;
}
void Single_Traverser::set_pid(size_t pid)
{
    this->pid = pid;
}


struct Waiter {
    Waiter(unsigned interval_s, unsigned count);
    void forward();
    bool done() const;
    uint64_t wait();

    private:
        ixxx::util::FD fd;
        unsigned count {0};
};
Waiter::Waiter(unsigned interval_s, unsigned count)
    : count(count)
{
    if (count)
        ++this->count;
    if (interval_s) {
        fd = ixxx::linux::timerfd_create(CLOCK_REALTIME, 0);
        struct itimerspec spec = {
            .it_interval = { .tv_sec = interval_s },
            .it_value    = { .tv_sec = interval_s }
        };
        ixxx::linux::timerfd_settime(fd, 0, &spec,  0);
    } else {
        this->count = 2;
    }
}
void Waiter::forward()
{
    if (count > 1)
        --count;
}
bool Waiter::done() const
{
    return count == 1;
}
uint64_t Waiter::wait()
{
    uint64_t v = 0;
    auto l = ixxx::posix::read(fd, &v, sizeof v);
    (void)l;
    assert(l == sizeof v);
    return v;
}


struct UID_Filter {

    UID_Filter(const optional<size_t> &uid);

    bool matches(size_t pid);

    private:
    ixxx::util::Directory proc{"/proc"};
    optional<size_t> uid;
    string base {"/proc/"};
    array<char, 1024> buf;

};
UID_Filter::UID_Filter(const optional<size_t> &uid)
    : uid(uid)
{
}
bool UID_Filter::matches(size_t pid)
{
    if (!uid)
        return true;
    base.resize(6);
    auto r = to_chars(buf.begin(), buf.end(), pid);
    base.append(buf.begin(), r.ptr);
    struct stat st;
    try {
        ixxx::posix::stat(base, &st);
    } catch (const ixxx::stat_error &) {
        // race condition of readdir vs. process termination ...
        // i.e. ENOENT
        return false;
    }
    return st.st_uid == *uid;
}

struct Regex_Filter {

    Regex_Filter(const string &expr);

    bool matches(size_t pid);

    private:
    ixxx::util::Directory proc{"/proc"};
    string base {"/proc/"};
    regex expr;
    bool empty { false };
    array<char, 1024> buf;

};
Regex_Filter::Regex_Filter(const string &expr)
    : expr(expr),
    empty(expr.empty())
{
}
bool Regex_Filter::matches(size_t pid)
{
    if (empty)
        return true;

    base.resize(6);
    auto r = to_chars(buf.begin(), buf.end(), pid);
    base.append(buf.begin(), r.ptr);
    base.append("/comm");

    try {
        ixxx::util::FD fd(base, O_RDONLY);
        size_t n = ixxx::util::read_all(fd, buf);
        bool b = regex_search(buf.data(), buf.data() + n, expr);
        return b;
    } catch (const ixxx::open_error &) {
        // race condition with process termination
        return false;
    }
}



static void lpad(unsigned l, const string_view &v, FILE *o)
{
    if (l >= v.size())
        l -= v.size();
    else
        l = 0;
    if (l)
        fprintf(o, "%*.*s", int(l), int(l), " ");
    fwrite(v.data(), 1, v.size(), o);
}

static void print_header(FILE *o, const Args &args)
{
    auto i = args.columns.begin();
    auto e = args.columns.end();
    if (i != e) {
        auto col = static_cast<unsigned>(*i);
        lpad(col2width[col], col2header[col], o);
    }
    ++i;
    for (; i != e; ++i) {
        auto col = static_cast<unsigned>(*i);
        fputc(' ', o);
        lpad(col2width[col], col2header[col], o);
    }
    fputc('\n', o);
}

static void print_column(FILE *o, Process &p, Column c, const string &env_var)
{
    auto l = col2width[static_cast<unsigned>(c)];
    switch (c) {
        case Column::PID:
            fprintf(o, "%*zu", int(l), p.pid);
            break;
        case Column::TID:
            fprintf(o, "%*zu", int(l), p.tid);
            break;
        case Column::ENV:
            fprintf(o, "%*s", int(l), p.getenv(env_var));
            break;
        default:
            {
                auto comm = p.column(c);
                if (comm.empty())
                    lpad(l, string_view("#"), o);
                else
                    lpad(l, comm, o);
            }
            break;
    }
}

static void print_row(FILE *o, Process &proc, const Args &args)
{
    unsigned i = 0;
    auto     b = args.columns.begin();
    auto     e = args.columns.end();

    if (b != e) {
        auto col = *b;
        print_column(o, proc, col, args.env_vars[i]);
        ++b;
        ++i;
    }
    for (; b != e; ++b, ++i) {
        fputc(' ', o);
        auto col = *b;
        print_column(o, proc, col, args.env_vars[i]);
    }
    fputc('\n', o);
}


int main(int argc, char **argv)
{
    Args args;
    try {
        args.parse(argc, argv);
    } catch (const std::exception &e) {
        fprintf(stderr, "Error parsing arguments: %s\n", e.what());
        return 1;
    }

    Process proc;
    proc.boot_time_s = args.boot_time_s;
    proc.clock_ticks = args.clock_ticks;


    UID_Filter   uid_filter(args.uid);
    Regex_Filter  re_filter(args.regex_str);

    unique_ptr<Proc_Traverser> trav;
    if (args.all_pids)
        trav.reset(new All_Traverser());
    else
        trav.reset(new PID_Traverser(args.pids));

    vector<unique_ptr<Thread_Traverser>> tid_travs;
    tid_travs.emplace_back(new Single_Traverser);
    if (args.traverse_threads)
        tid_travs.emplace_back(new Task_Traverser);

    if (args.show_header)
        print_header(stdout, args);

    Waiter w(args.interval_s, args.count);
    if (w.done())
        return 0;
    for (;;) {
        w.forward();
        while (auto pid = trav->next()) {

            if (!re_filter.matches(pid))
                continue;
            if (!uid_filter.matches(pid))
                continue;

            for (auto &tid_trav : tid_travs) {
                tid_trav->set_pid(pid);
                while (auto tid = tid_trav->next()) {

                    proc.set_pid(pid, tid);

                    unsigned flags = proc.flags();
                    if (args.show_tasks == Show_Tasks::KERNEL
                            && (flags & PF_KTHREAD) == 0)
                        continue;
                    if (args.show_tasks == Show_Tasks::USER && flags & PF_KTHREAD)
                        continue;

                    print_row(stdout, proc, args);
                }
            }
        }
        if (w.done())
            break;
        trav->reset();
        w.wait();
    }

    return 0;
}

