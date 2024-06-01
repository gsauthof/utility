#!/usr/bin/env python3

# coding: utf-8

# In[ ]:

# 2016, Georg Sauthoff <mail@georg.so>, GPLv3+


# In[ ]:

import sys
import subprocess
import re
import datetime
import logging


# In[ ]:

logging.basicConfig()
log = logging.getLogger(__name__)


# In[ ]:

class Cert_Error(Exception):
    def __init__(self, msg, output):
        super(Cert_Error, self).__init__(msg)
        self.output = output


# In[ ]:

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


# In[ ]:

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


# In[ ]:

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


# In[ ]:

def main(argv):
    if '-h' in argv or '--help' in argv:
        print('call: {} HOST1_PORT HOST2_PORT_STARTTLSPROTO ...'.format(argv[0]))
        return 0
    errors = check_certs(argv[1:])
    return int(errors>0)


# In[ ]:

if __name__ == '__main__':
    if 'IPython' in sys.modules:
        pass
    else:
        sys.exit(main(sys.argv))


# In[ ]:

# Start of Jupyter notebook scratch area
#
# export script via:
#
# { echo '#!/usr/bin/env python3'; ipython3 nbconvert --to script check-cert --stdout | sed '/^####/,$s/^/# #/'  } > check-cert.py && chmod 755 check-cert.py
#
# ##### everything below this mark is commented out on export
# #
# #
# ## In[ ]:
# #
# #sorted([ x for x in sys.modules.keys() if x[0] == 'i' or x[0] == 'I' ])
# #
# #
# ## In[ ]:
# #
# #'IPython' in sys.modules
# #
# #
# ## In[ ]:
# #
# #check_certs(['georg.so_25_smtp'])
# #
# #
# ## In[ ]:
# #
# ## as of 2016-10-30 fails because of CACert CA not being available or cert being expired
# #check_certs(['laforge.gnumonks.org_443'])
# #
# #
# ## In[ ]:
# #
# #check_certs(['georg.so_25_smtp', 'georg.so_993', 'escher.lru.li_993', 'gms.tf_443'])
# #
# #
# ## In[ ]:
# #
# #check_not_expired(lines, now = datetime.datetime(2017, 1, 26, 20, 10, 10))
# #
# #
# ## In[ ]:
# #
# #s = subprocess.check_output(['gnutls-cli', 'georg.so', '--port', '443', '--ocsp'],
# #                        stdin=subprocess.DEVNULL)
# #
# #
# ## In[ ]:
# #
# #lines = s.decode('utf8').splitlines()
# #
# #
# ## In[ ]:
# #
# #print(s)
# #
# #
# ## In[ ]:
# #
# #log.error('foo')
# #
