#!/usr/bin/env python3

# SPDX-FileCopyrightText: © 2023 Georg Sauthoff <mail@gms.tf>
# SPDX-License-Identifier: GPL-3.0-or-later

import argparse
import asyncio
import configparser
import logging
import nio
import os
import sys


log = logging.getLogger(__name__)


def parse_args(*a):
    p = argparse.ArgumentParser(
            formatter_class=argparse.RawDescriptionHelpFormatter,
            description='Send a message via Matrix',
            epilog='''
Examples:

Send a direct message (to an existing private room):

    matrixto -u @juser:matrix.example.de -m "Host $(hostname) is back at it since $(date -Is)!"

Send a message to some room:

    fortune | matrixto --room '!deadcafe:matrix.example.org'

Send multiple messages:

    dstat -cdngy 60 10 | matrixto --line -u @juser:matrix.example.de


Initial Setup:

    mkdir ~/.config/matrixto
    cd !$
    cat > config.ini
    [user]
    server   = https://matrix.myhomeserver.example.net
    user     = @juser:myhandle.example.net
    password = sehr-geheim


2021, Georg Sauthoff <mail@gms.tf>, GPLv3+
''')

    p.add_argument('--message', '-m',
            help='message to send instead of reading it from stdin')
    p.add_argument('--room', '-r', help='room ID to message')
    p.add_argument('--user', '-u', help='user ID to message to (i.e. looks for a direct messaging room the user is part of')
    p.add_argument('--profile', '-p', default='user',
            help='profile name to lookup homeserver, password, etc. (default: %(default)s)')
    p.add_argument('--config', '-c', metavar='CONFIG_FILENAME', help='configuration file')
    p.add_argument('--state', dest='state_filename',
            help='state file where access token etc. is stored')
    p.add_argument('--no-state', action='store_false', default=True, dest='store_state',
            help="don't store access token etc. in state file after successful login")
    p.add_argument('--line', action='store_true', help='read and send stdin line by line')

    args = p.parse_args(*a)

    if not args.config:
        args.config = os.getenv('HOME') + '/.config/matrixto/config.ini'

    cp = configparser.ConfigParser()
    cp.read(args.config)
    if args.profile not in cp:
        raise RuntimeError(f"Couldn't find any profile '{args.profile}' in {args.config}")
    args.config = cp[args.profile]

    default_state_filename = os.getenv('HOME') + '/.config/matrixto/state.ini'
    if not args.state_filename:
        fn = default_state_filename
        if os.path.exists(fn):
            args.state_filename = fn
    if args.state_filename:
        cp = configparser.ConfigParser()
        cp.read(args.state_filename)
        if args.profile in cp:
            args.state = cp[args.profile]
        else:
            log.warning(f"Couldn't find any profile '{args.profile}' in {args.state_filename}")
            args.state = None
    else:
        args.state = None
        args.state_filename = default_state_filename

    return args


class Response_Error(RuntimeError):
    def __init__(self, msg, resp):
        super().__init__(msg)
        self.resp = resp

def raise_for_response(r, prefix=''):
    if r.transport_response.status != 200:
        raise Response_Error(f'{prefix}{r.message}', r)


def is_private_room(my_id, friend_id, joined_members):
    if len(joined_members) != 2:
        return False
    xs = [ a.user_id for a in joined_members ]
    if my_id not in xs:
        return False
    if friend_id not in xs:
        return False
    return True


async def private_room(client, friend_id):
    rs = await client.joined_rooms()
    raise_for_response(rs)
    for room in rs.rooms:
        ms = await client.joined_members(room)
        raise_for_response(ms)
        if is_private_room(client.user_id, friend_id, ms.members):
            return room
    return

async def send_message(client, room, msg):
    r = await client.room_send(room_id=room, message_type='m.room.message',
            content={ 'msgtype': 'm.text', 'body': msg })
    raise_for_response(r)

async def do_with_relogin(client, args, f, *xs, **kws):
    try:
        x = await f(*xs, **kws)
    except Response_Error as e:
        if e.resp.transport_response.status == 401:
            r = await client.login(args.config['password'])
            raise_for_response(r)
            if args.store_state:
                update_state(client, args.state_filename, args.profile)
            x = await f(*xs, **kws)
    return x

def update_state(client, filename, profile):
        cp = configparser.ConfigParser()
        if os.path.exists(filename):
            cp.read(filename)
        if profile not in cp:
            cp[profile] = {}
        h = cp[profile]
        h['access_token'] = client.access_token
        h['user_id']      = client.user_id
        h['device_id']    = client.device_id
        with open(filename, 'w') as f:
            cp.write(f)

async def run(args):
    profile = args.config
    client = nio.AsyncClient(profile['server'], profile['user'])
    if args.state:
        state = args.state
        client.access_token = state['access_token']
        client.user_id      = state['user_id']
        client.device_id    = state['device_id']
    else:
        r = await client.login(profile['password'])
        raise_for_response(r)
        if args.store_state:
            update_state(client, args.state_filename, args.profile)

    if args.user:
        room = await do_with_relogin(client, args, private_room, client, args.user)
        if not room:
            raise RuntimeError(f'Could not find private direct message room user {args.user}')
    else:
        room = args.room

    if args.line:
        return await send_line_by_line(client, args, room)

    if args.message:
        msg = args.message
    else:
        msg = sys.stdin.read()

    await do_with_relogin(client, args, send_message, client, room, msg)

    await client.close()


async def send_line_by_line(client, args, room):
    for line in sys.stdin:
        await do_with_relogin(client, args, send_message, client, room, line)

    await client.close()


def main():
    args = parse_args()
    logging.basicConfig(level=logging.WARN)
    asyncio.get_event_loop().run_until_complete(run(args))

if __name__ == '__main__':
    sys.exit(main())
