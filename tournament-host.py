#!/usr/bin/env python3

import argparse
import asyncio
import csv
import itertools
import os
import pwd
import re
import sys
import time


HEX_SERVER_PROGRAM = os.path.abspath('hex-server')

HEX_AGENT_USERS = [
    ent.pw_name for ent in pwd.getpwall() if re.match('hex-agent-\d+$', ent.pw_name)
]

HEX_AGENT_UIDS = [
    str(pwd.getpwnam(user).pw_uid) for user in HEX_AGENT_USERS
]


arg_parser = argparse.ArgumentParser(prog='tournament-host',
                                     description='Helper script for hosting Hex tournaments.')

arg_parser.add_argument('schedule_file',
                        type=argparse.FileType('r'),
                        help='schedule file of agent-pairs to define the tournament schedule')
arg_parser.add_argument('output_file',
                        type=argparse.FileType('w'),
                        help='output file to write final tournament statistics to')
arg_parser.add_argument('--concurrent-matches',
                        default=1,
                        choices=[1,2,3,4,5,6,7,8],
                        type=int,
                        help='number of matches that can occur concurrently')
arg_parser.add_argument('-d', '--dimension',
                        default=11,
                        type=int,
                        help='size of the hex board')
arg_parser.add_argument('-s', '--seconds',
                        default=300,
                        type=int,
                        help='per-agent game timer length (seconds)')
arg_parser.add_argument('-t', '--threads',
                        default=4,
                        type=int,
                        help='per-agent thread hard-limit')
arg_parser.add_argument('-m', '--memory',
                        default=1024,
                        type=int,
                        help='per-agent memory hard-limit (MiB)')
arg_parser.add_argument('-v', '--verbose',
                        action='store_true',
                        help='enabling verbose logging for the server')


def log(string):
    print(f'[tournament-host] {string}')


async def game(sem, args, agent_pair, uid_pool):
    '''
    Plays a single game using the hex-server program, between the given pair
    of user agents and taking 2 uids from the current pool.
    '''

    await sem.acquire() # wait until we can play another (potentially concurrent) game

    agent1, agent2 = agent_pair
    agent1_uid = uid_pool.pop(0)
    agent2_uid = uid_pool.pop(0)

    log(f'Starting game between {agent1} (uid: {agent1_uid}) and {agent2} (uid: {agent2_uid}) ...')

    proc = await asyncio.create_subprocess_exec(
            HEX_SERVER_PROGRAM,
            '-a', agent1, '-ua', agent1_uid,
            '-b', agent2, '-ub', agent2_uid,
            '-d', str(args.dimension),
            '-s', str(args.seconds),
            '-t', str(args.threads),
            '-m', str(args.memory),
            '-v' if args.verbose else '',
            stdout=asyncio.subprocess.PIPE)

    stdout, _ = await proc.communicate()
    output = stdout.decode()
    csv_rows = [
        [e.strip() for e in row.split(',') if len(e)] for row in output.split('\n') if len(row)
    ]

    uid_pool.append(agent1_uid)
    uid_pool.append(agent2_uid)

    sem.release()

    return dict(zip(*csv_rows))


async def tournament(args, schedule):
    '''
    Play an entire tournament, using the given parsed args and the given
    agent-pair schedule.
    '''

    sem = asyncio.BoundedSemaphore(args.concurrent_matches) # limits concurrent matches
    uid_pool = [uid for uid in HEX_AGENT_UIDS]

    log(f'Starting tournament with {args.concurrent_matches} concurrent games...')

    start = time.time()

    tasks = [asyncio.create_task(game(sem, args, pair, uid_pool)) for pair in schedule]
    results = await asyncio.gather(*tasks)

    end = time.time()

    log(f'Finished tournament in {end - start:.03} seconds')

    return results


def main():
    if not os.path.exists(HEX_SERVER_PROGRAM):
        print(f'Failed to find server executable: {HEX_SERVER_PROGRAM}. Ensure it exists (run Make?) before attempting a tournament', file=sys.stderr)
        quit(1)

    args = arg_parser.parse_args()

    schedule = []
    with args.schedule_file as f:
        for raw_line in f.read().strip().split('\n'):
            line = raw_line.strip()
            if len(line) == 0: continue # skip empty lines
            if line[0] == '#': continue # skip commented lines
            elif ',' in line:  schedule.append(line.split(',')[:2])

    results = asyncio.run(tournament(args, schedule))

    fields = [
        'agent_1', 'agent_1_won', 'agent_1_rounds', 'agent_1_secs', 'agent_1_err', 'agent_1_logfile',
        'agent_2', 'agent_2_won', 'agent_2_rounds', 'agent_2_secs', 'agent_2_err', 'agent_2_logfile',
    ]

    fields_hdr = ','.join(fields)

    with args.output_file as f:
        f.write(f'game,{fields_hdr},\n')
        for i, res in enumerate(results):
            fields_row = ','.join(map(lambda f: str(res[f]), fields))
            f.write(f'{i},{fields_row},\n')


if __name__ == '__main__':
    main()

