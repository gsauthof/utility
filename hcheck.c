// hcheck - health-check a command with a healthchecks.io instance
//
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: © 2023 Georg Sauthoff <mail@gms.tf>


#include <errno.h>               // ENOENT
#include <stdbool.h>
#include <stdio.h>               // fprintf(), ...
#include <stdlib.h>              // calloc(), ...
#include <string.h>              // strerror(), ...

#include <spawn.h>               // posix_spawnp()
#include <sys/wait.h>            // waitid()
#include <unistd.h>              // getopt()

#include <curl/curl.h>


extern char **environ;


static const int hc_error_code = 23;

struct Args {
    bool dry;
    const char *url;
    const char *uuid;
    int argc;
    char **argv;
};
typedef struct Args Args;
static const char default_url[] = "https://hc-ping.com";
static void help(FILE *o, const char *argv0)
{
    fprintf(o, "%s - healthcheck command\n"
            "Usage: %s [OPTS] COMMAND [COMMAND_OPTS]\n"
            "\n"
            "Options:\n"
            "  -d            dry run, i.e. just execute the child without healthchecks\n"
            "  -h            show help\n"
            "  -u UUID       UUID of the healthcheck\n"
            "                NB: use hcheck_uuid environment variable for hiding the UUID from other users\n"
            "  -l URL        Healthchecks instance URL (default: %s)\n"
            "\n"
            "In case of healthchecks transmission failures the command is still executed.\n"
            "Exit status is the exit status of the child command, unless:\n"
            "    - spawning failed, then it's 127 (not found) or 126 (other error)\n"
            "    - healthchecks communicateion error, then it's 23\n"
            "\n"
            "2023, Georg Sauthoff <mail@gms.tf>, GPLv3+\n"
            ,
            argv0,
            argv0,
            default_url);
}
static void parse_args(int argc, char **argv, Args *args)
{
    char     c     = 0;
    *args = (const Args){
        .url = default_url
    };
    // '+' prefix: no reordering of arguments
    // ':': preceding opting takes a mandatory argument
    while ((c = getopt(argc, argv, "+dhl:u:")) != -1) {
        switch (c) {
            case '?':
                fprintf(stderr, "unexpected option character: %c\n", optopt);
                exit(hc_error_code);
                break;
            case 'd':
                args->dry = 1;
                break;
            case 'h':
                help(stdout, argv[0]);
                exit(0);
                break;
            case 'l':
                args->url = optarg;
                break;
            case 'u':
                args->uuid = optarg;
                break;
        }
    }
    if (!args->uuid)
        args->uuid = getenv("hcheck_uuid");
    if (!args->uuid) {
        fprintf(stderr, "no healtlcheck uuid specified (cf. hcheck_uuid environment variable or -u option)\n");
        exit(hc_error_code);
    }

    if (optind >= argc) {
        fprintf(stderr, "positional arguments are missing\n");
        exit(hc_error_code);
    }
    args->argc = argc - optind;
    args->argv = argv + optind;
}


static int run(int argc, char **argv)
{
    if (!argc)
        return hc_error_code;
    pid_t pid = 0;
    int r = posix_spawnp(&pid, argv[0], 0, 0, argv, environ);
    if (r) {
        fprintf(stderr, "failed to execute %s: %s\n", argv[0], strerror(r));
        return r == ENOENT ? 127 : 126;
    }
    siginfo_t info;
    r = waitid(P_PID, pid, &info, WEXITED);
    if (r == -1) {
        perror("waitid failed");
        return hc_error_code;
    }
    return info.si_code == CLD_EXITED ? info.si_status : 128 + info.si_status;
}

static size_t write_ignore(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    (void)ptr;
    (void)userdata;
    return size * nmemb;
}
static size_t read_nothing(char *buffer, size_t size, size_t nitems, void *userdata)
{
    (void)buffer;
    (void)size;
    (void)nitems;
    (void)userdata;
    return 0;
}

static CURL *mk_curl_handle(char *curl_msg, int *rc)
{
    CURL *h = curl_easy_init();
    if (!h) {
        fprintf(stderr, "curl_easy_init failed\n");
        return h;
    }

    CURLcode r = curl_easy_setopt(h, CURLOPT_FAILONERROR, 1L);
    if (r) {
        fprintf(stderr, "CURLOPT_FAILONERROR failed: %s\n", curl_easy_strerror(r));
        *rc = hc_error_code;
    }
    r = curl_easy_setopt(h, CURLOPT_TIMEOUT_MS, 10000L);
    if (r) {
        fprintf(stderr, "CURLOPT_TIMEOUT_MS failed: %s\n", curl_easy_strerror(r));
        *rc = hc_error_code;
    }
    r = curl_easy_setopt(h, CURLOPT_USERAGENT, "hcheck-0.1/curl");
    if (r) {
        fprintf(stderr, "CURLOPT_USERAGENT failed: %s\n", curl_easy_strerror(r));
        *rc = hc_error_code;
    }
    r = curl_easy_setopt(h, CURLOPT_ERRORBUFFER, curl_msg);
    if (r) {
        fprintf(stderr, "CURLOPT_ERRORBUFFER failed: %s\n", curl_easy_strerror(r));
        *rc = hc_error_code;
    }
    r = curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, write_ignore);
    if (r) {
        fprintf(stderr, "CURLOPT_WRITEFUNCTION failed: %s\n", curl_easy_strerror(r));
        *rc = hc_error_code;
    }
    // with an HTTP GET request, libcurl shouldn't read anything, anyways,
    // but to be extra defensive here ...
    r = curl_easy_setopt(h, CURLOPT_READFUNCTION, read_nothing);
    if (r) {
        fprintf(stderr, "CURLOPT_READFUNCTION failed: %s\n", curl_easy_strerror(r));
        *rc = hc_error_code;
    }
    // tell curl to sned HTTP POST instead of the default GET requests;
    // healthchecks.io checks accept GET/HEAD/POST, by default,
    // but it's also possible to create POST-only checks
    r = curl_easy_setopt(h, CURLOPT_POSTFIELDS, "");
    if (r) {
        fprintf(stderr, "CURLOPT_POSTFIELDS failed: %s\n", curl_easy_strerror(r));
        *rc = hc_error_code;
    }

    return h;
}

static void transmit_start(CURL *h, const char *url, const char *uuid,
        char *curl_msg, int *rc)
{
    if (!h)
        return;

    char buf[1024] = {0};
    int r = snprintf(buf, sizeof buf, "%s/%s/start", url, uuid);
    if (r < 0) {
        perror("Constructing start URL failed");
        *rc = hc_error_code;
        return;
    }
    if ((size_t) r >= sizeof buf) {
        fprintf(stderr, "start URL truncated\n");
        *rc = hc_error_code;
        return;
    }
    CURLcode cc = curl_easy_setopt(h, CURLOPT_URL, buf);
    if (cc) {
        fprintf(stderr, "CURLOPT_URL failed: %s\n", curl_easy_strerror(r));
        *rc = hc_error_code;
        return;
    }
    cc = curl_easy_perform(h);
    if (cc) {
        fprintf(stderr, "curl perform failed: %s (%s)\n", curl_easy_strerror(cc), curl_msg);
        *curl_msg = 0;
        *rc = hc_error_code;
    }
}

static void transmit_exit(CURL *h, const char *url, const char *uuid, int code,
        char *curl_msg, int *rc)
{
    if (!h)
        return;

    char buf[1024] = {0};
    int r = snprintf(buf, sizeof buf, "%s/%s/%d", url, uuid, code);
    if (r < 0) {
        perror("Constructing exit URL failed");
        *rc = hc_error_code;
        return;
    }
    if ((size_t) r >= sizeof buf) {
        fprintf(stderr, "exit URL truncated\n");
        *rc = hc_error_code;
        return;
    }
    CURLcode cc = curl_easy_setopt(h, CURLOPT_URL, buf);
    if (cc) {
        fprintf(stderr, "CURLOPT_URL failed: %s\n", curl_easy_strerror(r));
        *rc = hc_error_code;
        return;
    }
    cc = curl_easy_perform(h);
    if (cc) {
        fprintf(stderr, "curl perform failed: %s (%s)\n", curl_easy_strerror(cc), curl_msg);
        *curl_msg = 0;
        *rc = hc_error_code;
    }
}


int main(int argc, char **argv)
{
    Args args;
    parse_args(argc, argv, &args);

    int rc = 0;

    char *curl_msg = calloc(CURL_ERROR_SIZE, 1);
    if (!curl_msg) {
        perror("calloc");
        return hc_error_code;
    }
    CURLcode cc = args.dry ? 0 : curl_global_init(CURL_GLOBAL_DEFAULT);
    if (cc) {
        fprintf(stderr, "curl_global_init() failed: %s\n", curl_easy_strerror(cc));
        rc = hc_error_code;
    }
    CURL *h = (args.dry || cc) ? 0 : mk_curl_handle(curl_msg, &rc);

    transmit_start(h, args.url, args.uuid, curl_msg, &rc);
    int r = run(args.argc, args.argv);
    transmit_exit(h, args.url, args.uuid, r, curl_msg, &rc);

    if (h) {
        curl_easy_cleanup(h);
        h = 0;
    }

    if (!args.dry && !cc)
        curl_global_cleanup();

    if (r)
        return r;
    return rc;
}
