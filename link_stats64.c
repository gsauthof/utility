// link_stats64 - dump the kernel's rtnl_link_stats64 structs
//
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: © 2023 Georg Sauthoff <mail@gms.tf>


#include <asm/types.h>           // netlink integer types
#include <errno.h>
#include <inttypes.h>
#include <linux/netlink.h>       // NETLINK_ROUTE, nlmsghdr, ...
#include <linux/rtnetlink.h>     // RTM_*, rtnl_link_stats64, ...
#include <net/if.h>              // if_indextoname()
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <unistd.h>              // getopt()


struct Args {
    unsigned period_s;
    unsigned count;
    const char *ofilename;
    bool dump_all;
    bool csv;
    bool flush;
};
typedef struct Args Args;
static void help(FILE *o, const char *argv0)
{
    fprintf(o, "%s - dump the kernel's rtnl_link_stats64 structs\n"
            "Usage: %s [OPTS] [DELAY_S] [COUNT]\n"
            "\n"
            "Options:\n"
            "  -a            also dump counters that are zero\n"
            "  -c            dump as CSV\n"
            "  -f            flush stdio\n"
            "  -o FILENAME   write to file instead of stdout\n"
            "\n"
            "2023, Georg Sauthoff <mail@gms.tf>, GPLv3+\n"
            ,
            argv0,
            argv0);
}
static void parse_args(int argc, char **argv, Args *args)
{
    unsigned state = 0;
    char     c     = 0;
    *args = (const Args){
        .period_s = 1,
        .count = 1
    };
    // '-' prefix: no reordering of arguments, non-option arguments are
    // returned as argument to the 1 option
    // ':': preceding opting takes a mandatory argument
    while ((c = getopt(argc, argv, "-acfho:")) != -1) {
        switch (c) {
            case '?':
                fprintf(stderr, "unexpected option character: %c\n", optopt);
                exit(1);
                break;
            case 1:
                switch (state) {
                    case 0:
                        args->period_s = atoi(optarg);
                        args->count    = 0;
                        ++state;
                        break;
                    case 1:
                        args->count    = atoi(optarg);
                        ++state;
                        break;
                    case 2:
                        fprintf(stderr, "Too many positional arguments\n");
                        exit(1);
                        break;
                }
                break;
            case 'a':
                args->dump_all = true;
                break;
            case 'c':
                args->csv = true;
                break;
            case 'f':
                args->flush = true;
                break;
            case 'h':
                help(stdout, argv[0]);
                exit(0);
                break;
            case 'o':
                args->ofilename = optarg;
                break;
            default:
                fprintf(stderr, "Sorry, option -%c isn't implemented, yet ...\n",
                        optopt);
                exit(1);
                break;
        }
    }
}


static volatile sig_atomic_t globally_interrupted;

static void int_handler(int)
{
    globally_interrupted = 1;
}

static volatile sig_atomic_t globally_huped;

static void hup_handler(int)
{
    globally_huped = 1;
}


static const char *nlmsg_type_str(uint16_t t)
{
    switch (t) {
        case RTM_NEWSTATS:
            return "RTM_NEWSTATS";
            break;
        case NLMSG_DONE:
            return "NLMSG_DONE";
            break;
        default:
            return "unk";
            break;
    }
}
static const char *nlmsg_flags_str(uint16_t f)
{
    switch (f) {
        case NLM_F_MULTI:
            return "NLM_F_MULTI";
            break;
        default:
            return "unk";
            break;
    }
}

static void pp_link_stats64(FILE *o, const struct rtnl_link_stats64 *s,
        const char *name, bool dump_all, time_t epoch)
{
    if (epoch)
        fprintf(o, "epoch: %zu\n", (size_t) epoch);
    #include "pp_link_stats64.c"
}

#include "csv_link_stats64.c"


// Expected payload:
//
//     struct nlmsghdr | if_stats_msg | nlattr | rtnl_link_stats64
//
// or just
//
//     struct nlmsghdr
//
// which terminates a multipart message
static int dump_stats(FILE *o, const unsigned char *b, size_t n, bool dump_all,
        bool csv,
        time_t epoch)
{
    const unsigned char *e = b + n;
    const unsigned char *p = b;

    while (p < e) {
        if (p +  sizeof(struct nlmsghdr) > e) {
            fprintf(stderr, "nlmsghdr truncated\n");
            return -1;
        }
        // This doesn't violate strict-aliasing rules
        // because of C's effective type rules
        // (cf. https://en.cppreference.com/w/c/language/object#Effective_type).
        // The same reasoning applies to the following struct casts in this
        // function.
        // Also, receive buffer reuse isn't an issue because the placement
        // of nlmsghdr/if_stats_msg/nlattr/rtnl_link_stats64 in this function
        // doesn't conflict with the placement in a previous call.
        struct nlmsghdr *h = (struct nlmsghdr*) p;
        p += sizeof *h;
        if (h->nlmsg_flags != NLM_F_MULTI) {
            fprintf(stderr, "Unexpected nlmsghdr::nlmsg_flags: %s (%" PRIu16 ")\n",
                   nlmsg_flags_str(h->nlmsg_flags), h->nlmsg_flags);
            return -1;
        }

        switch (h->nlmsg_type) {
            case RTM_NEWSTATS:
                if (p + sizeof(struct if_stats_msg) > e) {
                    fprintf(stderr, "if_stats_msg truncated\n");
                    return -1;
                }
                struct if_stats_msg *m = (struct if_stats_msg*) p;
                p += sizeof *m;
                char name[IF_NAMESIZE] = {0};
                if_indextoname(m->ifindex, name);

                if (p + sizeof(struct nlattr) > e) {
                    fprintf(stderr, "nlattr truncated\n");
                    return -1;
                }
                struct nlattr *a = (struct nlattr*) p;
                p += sizeof *a;
                if (a->nla_type != IFLA_STATS_LINK_64) {
                    fprintf(stderr, "unexpected nlattr::nla_type\n");
                    return -1;
                }
                if (a->nla_len != sizeof *a + sizeof(struct rtnl_link_stats64)) {
                    fprintf(stderr, "unexpected struct rtnl_link_stats64 size\n");
                    return -1;
                }
                if (p + sizeof(struct rtnl_link_stats64) > e) {
                    fprintf(stderr, "rtnl_link_stats64 truncated\n");
                    return -1;
                }
                struct rtnl_link_stats64 *s = (struct rtnl_link_stats64*) p;
                p += sizeof *s;
                if (csv)
                    pp_csv_row(o, epoch, name, s);
                else
                    pp_link_stats64(o, s, name, dump_all, epoch);
                break;
            case NLMSG_DONE:
                return 0;
            default:
                fprintf(stderr, "Unexpected nlmsghdr::nlmsg_type: %s\n",
                        nlmsg_type_str(h->nlmsg_type));
                return -1;
        }
    }
    return 1;
}

static int connect_netlink(struct sockaddr_nl *sa)
{
    // with AF_NETLINK, SOCK_RAW is equivalent to SOCK_DGRAM
    int fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
    if (fd == -1) {
        perror("socket");
        return -1;
    }
    int x = 32768;
    int r = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &x, sizeof x);
    if (r == -1) {
        perror("setsockopt SO_SNDBUF");
        return -1;
    }
    x = 1048576;
    r = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &x, sizeof x);
    if (r == -1) {
        perror("setsockopt SO_RCVBUF");
        return -1;
    }
    // we don't really need extended acks, or do we?
    /*
    x = 1;
    r = setsockopt(fd, SOL_NETLINK, NETLINK_EXT_ACK, &x, sizeof x);
    if (r == -1) {
        perror("setsockopt NETLINK_EXT_ACK");
        return -1;
    }
    */
    *sa = (struct sockaddr_nl) {
        .nl_family = AF_NETLINK,
        .nl_pid    = 0,
        .nl_groups = 0
    };
    r = bind(fd, (struct sockaddr*) sa, sizeof *sa);
    if (r == -1) {
        perror("bind");
        return -1;
    }
    return fd;
}

static int mk_timer(unsigned period_s)
{
    int tfd = timerfd_create(CLOCK_REALTIME, TFD_CLOEXEC);
    if (tfd == -1) {
        perror("timerfd_create");
        return -1;
    }
    unsigned x;
    ssize_t l = getrandom(&x, sizeof x, 0);
    if (l == -1) {
        perror("getrandom");
        return -1;
    }
    x = x % 1000000000l;
    struct itimerspec spec = {
        .it_interval = {
            .tv_sec = period_s
        },
        .it_value = {
            // start at a random sub-second offset to avoid accidental
            // clustering of such monitoring jobs
            .tv_nsec = x
        }
    };
    int r = timerfd_settime(tfd, 0, &spec, 0);
    if (r == -1) {
        perror("timerfd_settime");
        return -1;
    }
    return tfd;
}


static int wait_for_period(int tfd)
{
    for (;;) {
        uint64_t x;
        ssize_t r = read(tfd, &x, sizeof x);
        if (r == -1) {
            if (errno == EINTR) {
                if (globally_interrupted)
                    return 1;
                else
                    continue;
            }
            perror("read timerfd");
            return -1;
        }
        if (r != sizeof x) {
            fprintf(stderr, "short timer expirations read\n");
            return -1;
        }
        break;
    }
    return 0;
}

static int send_rtm_getstats(int fd)
{
    struct {
        struct nlmsghdr h;
        struct if_stats_msg p;
    } __attribute__((packed)) msg = {
        .h = {
            .nlmsg_len   = sizeof msg,
            .nlmsg_type  = RTM_GETSTATS,
            .nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP,
            .nlmsg_seq   = 0, // optional
            .nlmsg_pid   = 0  // always zero when talking to the kernel
        },
        .p = {
            .family      = AF_UNSPEC,
            .ifindex     = 0,
            .filter_mask = IFLA_STATS_FILTER_BIT(IFLA_STATS_LINK_64)
        }
    };
    ssize_t l = sendto(fd, &msg, sizeof msg, 0, 0, 0);
    if (l == -1) {
        perror("sendto");
        return -1;
    }
    if (l != sizeof msg) {
        fprintf(stderr, "sendto: transmitted less than expected");
        return -1;
    }
    return 0;
}

static ssize_t recv_stats(int fd, struct sockaddr_nl *sa,
        unsigned char **buf, size_t *n)
{
    struct iovec iov = { 0 };
    struct msghdr mh = {
        .msg_name = sa,
        .msg_namelen = sizeof *sa,
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_flags = MSG_TRUNC
    };
    ssize_t l = recvmsg(fd, &mh, MSG_PEEK | MSG_TRUNC);
    if (l == -1) {
        perror("recvmsg peek");
        return -1;
    }
    if ((size_t)l > *n) {
        *buf = realloc(*buf, l);
        if (!*buf) {
            perror("realloc");
            return -1;
        }
        memset(*buf + *n, 0, l - *n);
        *n = l;
    }
    iov.iov_base = *buf;
    iov.iov_len  = *n;
    mh.msg_flags = 0;
    ssize_t old_l = l;
    l = recvmsg(fd, &mh, 0);
    if (l == -1) {
        perror("recvmsg");
        return -1;
    }
    if (l != old_l) {
        fprintf(stderr, "received less than peeked\n");
        return -1;
    }
    return l;
}

static int dump(const Args *args, int fd, struct sockaddr_nl *sa,
        unsigned char **buf, size_t *n,
        FILE *o)
{
    int r = send_rtm_getstats(fd);
    if (r == -1)
        return -1;

    struct timespec ts;
    for (;;) {
        r = clock_gettime(CLOCK_REALTIME, &ts);
        if (r == -1) {
            perror("clock_gettime");
            return -1;
        }

        ssize_t l = recv_stats(fd, sa, buf, n);
        if (l == -1)
            return -1;

        r = dump_stats(o, *buf, l, args->dump_all, args->csv,
                args->count == 1 ? 0 : ts.tv_sec);
        if (args->flush)
            fflush(o);
        switch (r) {
            case -1: return -1;
            case  0: return  0;
        }
    }

    return 0;
}


static FILE *open_output(const char *filename)
{
    if (!filename || (filename[0] == '-' && !filename[1]))
        return stdout;
    FILE *o = fopen(filename, "w");
    if (!o)
        perror("fopen");
    return o;
}

static FILE *logrotate(FILE *o, const char *filename)
{
    if (o == stdout)
        return stdout;

    if (fclose(o) == EOF) {
        perror("fclose for logrotate");
        return 0;
    }
    return  open_output(filename);
}


static int setup_signal_handler()
{
    {
        struct sigaction int_action = {
            .sa_handler = int_handler
        };
        int r = sigaction(SIGINT, &int_action, 0);
        if (r == -1) {
            perror("sigaction SIGINT");
            return -1;
        }
    }
    {
        struct sigaction hup_action = {
            .sa_handler = hup_handler
        };
        int r = sigaction(SIGHUP, &hup_action, 0);
        if (r == -1) {
            perror("sigaction SIGHUP");
            return -1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    Args args;
    parse_args(argc, argv, &args);

    struct sockaddr_nl sa;
    int fd = connect_netlink(&sa);
    if (fd == -1)
        return 1;

    int tfd = -1;
    if (args.count != 1) {
        tfd = mk_timer(args.period_s);
        if (tfd == -1)
            return 1;
    }

    size_t n = 32 * 1024;
    unsigned char *buf = calloc(n, 1);
    if (!buf) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    FILE *o = stdout;
    if (!(o = open_output(args.ofilename)))
        return 1;

    if (setup_signal_handler() == -1)
        return 1;

    if (args.csv)
        pp_csv_header(o);

    for (unsigned i = 0;
            (i < args.count || !args.count) && !globally_interrupted; ++i) {
        if (args.count != 1) {
            switch (wait_for_period(tfd)) {
                case -1: return 1;
                case  1: return 0;
            }
        }
        int r = dump(&args, fd, &sa, &buf, &n, o);
        if (r == -1)
            return 1;

        if (globally_huped) {
            o = logrotate(o, args.ofilename);
            if (args.csv)
                pp_csv_header(o);
            globally_huped = 0;
        }
    }

    return 0;
}
