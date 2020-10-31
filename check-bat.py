#!/usr/bin/env python

# Monitor battery charging on an Android device and alarm user when
# the 80 % capacity mark is reached.
#
# Has to run inside termux environment, in a local session
# that should has the wake-lock aquired.
#
# SPDX-FileCopyrightText: © 2020 Georg Sauthoff <mail@gms.tf>
# SPDX-License-Identifier: GPL-3.0-or-later

import json
import logging
import subprocess
import sys
import time



log = logging.getLogger(__name__)


class Relative_Formatter(logging.Formatter):
    def format(self, rec):
        rec.rel_secs = rec.relativeCreated/1000.0
        return super(Relative_Formatter, self).format(rec)

def setup_logging():
    logging.basicConfig(level=logging.INFO, stream=sys.stdout)
    log_format      = '{rel_secs:6.1f} {message}'
    logging.getLogger().handlers[0].setFormatter(
            Relative_Formatter(log_format, style='{'))


def status():
    o = subprocess.check_output(['termux-battery-status'], universal_newlines=True)
    d = json.loads(o)
    return d


def alarm():
    p = subprocess.Popen(['termux-tts-speak'],
            stdin=subprocess.PIPE, universal_newlines=True, bufsize=1)
    for i in range(20):
        log.warning('Alarm!')
        p.stdin.write('Alarm!\n')
        #p.stdin.flush()
        time.sleep(1.2)
        if i % 10 == 0:
            d = status()
            if d['plugged'] == 'UNPLUGGED':
                break
    p.stdin.close()
    p.wait()


def main():
    setup_logging()
    i = 0
    while True:
        d = status()
        p = d['percentage']
        t = d['temperature']
        log.info(f'{p:3d} % - {t:6.2f} C')

        if p > 80:
            return alarm()

        if d['plugged'] == 'UNPLUGGED':
            i += 1
        else:
            i = 0
        if i > 1:
            break

        time.sleep(60)


if __name__ == '__main__':
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print()

