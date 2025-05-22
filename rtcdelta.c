// 2025, Georg Sauthoff <mail@gms.tf>
//
// Compute difference between RTC and system clock.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <fcntl.h>
#include <linux/rtc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>


static void help(const char *argv0)
{
    printf("call: %s [RTC_DEVICE]\n", argv0);
    exit(0);
}


int main(int argc, char **argv)
{
    const char *rtc = "/dev/rtc";
    if (argc > 1)
        rtc = argv[1];
    if (!strcmp(rtc, "-h") || !strcmp(rtc, "--help"))
        help(argv[0]);

    int fd = open(rtc, O_RDONLY);
    if (fd == -1) {
        perror("open rtc");
        return 1;
    }

    struct rtc_time rt = { 0 };
    int r = ioctl(fd, RTC_RD_TIME, &rt);
    if (r) {
        perror("RTC_RD_TIME");
        return 1;
    }
    struct tm rtm = {
        .tm_sec  = rt.tm_sec  ,
        .tm_min  = rt.tm_min  ,
        .tm_hour = rt.tm_hour ,
        .tm_mday = rt.tm_mday ,
        .tm_mon  = rt.tm_mon  ,
        .tm_year = rt.tm_year
    };
    r = setenv("TZ", "", 1);
    if (r == -1) {
        perror("setenv");
        return 1;
    }
    time_t rtc_epoch = mktime(&rtm);
    if (rtc_epoch == (time_t)-1) {
        perror("mktime");
        return 1;
    }

    struct timespec ts = {0};
    r = clock_gettime(CLOCK_REALTIME, &ts);
    if (r == -1) {
        perror("clock_gettime");
        return 1;
    }

    ssize_t delta = rtc_epoch;
    delta -= ts.tv_sec;

    printf("rtc-sys: %zd\n", delta);

    return 0;
}
