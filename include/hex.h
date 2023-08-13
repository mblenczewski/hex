#ifndef HEX_H
#define HEX_H

#ifdef _XOPEN_SOURCE
#undef _XOPEN_SOURCE
#endif /* _XOPEN_SOURCE */

#define _XOPEN_SOURCE 700

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif /* _GNU_SOURCE */

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif /* _DEFAULT_SOURCE */

#ifdef __cplusplus
	#include <cassert>
	#include <cerrno>
	#include <cinttypes>
	#include <climits>
	#include <cstdarg>
	#include <cstdbool>
	#include <cstdint>
	#include <cstdio>
	#include <cstdlib>
	#include <cstring>
#else
	#include <assert.h>
	#include <errno.h>
	#include <inttypes.h>
	#include <limits.h>
	#include <stdarg.h>
	#include <stdbool.h>
	#include <stdint.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
#endif /* __cplusplus */

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "hex/types.h"
#include "hex/proto.h"

/* timeout for accept()-ing an agent connection before assuming a forfeit
 */
#define HEX_AGENT_ACCEPT_TIMEOUT_MS (1 * 1000)

#define HEX_AGENT_LOGFILE_TEMPLATE "/tmp/hex-agent.XXXXXX"
#define HEX_AGENT_LOGFILE_MODE (0666)

extern struct args {
	char *agent_1;
	uid_t agent_1_uid;
	char *agent_2;
	uid_t agent_2_uid;
	u32 board_dimensions;
	u32 game_secs;
	u32 thread_limit;
	u32 mem_limit_mib;
	b32 verbose;
} args;

enum hex_error {
	HEX_ERROR_OK,
	HEX_ERROR_GAME_OVER,
	HEX_ERROR_TIMEOUT,
	HEX_ERROR_BAD_MOVE,
	HEX_ERROR_BAD_MSG,
	HEX_ERROR_DISCONNECT,
	HEX_ERROR_SERVER,
};

inline char const *
hexerrorstr(enum hex_error val)
{
	switch (val) {
		case HEX_ERROR_OK:		return "OK";
		case HEX_ERROR_GAME_OVER:	return "GAME_OVER";
		case HEX_ERROR_TIMEOUT:		return "TIMEOUT";
		case HEX_ERROR_BAD_MOVE:	return "BAD_MOVE";
		case HEX_ERROR_BAD_MSG:		return "BAD_MSG";
		case HEX_ERROR_DISCONNECT:	return "DISCONNECT";
		case HEX_ERROR_SERVER:		return "SERVER";
		default:			return "UNKNOWN";
	}
}

struct statistics {
	char *agent_1;
	b32 agent_1_won;
	u32 agent_1_rounds;
	f32 agent_1_secs;
	enum hex_error agent_1_err;
	char *agent_2;
	b32 agent_2_won;
	u32 agent_2_rounds;
	f32 agent_2_secs;
	enum hex_error agent_2_err;
};

struct agent_state {
	/* which player are we, and what agent do we run */
	enum hex_player player;
	char *agent;
	uid_t agent_uid;
	char logfile[PATH_MAX];

	/* how much time this agent has left to execute before it times out */
	struct timespec timer;

	/* socket for communicating with agent */
	int sockfd;
	struct sockaddr_storage sock_addr;
	socklen_t sock_addrlen;
};

enum cell_state {
	CELL_EMPTY,
	CELL_BLACK,
	CELL_WHITE,
};

struct board_segment {
	s16 parent_relptr; /* pointer to root of rooted tree */
	u8 rank; /* disambiguation between identical segments */
	u8 cell; /* the owner of the current cell */
};

static inline s16
board_segment_abs2rel(struct board_segment *base, struct board_segment *absptr) {
	return RELPTR_ABS2REL(s16, base, absptr);
}

static inline struct board_segment *
board_segment_rel2abs(struct board_segment *base, s16 relptr) {
	return RELPTR_REL2ABS(struct board_segment *, s16, base, relptr);
}

extern struct board_segment *
board_segment_root(struct board_segment *self);

extern void
board_segment_merge(struct board_segment *restrict self, struct board_segment *restrict elem);

extern b32
board_segment_joined(struct board_segment *self, struct board_segment *elem);

struct board_state {
	u32 size;

	/* track connections between board "segments" (groups of cells owned
	 * by one player), and the edges for each player
	 */
	struct board_segment black_source, black_sink, white_source, white_sink;
	struct board_segment segments[];
};

extern struct board_state *
board_alloc(size_t size);

extern void
board_free(struct board_state *self);

extern void
board_print(struct board_state *self);

extern b32
board_play(struct board_state *self, enum hex_player player, s32 x, s32 y);

extern void
board_swap(struct board_state *self);

extern b32
board_completed(struct board_state *self, enum hex_player *winner);

struct server_state {
	struct agent_state black_agent, white_agent;
	struct board_state *board;

	/* socket for accepting agent connections */
	int servfd;
	struct sockaddr_storage serv_addr;
	socklen_t serv_addrlen;
	char serv_host[NI_MAXHOST], serv_port[NI_MAXSERV];
};

extern bool
server_init(struct server_state *state);

extern void
server_free(struct server_state *state);

extern bool
server_spawn_agent(struct server_state *state, struct agent_state *agent_state);

extern void
server_wait_all_agents(struct server_state *state);

extern void
server_run(struct server_state *state, struct statistics *statistics);

inline void
errlog(char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
}

inline void
dbglog(char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	if (args.verbose) vfprintf(stderr, fmt, va);
	va_end(va);
}

inline void
difftimespec(struct timespec *restrict lhs, struct timespec *restrict rhs, struct timespec *restrict out)
{
	if (lhs->tv_sec <= rhs->tv_sec && lhs->tv_nsec < rhs->tv_nsec) {
		out->tv_sec = 0;
		out->tv_nsec = 0;
	} else {
		out->tv_sec = lhs->tv_sec - rhs->tv_sec - (lhs->tv_nsec < rhs->tv_nsec);
		out->tv_nsec = lhs->tv_nsec - rhs->tv_nsec + (lhs->tv_nsec < rhs->tv_nsec) * NANOSECS;
	}
}

#endif /* HEX_H */
