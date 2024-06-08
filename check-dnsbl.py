#!/usr/bin/env python3


# 2016, Georg Sauthoff <mail@georg.so>, GPLv3+

import argparse
import csv
# require dnspython >= 1.15
# because of: https://github.com/rthalley/dnspython/issues/206
import dns.resolver
import dns.reversename
import itertools
import logging
import re
import sys
import time


ipv6_incapable = { 'bl.0spam.org', 'rbl.0spam.org', 'nbl.0spam.org' }


default_blacklists = [
        ('zen.spamhaus.org'             , 'Spamhaus SBL, XBL and PBL'         , 'https://www.spamhaus.org'        ),
        ('ix.dnsbl.manitu.net'          , 'Heise iX NiX Spam'                 , 'https://www.nixspam.net'         ),
        ('truncate.gbudb.net'           , 'Exclusively Spam/Malware'          , 'http://www.gbudb.com/truncate/'  ),
        # accepts de-listing payments, overblocking
        # https://en.wikipedia.org/w/index.php?title=Comparison_of_DNS_blacklists&oldid=1169665628#Suspect_RBL_providers
        ('dnsbl-1.uceprotect.net'       , 'Trapserver Cluster'                , 'https://www.uceprotect.net'      ),
        ('psbl.surriel.com'             , 'passive list, easy to unlist'      , 'https://psbl.org'                ),
        ('db.wpbl.info'                 , 'Weighted private'                  , 'http://wpbl.info'                ),
        ('bl.spamcop.net'               , 'Based on spamcop users'            , 'https://www.spamcop.net'         ),
        # spamrats doesn't serve DNS servers of hosting providers such as hetzner
        ('dyna.spamrats.com'            , 'Dynamic IP addresses'              , 'https://www.spamrats.com/'       ),
        ('spam.spamrats.com'            , 'Manual submissions'                , 'https://www.spamrats.com/'       ),
        ('auth.spamrats.com'            , 'Suspicious authentications'        , 'https://www.spamrats.com/'       ),
        ('bl.blocklist.de'              , 'fail2ban reports etc.'             , 'https://www.blocklist.de'        ),
        ('all.s5h.net'                  , 'traps'                             , 'https://www.usenix.org.uk/content/rbl.html'),
        ('b.barracudacentral.org'       , 'traps'                             , 'http://www.barracudacentral.org/rbl'),
        # also returns white-listed entries with return code 127.0.0.1
        ('hostkarma.junkemailfilter.com', 'Autotected Virus Senders'          , 'https://wiki.junkemailfilter.com'),
        ('bl.0spam.org'                 , 'Main 0spam DNSBL'                  , 'https://0spam.org'               ),
        ('rbl.0spam.org'                , 'Main 0spam realtime BL'            , 'https://0spam.org'               ),
        ('nbl.0spam.org'                , '0spam network BL'                  , 'https://0spam.org'               ),
        ('bl.nordspam.com'              , 'NordSpam IP addresses'             , 'https://www.nordspam.com/usage/' ),
        # doesn't list 127.0.0.2 RFC 5782 mandatory test entry
        ('combined.mail.abusix.zone'    , 'Abusix aggregated'                 , 'https://abu.sx/combined'         ),
        # seems to include dnsbl-1.uceprotect.net
        ('black.dnsbl.brukalai.lt'      , 'Brukalai.lt junk mail'             , 'https://www.brukalai.lt'         ),
        ('light.dnsbl.brukalai.lt'      , 'Brukalai.lt abuse'                 , 'https://www.brukalai.lt'         ),
        ('bip.virusfree.cz'             , 'botnet IP addresses'               , 'https://www.virusfree.cz/en/'    ),
        ('bad.virusfree.cz'             , 'high spam rate/malware senders'    , 'https://www.virusfree.cz/en/'    ),
        ('rbl.mailspike.org'            , 'whatever'                          , 'https://mailspike.org'           ),
        ('rbl.metunet.com'              , 'whatever'                          , 'https://rbl.metunet.com'         ),

        ('all.spam-rbl.fr'              , 'French DNSBL'                      , 'https://spam-rbl.fr/rbl.php'     ),
        ('bl.drmx.org'                  , 'traps and provider feedback'       , 'http://drmx.org/'                ),
        ('bl.spameatingmonkey.net'      , 'traps and policy'                  , 'https://spameatingmonkey.com/services'),
        ('backscatter.spameatingmonkey.net', 'backscatter to traps'           , 'https://spameatingmonkey.com/services'),
        ('bl.ipv6.spameatingmonkey.net', 'traps and policy for ipv6'          , 'https://spameatingmonkey.com/services'),
        ('netbl.spameatingmonkey.net',  'low reputation networks'             , 'https://spameatingmonkey.com/services'),
        ('bogons.cymru.com'             , 'unallocated netblocks'             , 'https://www.team-cymru.com/bogon-networks'),
        ('combined.rbl.msrbl.net'       , 'traps, virus scanners etc.'        , 'https://www.msrbl.com'           ),
        ('dnsbl.justspam.org'           , 'whatever'                          , 'http://www.justspam.org/usage'   ),
        ('dnsbl.madavi.de'              , 'website comment spam'              , 'https://www.madavi.de/madavibl/' ),
        ('dnsbl.rv-soft.info'           , 'whatever from czech repuplic'      , 'http://dnsbl.rv-soft.info'       ),
        ('dnsbl.zapbl.net'              , 'traps and admin choice'            , 'https://zapbl.net/using'         ),
        # doesn't list 127.0.0.2 RFC 5782 mandatory test entry
        ('dnsrbl.swinog.ch'             , 'traps'                             , 'https://antispam.imp.ch'         ),
        ('spamrbl.swinog.ch'            , 'IP addresses included in spam'     , 'https://antispam.imp.ch'         ),
        ('korea.services.net'           , 'korean networks minus whitelisted' , 'http://korea.services.net'       ),
        ('mail-abuse.blacklist.jippg.org', 'whatever from japan'              , 'http://blacklist.jippg.org'      ),
        ('rbl.blockedservers.com'       , 'whatever'                          , 'https://www.blockedservers.com'  ),
        ('spam.rbl.blockedservers.com'  , 'whatever'                          , 'https://www.blockedservers.com'  ),
        ('netscan.rbl.blockedservers.com', 'whatever'                         , 'https://www.blockedservers.com'  ),
        ('torexit.rbl.blockedservers.com', 'whatever'                         , 'https://www.blockedservers.com'  ),
        # doesn't list 127.0.0.2 RFC 5782 mandatory test entry
        ('pofon.foobar.hu'              , 'the fast-expiration spambot-network DNSBL', 'https://rbl.foobar.hu'    ),
        ('rbl.abuse.ro'                 , 'spamtraps'                         , 'https://abuse.ro'                ),
        ('pbl.abuse.ro'                 , 'spamtraps'                         , 'https://abuse.ro'                ),
        ('rbl.efnetrbl.org'             , 'BL for IRC servers'                , 'https://rbl.efnetrbl.org'        ),
        ('rbl.schulte.org'              , 'personal blocklist'                , 'https://rbl.schulte.org'         ),
        ('spam.dnsbl.anonmails.de'      , 'whatever'                          , 'https://anonmails.de/dnsbl.php'  ),
        ('spam.pedantic.org'            , 'personal blocklist'                , 'http://pedantic.org'             ),
        ('spamsources.fabel.dk'         , 'whatever'                          , 'https://www.spamsources.fabel.dk'),
        ('tor.dan.me.uk'                , 'tor nodes - exit and others'       , 'https://www.dan.me.uk/dnsbl'     ),
        ('torexit.dan.me.uk'            , 'tor exit nodes'                    , 'https://www.dan.me.uk/dnsbl'     ),

        ('bl.scientificspam.net'        , 'scientifc spam specific traps'     , 'https://www.scientificspam.net'  ),
        ('bl.suomispam.net'             , 'finish language spam'              , 'https://suomispam.net'           ),
        ('gl.suomispam.net'             , 'finish language spam - gray list'  , 'https://suomispam.net'           ),
        ('dnsblchile.org'               , 'whatever from chile'               , 'https://www.dnsblchile.org'      ),
        ('free.v4bl.org'                , 'whatever'                          , 'https://v4bl.org'                ),
        # Spanish academic and research network
        ('weak.dnsbl.rediris.es'        , 'traps and aggregation with other lists', 'https://www.rediris.es/irisrbl/'),
        ('strong.dnsbl.rediris.es'      , 'traps and aggregation with other lists', 'https://www.rediris.es/irisrbl/'),
        ('netblockbl.spamgrouper.to'    , 'bad networks'                      , 'http://www.spamgrouper.to'       ),
        ('rbl.lugh.ch'                  , 'traps and suspicious login attempts', 'https://lugh.ch/pages/dnsbl.html'),
        ('singular.ttk.pte.hu'          , 'traps, stats and user reports'     , 'https://singular.ttk.pte.hu/en/' ),

        ('bl.nosolicitado.org'          , 'whatever'                          , 'https://www.nosolicitado.org'    ),
        ('bl.worst.nosolicitado.org'    , 'whatever'                          , 'https://www.nosolicitado.org'    ),
        ('dnsbl.beetjevreemd.nl'        , 'personal curated blocklist'        , 'https://www.beetjevreemd.nl/dnsbl/'),
        ('dnsbl.dronebl.org'            , 'whatever'                          , 'https://dronebl.org'             ),
        ('ip4.bl.zenrbl.pl'             , 'some polish bl'                    , 'https://multirbl.valli.org/detail/ip4.bl.zenrbl.pl.html'),
        # offer paid list removal ($ 50)
        # https://www.mailcleaner.net/support/request-removal-from-mailcleaner-uribl/
        ('iprbl.mailcleaner.net'        , 'whatever'                          , 'https://multirbl.valli.org/detail/iprbl.mailcleaner.net.html'),
        ('rbl.interserver.net'          , 'whatever'                          , 'http://rbl.interserver.net/usage.php'),
        ('rbl.ircbl.org'                , 'IRC abuse blacklist'               , 'https://ircbl.org/about'         ),
        ('rbl.rbldns.ru'                , 'some russian blocklist'            , 'http://rbldns.ru/index.php/en/technical-recommendations.html'),
        ('reputation-ip.rbl.scrolloutf1.com', 'whatever'                      , 'http://www.scrolloutf1.com/rbl'  ),

        ('bl.fmb.la'                    , 'whatever'                          , 'https://fmb.la/pages/about'      ),
        ('bl.rbl.polspam.pl'            , 'some polish blacklist'             , 'https://polspam.pl/rbll.php'     ),
        ('bl6.rbl.polspam.pl'           , 'some polish blacklist'             , 'https://polspam.pl/rbll.php'     ),
        ('nsbl.ascams.com'              , 'whatever'                          , 'https://ascams.com/info/dnsbl-ascams-com/'),
        ('bl.octopusdns.com'            , 'whatever'                          , 'https://octopusrbl.monster/'     ),
        ('dnsbl.calivent.com.pe'        , 'some peruan blacklist'             , 'http://dnsbl.calivent.com.pe'    ),
        ('openproxy.bls.digibase.ca'    , 'whatever'                          , 'http://bls.digibase.ca/DNSBL'    ),
        ('proxyabuse.bls.digibase.ca'   , 'whatever'                          , 'http://bls.digibase.ca/DNSBL'    ),
        ('spambot.bls.digibase.ca'      , 'whatever'                          , 'http://bls.digibase.ca/DNSBL'    ),

        ('bl.mxrbl.com'                 , 'whatever'                          , 'https://mxrbl.com'               ),

        ]

# blacklists disabled by default because they return mostly garbage
garbage_blacklists = [
        # The spfbl.net operator doesn't publish clear criteria that lead to a
        # blacklisting.
        # When an IP address is blacklisted the operator can't name a specific
        # reason for the blacklisting. The blacklisting details page just names
        # overly generic reasons like:
        # 'This IP was flagged due to misconfiguration of the e-mail service or
        # the suspicion that there is no MTA at it.'
        # When contacting the operator's support, they can't back up such
        # claims.
        # There are additions of IP addresses to the spfbl.net blacklist that
        # have a properly configured MTA running and that aren't listed in any
        # other blacklist. Likely, those additions are caused by a bug in the
        # spfbl.net update process. But their support is uninterested in
        # improving that process. Instead they want to externalize maintenance
        # work by asking listed parties to waste some time on their manual
        # delisting process.
        # Suspiciously, you can even whitelist your listed address via
        # transferring $ 1.50 via PayPal. Go figure.
        # Thus, the value of querying this blacklist is utterly low as
        # you get false-positive results, very likely.
        ('dnsbl.spfbl.net'              , 'Reputation Database'              ),
        # offers paid delisting, violates RFC 5782 by listing 127.0.0.1 test entry
        # and NOT listing 127.0.0.2 ...
        # seems to be a fraudulent operation:
        # https://www.spamhaus.org/organization/statement/008/fraudulent-fake-dnsbl-uncovered-nszones.com
        ('bl.nszones.com'               , 'dynamic IP addresses etc.'        ),
        # references outdated contact information
        # DNSBL contains stale entries/false positives
        # doesn't list 127.0.0.2 RFC 5782 mandatory test entry
        ('vote.drbl.gremlin.ru'         , 'distributed blacklist'            ),
        # doesn't list 127.0.0.2 RFC 5782 mandatory test entry
        ('work.drbl.gremlin.ru'         , 'distributed blacklist aggregation'),
        ]


# See also:
# https://en.wikipedia.org/wiki/DNSBL
# https://tools.ietf.org/html/rfc5782
# https://en.wikipedia.org/wiki/Comparison_of_DNS_blacklists

# some lists provide detailed stats, i.e. the actual listed addresses
# useful for testing


default_domain_blacklists = [
        ('dbl.spamhaus.org', 'Spamhaus DBL', 'https://www.spamhaus.org/dbl/'),
        ('hostkarma.junkemailfilter.com', 'Hostkarma DBL', 'https://wiki.junkemailfilter.com/index.php/Spam_DNS_Lists#Name_Based_Lookups'),
        ('dbl.0spam.org'   , '0spam URLBL' , 'https://0spam.org'            ),
        ('dbl.nordspam.com', 'NordPM URIBL', 'https://www.nordspam.com/usage/'),
        ('dblack.mail.abusix.zone', 'Abusix URIBL', 'https://abu.sx/dblack' ),
        ('black.dnsbl.brukalai.lt', 'Brukalai also lists domains', 'https://www.brukalai.lt'),
        ('uribl.spameatingmonkey.net', 'Spameatingmonkey URIBL', 'https://spameatingmonkey.com/services'),
        ('urired.spameatingmonkey.net', 'Spameatingmonkey likely URIBL (includes uribl)', 'https://spameatingmonkey.com/services'),
        ('rhsbl.zapbl.net' , 'whatever', 'https://zapbl.net/using'),
        ('uribl.swinog.ch' , 'realtime URL blacklist', 'https://antispam.imp.ch'),
        ('uribl.pofon.foobar.hu', 'URIBL',   'https://rbl.foobar.hu'        ),
        ('uribl.abuse.ro'  , 'spamvertized domains', 'https://abuse.ro'     ),
        ('dbl.abuse.ro'    , 'spam sending domains', 'https://abuse.ro'     ),
        ('rhsbl.scientificspam.net', 'whatever', 'https://www.scientificspam.net'),
        ('dbl.suomispam.net', 'Finish DBL', 'https://suomispam.net'),
        # offer paid list removal ($ 50)
        # https://www.mailcleaner.net/support/request-removal-from-mailcleaner-uribl/
        ('uribl.mailcleaner.net', 'Pay for removal URIBL', 'https://multirbl.valli.org/detail/uribl.mailcleaner.net.html'),
        ('rbluri.interserver.net', 'whatever', 'http://rbl.interserver.net/usage.php'),
        ('bl-domain.rbl.scrolloutf1.com', 'whatever', 'http://www.scrolloutf1.com/rbl'),
        ('UnsafeSenders.rbl.scrolloutf1.com', 'whatever', 'http://www.scrolloutf1.com/rbl'),
        ('bl.fmb.la', 'BL also contains domains', 'https://fmb.la/pages/about'),
        ('short.fmb.la', 'URL shortener domain', 'https://fmb.la/pages/about'),
        ('rhsbl.rbl.polspam.pl', 'Polish DBL', 'https://polspam.pl/rbll.php'),
        ('rhsbl-h.rbl.polspam.pl', 'Polish URIBL', 'https://polspam.pl/rbll.php'),
        ('rhsbl-v.rbl.polspam.pl', 'Polish URIBL for suspicious URI', 'https://polspam.pl/rbll.php'),
        ('rhsbl-danger.rbl.polspam.pl', 'Polish DBL of dangerous domains', 'https://polspam.pl/rbll.php'),
        ]


log_format      = '%(asctime)s - %(levelname)-8s - %(message)s [%(name)s]'
log_date_format = '%Y-%m-%d %H:%M:%S'

## Simple Setup

# Note that the basicConfig() call is a NOP in Jupyter
# because Jupyter calls it before
logging.basicConfig(format=log_format, datefmt=log_date_format, level=logging.ERROR)
log = logging.getLogger(__name__)



def mk_arg_parser():
    p = argparse.ArgumentParser(
            formatter_class=argparse.RawDescriptionHelpFormatter,
            description = 'Check if mailservers are in any blacklist (DNSBL)',
            epilog='''Don't panic if a server is listed in some blacklist.
See also https://en.wikipedia.org/wiki/Comparison_of_DNS_blacklists for the
mechanics and policies of the different lists.

2016, Georg Sauthoff <mail@georg.so>, GPLv3+''')
    p.add_argument('dests', metavar='DESTINATION', nargs='*',
            help = 'servers, a MX lookup is done if it is a domain')
    p.add_argument('--bl', action='append', default=[],
            help='add another blacklist')
    p.add_argument('--bl-file', help='read more DNSBL from a CSV file')
    p.add_argument('--clear', action='store_true',
            help='clear default list of DNSBL')
    # https://blog.cloudflare.com/dns-resolver-1-1-1-1/
    p.add_argument('--cloudflare', action='store_true',
            help="use Cloudflare's public DNS nameservers")
    p.add_argument('--debug', action='store_true',
            help='print debug log messages')
    p.add_argument('--verbose', '-v', action='store_true',
                   help='print warnings')
    # cf. https://en.wikipedia.org/wiki/Google_Public_DNS
    p.add_argument('--google', action='store_true',
            help="use Google's public DNS nameservers")
    p.add_argument('--rev', action='store_true', default=True,
            help='check reverse DNS record for each domain (default: on)')
    p.add_argument('--mx', action='store_true', default=True,
            help='try to folow MX entries')
    p.add_argument('--address', action='store_true', default=True,
            help='check IP addresses against IP DNSBL (default: on)')
    p.add_argument('--domain', action='store_true', default=True,
            help='check domains also against DBLS (default: on)')
    p.add_argument('--no-mx', dest='mx', action='store_false',
            help='ignore any MX records')
    p.add_argument('--no-rev', action='store_false', dest='rev',
            help='disable reverse DNS checking')
    p.add_argument('--no-domain', action='store_false', dest='domain',
            help='disable DBL checking')
    p.add_argument('--no-address', action='store_false', dest='address',
            help='disable IP address blacklist checking')
    p.add_argument('--ns', action='append', default=[],
            help='use one or more alternate nameserverse')
    # cf. https://en.wikipedia.org/wiki/OpenDNS
    p.add_argument('--opendns', action='store_true',
            help="use Cisco's public DNS nameservers")
    # cf. https://quad9.net/faq/
    p.add_argument('--quad9', action='store_true',
            help="use Quad9's public DNS nameservers (i.e. the filtering ones)")
    p.add_argument('--retries', type=int, default=5,
            help='Number of retries if request times out (default: 5)')
    p.add_argument('--with-garbage', action='store_true',
            help=('also include low-quality blacklists that are maintained'
            ' by clueless operators and thus easily return false-positives'))
    p.add_argument('--check-lists', action='store_true',
            help='check lists for mandatory RFC 5782 test entries')
    return p



def parse_args(*a):
    p = mk_arg_parser()
    args = p.parse_args(*a)
    args.bls = default_blacklists
    args.dbls = default_domain_blacklists
    if args.clear:
        args.bls = []
    for bl in args.bl:
        args.bls.append((bl, ''))
    if args.bl_file:
        args.bls = args.bls + read_csv_bl(args.bl_file)
    if args.with_garbage:
        args.bls.extend(garbage_blacklists)
    if args.google:
        args.ns = args.ns + ['8.8.8.8', '2001:4860:4860::8888', '8.8.4.4', '2001:4860:4860::8844']
    if args.opendns:
        args.ns = args.ns + ['208.67.222.222', '2620:0:ccc::2', '208.67.220.220', '2620:0:ccd::2']
    if args.cloudflare:
        args.ns += ['1.1.1.1', '2606:4700:4700::1111', '1.0.0.1', '2606:4700:4700::1001']
    if args.quad9:
        args.ns += ['9.9.9.9', '2620:fe::fe', '149.112.112.112', '2620:fe::9']
    if args.ns:
        dns.resolver.default_resolver = dns.resolver.Resolver(configure=False)
        dns.resolver.default_resolver.nameservers = args.ns

    if args.debug:
        l = logging.getLogger() # root logger
        l.setLevel(logging.DEBUG)
    elif args.verbose:
        l = logging.getLogger() # root logger
        l.setLevel(logging.INFO)

    if not args.dests and not args.check_lists:
        raise RuntimeError('supply either destinations or --check-lists')
    return args



def read_csv_bl(filename):
    with open(filename, newline='') as f:
        reader = csv.reader(f)
        xs = [ row for row in reader
                if len(row) > 0 and not row[0].startswith('#') ]
        return xs



v4_ex = re.compile('^[.0-9]+$')
v6_ex = re.compile('^[:0-9a-fA-F]+$')

def get_addrs(dest, mx=True):
    if v4_ex.match(dest) or v6_ex.match(dest):
        return [ (dest, None) ], []
    domains = [ dest ]
    ds = [ dest ]
    if mx:
        try:
            r = dns.resolver.resolve(dest, 'mx', search=False)
            domains = [ answer.exchange for answer in r ]
            ds.extend(domains)
            log.debug('destinatin {} has MXs: {}'
                      .format(dest, ', '.join([str(d) for d in domains])))
        except dns.resolver.NoAnswer:
            pass
    addrs = []
    for domain in domains:
        for t in ['a', 'aaaa']:
            try:
                r = dns.resolver.resolve(domain, t, search=False)
            except dns.resolver.NoAnswer:
                continue
            xs = [ ( answer.address, domain ) for answer in r ]
            addrs = addrs + xs
            log.debug('domain {} has addresses: {}'
                      .format(domain, ', '.join([x[0] for x in xs])))
    if not addrs:
        raise ValueError("There isn't any a/aaaa DNS record for {}".format(domain))
    return addrs, ds


def check_dbl(domain, bl):
    t = str(domain)
    if t.endswith('.'):
        d = t + bl
    else:
        d = f'{t}.{bl}'
    try:
        v = dns.resolver.resolve(d, 'a',   search=False)
    except (dns.resolver.NXDOMAIN, dns.resolver.NoNameservers, dns.resolver.NoAnswer):
        return None
    try:
        w = dns.resolver.resolve(d, 'txt', search=False)
    except (dns.resolver.NXDOMAIN, dns.resolver.NoNameservers, dns.resolver.NoAnswer):
        w = []
    s = ','.join(f'{a} ({t})' for a, t in itertools.zip_longest(v, w, fillvalue=''))
    m = f'OMG, {domain} is listed in DBL {bl}: {s}'
    return m



def check_dnsbl(addr, bl):
    rev = dns.reversename.from_address(addr)
    domain = str(rev.split(3)[0]) + '.' + bl
    try:
        r = dns.resolver.resolve(domain, 'a', search=False)
    except (dns.resolver.NXDOMAIN, dns.resolver.NoNameservers, dns.resolver.NoAnswer):
        return None
    address = list(r)[0].address
    try:
        r = dns.resolver.resolve(domain, 'txt', search=False)
        txt = list(r)[0].to_text()
    except (dns.resolver.NoAnswer, dns.resolver.NXDOMAIN):
        txt = ''
    m = f'OMG, {addr} is listed in DNSBL {bl}: {address} ({txt})'
    return m


def check_rdns(addrs):
    errs = 0
    for (addr, domain) in addrs:
        if domain is None:
            continue
        log.debug('Check if there is a reverse DNS record that maps address {} to {}'
                  .format(addr, domain))
        try:
            r = dns.resolver.resolve(dns.reversename.from_address(addr), 'ptr', search=False)
            a = list(r)[0]
            target = str(a.target).lower()
            source = str(domain).lower()
            log.debug('Reverse DNS record for {} points to {}'.format(addr, target))
            if domain and source + '.' != target and source != target:
                log.error('domain {} resolves to {}, but the reverse record resolves to {}'.
                         format(domain, addr, target))
                errs = errs + 1
        except (dns.resolver.NXDOMAIN, dns.resolver.NoAnswer):
            log.error('There is no reverse DNS record for {}'.format(addr))
            errs = errs + 1
            return errs
    return errs



def check_dbls(domain, dbls, retries):
    d = domain
    ls = [ (bl[0], 0, 0) for bl in dbls ]
    errs = 0
    while ls:
        bl, i, ts = ls.pop(0)
        if i > 0:
            now = time.clock_gettime(time.CLOCK_REALTIME)
            t = max(i * 23 - max(now - ts, 0), 0)
            time.sleep(t)
        try:
            log.debug(f'Checking if domain {d} is listed in {bl} ...')
            s = check_dbl(d, bl)
            if s:
                errs += 1
                log.error(s)
        except dns.exception.Timeout as e:
            if i + 1 < retries:
                log.warning(f'Resolving {d} in {bl} timed out - retrying later ...')
                ls.append((bl, i+1, time.clock_gettime(time.CLOCK_REALTIME)))
            else:
                log.warning(f'Resolving {d} in {bl} timed out - giving up on it.')
    return errs


def check_bls(addrs, bls, dest, retries):
    ls = [ (x[0], x[1], y, 0, 0) for x in addrs for y in bls ]
    errs = 0
    while ls:
        addr, domain, bl, i, ts = ls.pop(0)
        if i > 0:
            now = time.clock_gettime(time.CLOCK_REALTIME)
            t = max(i * 23 - max(now - ts, 0), 0)
            time.sleep(t)
        try:
            if ':' in addr and bl[0] in ipv6_incapable:
                log.debug(f"Ignoring {bl[0]} because it doesn't support IPv6 ({addr})")
                continue
            log.debug(f'Checking if address {addr} (via {dest}) is listed in {bl[0]} ({bl[1]})')
            s = check_dnsbl(addr, bl[0])
            if s:
                errs += 1
                log.error(s)
        except dns.exception.Timeout as e:
            if i + 1 < retries:
                log.warning(f'Resolving {addr} in {bl[0]} timed out - retrying later ...')
                ls.append((addr, domain, bl, i+1, time.clock_gettime(time.CLOCK_REALTIME)))
            else:
                log.warning(f'Resolving {addr} in {bl[0]} timed out - giving up on it.')
    return errs


def check_domain_lists(dbls):
    errs = 0
    for d in dbls:
        bl = d[0]
        log.debug(f'Checking {bl}')
        s = check_dbl('test', bl)
        if s is None:
            log.error(f'OMG, mandatory "test" name is NOT listed in DBL {bl}')
            errs += 1
        s = check_dbl('invalid', bl)
        if s:
            log.error(s)
            errs += 1
    return errs

def check_addr_lists(bls):
    errs = 0
    for bl, blv, _ in bls:
        try:
            s = check_dnsbl('127.0.0.1', bl)
            if s:
                log.error(s)
                errs += 1
            s = check_dnsbl('127.0.0.2', bl)
            if not s:
                log.error(f'OMG, mandatory 127.0.0.2 is NOT listed in DNSBL {bl}')
                errs += 1
        except dns.exception.Timeout as e:
            log.error(f'Resolving mandatory entries timed out on {bl}')
    return errs


def run(args):
    if args.check_lists:
        return (  (check_addr_lists  (args.bls ) if args.address else 0)
                + (check_domain_lists(args.dbls) if args.domain  else 0) != 0 )

    errs = 0
    if args.address:
        log.debug(f'Checking {len(args.bls)} DNS blacklists')
    if args.domain:
        log.debug(f'Checking {len(args.dbls)} domain based DNS blacklist')

    for dest in args.dests:
        addrs, domains = get_addrs(dest, mx=args.mx)
        if args.address:
            if args.rev:
                errs = errs + check_rdns(addrs)

            e = check_bls(addrs, args.bls, dest, args.retries)
            if e:
                log.error(f'{dest} is listed in {e} blacklists')
            errs += e

        if args.domain:
            for d in domains:
                e = check_dbls(d, args.dbls, args.retries)
                if e:
                    log.error(f'{d} is listed {e} blacklists')
                errs += e

    return errs != 0



def main(*a):
    args = parse_args(*a)
    return run(args)



if __name__ == '__main__':
    if 'IPython' in sys.modules:
        # do something different when running inside a Jupyter notebook
        pass
    else:
        try:
            sys.exit(main())
        except Exception as e:
            log.error(e)
            sys.exit(1)




