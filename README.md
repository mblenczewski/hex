hex-server: A Simple 'Hex' Server
==============================================================================
A simple 'Hex' game server, for running 1 round of a tournament between 2
player agents. Supports the PIE rule. Includes example agents written in C,
C++, Java, and Python3.

To run tournaments, there exists a helper script written for Python3.11 (see
`tournament-host.py`), which takes a comma-delimited agent-pair tournament
schedule, runs the tournament, and collects the resulting statistics in the
given output file.

NOTE: currently, the Java agent (written for Java 17) requires ~16 threads to
load a jar and execute, thanks to the JVM requirements. since the default
thread limit is 4, this agent cannot be used without increasing the limit to
16

hex-server: Building
------------------------------------------------------------------------------
To build the server, run the following shell commands in the project's root
directory:
```sh
$ make build    # default, builds only the hex server
$ make extra    # optional, builds all included agents
$ make all      # optional, builds the hex server and all included agents
```

To clean all built artefacts, run the following shell commands:
```sh
$ make clean    # cleans only the hex server binary
$ make cleanall # cleans the hex server binary and all agent artefacts
```

hex-server: Usage
------------------------------------------------------------------------------
The server can be invoked using the following shell command:
```sh
$ sudo hex-server -a <agent-1> -ua <uid> -b <agent-2> -ub <uid> \
                 [-d 11] [-s 300] [-t 4] [-m 1024] [-v]
```

NOTE: The server MUST be ran as root (i.e. as a privileged process), or by a
user with the Linux CAP_SETUID capability. This is due to the use of setuid()
to set the user id for the agents, and thus maintain process limits which work
based on the (effective) user id of a given process. Note that despite the
server running as root, the user agents run as the given users (via -ua/-ub).

Server Options:
+-----+-----------------------------------------------+-----------+-----------+
| Opt | Description                                   | Optional  | Default   |
+-----+-----------------------------------------------+-----------+-----------+
| -a  | The first agent (black)                       | Required  | N/A       |
| -ua | The uid to set for the first agent (black)    | Required  | N/A       |
| -b  | The second agent (white)                      | Required  | N/A       |
| -ub | The uid to set for the second agent (white)   | Required  | N/A       |
| -d  | Board dimensions                              | Optional  | 11        |
| -s  | Per-Agent game timer (seconds)                | Optional  | 300       |
| -t  | Per-Agent thread hard-limit                   | Optional  | 4         |
| -m  | Per-Agent memory hard-limit (MiB)             | Optional  | 1024      |
| -v  | Verbose output                                | Optional  | N/A       |
+-----+-----------------------------------------------+-----------+-----------+

Each agent will be invoked using the following shell command:
```sh
<agent-string> <server-host> <server-port>
```

For maximum flexibility and ease of use, it is recommended to write a wrapper
shell script to be passed as the `agent-string`, to allow for a more
specialised run command structure (e.g. using a compiled agent with
differently named options, or having an interpreted agent and having to pass
the agent source to the interpreter).

An example of such a wrapper script is as follows:
```sh
#!/bin/sh

exec /usr/bin/env python3 $(dirname $0)/my_agent.py $@ # forward all args
```

Writing such a wrapper shell script also means that agent-specific commandline
options (e.g. a verbose logging mode, an optimised "release" mode or
unoptimised "debug" mode, or passing the server host and port via named
commandline arguments instead of as positional ones) can be implemented,
without any special handling from the server.

For examples of both compiled and interpreted agents, as well as for an
example of the wrapper scripts used to invoke the agents, please see the
`run.sh` wrapper scripts in the example agent directories (under `agents/`).

NOTE: the wrapper script, if used, must be made executable. This can be done
using the following shell command (replacing `/tmp/my_agent/` with your
specific agent's directory):
```sh
$ chmod +x /tmp/my_agent/my_wrapper_script.sh
```

hex-server: Protocol
------------------------------------------------------------------------------
The server uses a simple binary protocol for all messages between the server
and individual agents, and will communicate between itself and an agent using
a socket.

Server Flow
1) Create processes for both agents (setting process limits)
2) accept() both agents (within a timeout)
3) send() a MSG_START to both agents
4) recv() a MSG_MOVE (or MSG_SWAP on round 1 as white only)
5) Make said move and test the board for a winner
   a) If there is a winner, goto 7)
   b) Otherwise, goto 4)
6) send() the received message to the other agent, goto 4)
7) send() a MSG_END to both agents

NOTE: if at any point in this flow an agent sends a malformed message, plays
an invalid move (e.g. attempts to move out-of-bounds, swaps except as player 2
on the first turn, moves onto a spot with an existing piece), or causes the
socket connection to close, the game is over and the other agent wins by
default.

Agent Flow
1) connect() to the server given by the commandline args (host/port)
2) recv() a MSG_START from the server
   a) If playing as black (agent 1), goto 3)
   b) If playing as white (agent 2), goto 4)
3) send() a MSG_MOVE (or MSG_SWAP on round 1 as white only)
4) recv() a MSG_MOVE, MSG_SWAP, or MSG_END
   a) If received MSG_MOVE or MSG_SWAP, update internal state, goto 3)
   b) If received MSG_END, goto 5)
5) close() connection to server

hex-server: Protocol Wire Format
------------------------------------------------------------------------------
The wire format consists of a fixed 32-byte packet, with a simple
(type:u32,params:u32[]) packet structure.

The wire format if oriented around 32-bit unsigned words for simplicity, and
values for the packet type and all parameters, will all be of this type.

NOTE: for implementations, this boils down into a single recv() or send() of
32 bytes, followed by parsing the received packet based on the `type`,
extracting all the required parameters. Agents should also make sure to follow
this exact packet structure when sending messages, as otherwise they will be
seen by the server as having sent malformed messages and the server will
consider this a forfeit. Please see the example agents under `agents/` for
specific example implementations.

hex-server: Protocol Messages
------------------------------------------------------------------------------
Below is a listing of the messages in the protocol, their IDs and parameters,
and the relationships between the server and 2 agents.

Protocol Messages:
+-------+-----------+---------------------------------------------------------+
| ID    | Name      | Params                                                  |
+-------+-----------+---------------------------------------------------------+
| 0     | MSG_START | player:u32, board_size:u32, game_secs:u32               |
|       |           | thread_limit:u32, mem_limit_mib:u32                     |
+-------+-----------+---------------------------------------------------------+
| 1     | MSG_MOVE  | board_x:u32, board_y:u32                                |
+-------+-----------+---------------------------------------------------------+
| 2     | MSG_SWAP  | N/A                                                     |
+-------+-----------+---------------------------------------------------------+
| 3     | MSG_END   | winner:u32                                              |
+-------+-----------+---------------------------------------------------------+

An example of this protocol defined in a C-like language is as follows:
```c
enum player_type : u32 {
	PLAYER_BLACK	= 0,
	PLAYER_WHITE	= 1,
};

enum msg_type : u32 {
	MSG_START	= 0,
	MSG_MOVE	= 1,
	MSG_SWAP	= 2,
	MSG_END		= 3,
};

union msg_data {
	struct {
		enum player_type player;
    u32 board_size;
    u32 game_secs;
		u32 thread_limit;
		u32 mem_limit_mib; // NOTE: in units of MiB
	} start;

	struct {
		u32 board_x;
		u32 board_y;
	} move;

/* struct { } swap; */ // NOTE: swap has no parameters

	struct {
		enum player_type winner;
	} end;
};

struct msg {
	enum msg_type type;
	union msg_data data;
};
```
