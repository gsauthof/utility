// 2020, Georg Sauthoff <mail@gms.tf>
//
// Display some clock related system settings.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>

#include <sys/timex.h>

int main(int argc, char **argv)
{
    struct timex t = {0};

    int r = adjtimex(&t);
    if (r == -1) {
        perror("adjtimex");
        return 1;
    }

    // this flag is removed by NTP/PTP daemons such as chronyd/ptp4l/sfptpd
    // (exception: Solarflare sfptpd versions before 3.6 (released 2023)
    //             don't remove this flag)
    // with STA_UNSYNC unset the kernel writes to the RTC every 11 minutes
    printf("Clock is %ssynchronized (%s)\n", t.status & STA_UNSYNC ? "un" : "",
            t.status & STA_UNSYNC ? "STA_UNSYNC" : "STA_UNSYNC unset");

    printf("Maxerror: %ld us\n", t.maxerror);

    // the offset the kernel uses for CLOCK_TAI, 
    // i.e. clock_gettime(CLOCK_TAI) == clock_gettime(CLOCK_REALTIME) + tai_off
    printf("TAI offset: %d s\n", t.tai);

    printf("PPS frequency discipline (STA_PPSFREQ): %s\n",
            t.status  & STA_PPSFREQ ? "enabled" : "disabled");
    printf("PPS time discipline (STA_PPSTIME): %s\n",
            t.status  & STA_PPSTIME ? "enabled" : "disabled");


    // if NTP/PTP daemon doesn't periodically update status and maxerror
    // the kernel 'decays' maxerror and if it's above a threshold it
    // sets the STA_UNSYNC bit;
    // this immediately sets the STA_UNSYNC bit
    if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'u') {
        t.status |= STA_UNSYNC;
        t.modes = ADJ_STATUS;
        r = adjtimex(&t);
        if (r == -1) {
            perror("adjtimex");
            return 1;
        }
    }

    return 0;
}
