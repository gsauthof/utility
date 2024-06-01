#!/usr/bin/env python3



# 2016, Georg Sauthoff <mail@georg.so>, GPLv3+



import sys
import subprocess
import re
import datetime
import logging



logging.basicConfig()
log = logging.getLogger(__name__)



class Cert_Error(Exception):
    def __init__(self, msg, output):
        super(Cert_Error, self).__init__(msg)
        self.output = output



def check_not_expired(lines, now = datetime.datetime.now(datetime.UTC)):
    exp = re.compile("expires `([^']+) UTC'")
    thresh = now + datetime.timedelta(days=20)
    for l in lines:
        m = exp.search(l)
        if m:
            d = datetime.datetime.strptime(m.group(1), '%Y-%m-%d %H:%M:%S')
            if d <= now:
                # should not happen, as gnutls-cli should exit with code != 0
                raise ValueError('cert already expired')
            if thresh > d:
                raise ValueError('cert expires in less than 20 days')



def check_cert(host, port, proto=None):
    start = []
    if proto:
        start = [ '--starttls-proto', proto]
    s = subprocess.check_output(['gnutls-cli', host, '--port', port, '--ocsp'] + start,
                        stdin=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    t = s.decode('utf8')
    lines = t.splitlines()
    try:
        check_not_expired(lines)
    except ValueError as e:
        raise Cert_Error(str(e), t)



def check_certs(args):
    errors = 0
    for arg in args:
        v = arg.split('_')
        if v.__len__() < 2:
            raise LookupError('Not enough _ delimited components: {}' + arg)
        host = v[0]
        port = v[1]
        if v.__len__() > 2:
            proto = v[2]
        else:
            proto = None
        try:
            o = check_cert(host, port, proto)
        except (Cert_Error, subprocess.CalledProcessError) as e:
            o = e.output if type(e.output) is str else e.output.decode('utf8')
            log.error('{}:{} => {} - gnutls-cli output:\n{}'.format(host, port, str(e), o))
            errors = errors + 1
    return errors



def main(argv):
    if '-h' in argv or '--help' in argv:
        print('call: {} HOST1_PORT HOST2_PORT_STARTTLSPROTO ...'.format(argv[0]))
        return 0
    errors = check_certs(argv[1:])
    return int(errors>0)



if __name__ == '__main__':
    sys.exit(main(sys.argv))


