#include "hex.h"

bool
server_init(struct server_state *state)
{
	assert(state);

	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
		.ai_flags = AI_PASSIVE,
	}, *addrinfo, *ptr;

	int res;
	if ((res = getaddrinfo("localhost", "0", &hints, &addrinfo))) {
		errlog("[server] Failed to get address information: %s\n", gai_strerror(res));
		goto error_without_socket;
	}

	for (ptr = addrinfo; ptr; ptr = ptr->ai_next) {
		state->servfd = socket(ptr->ai_family, ptr->ai_socktype | SOCK_CLOEXEC, ptr->ai_protocol);
		if (state->servfd == -1) continue;
		if (bind(state->servfd, ptr->ai_addr, ptr->ai_addrlen) != -1) break;
		close(state->servfd);
	}

	freeaddrinfo(addrinfo);

	if (!ptr) {
		errlog("[server] Failed to bind server socket\n");
		goto error_without_socket;
	}

	state->serv_addrlen = sizeof state->serv_addr;
	if (getsockname(state->servfd, (struct sockaddr *) &state->serv_addr, &state->serv_addrlen)) {
		errlog("[server] Failed to get server socket addr\n");
		goto error;
	}

	if ((res = getnameinfo((struct sockaddr *) &state->serv_addr, state->serv_addrlen,
				state->serv_host, sizeof state->serv_host,
				state->serv_port, sizeof state->serv_port,
				NI_NUMERICHOST | NI_NUMERICSERV)) != 0) {
		errlog("[server] Failed to get bound socket addr host and port\n");
		goto error;
	}

	listen(state->servfd, 2);

	dbglog("[server] Server socket is listening on %s:%s\n", state->serv_host, state->serv_port);

	return true;

error:
	close(state->servfd);

error_without_socket:
	return false;
}

void
server_free(struct server_state *state)
{
	assert(state);

	close(state->servfd);
}

bool
server_spawn_agent(struct server_state *state, struct agent_state *agent_state)
{
	assert(state);
	assert(agent_state);

	int fd;
	if ((fd = mkstemp(agent_state->logfile)) != -1) {
		fchmod(fd, HEX_AGENT_LOGFILE_MODE);

		dbglog("[server] Created logfile '%s' for agent: '%s'\n",
			agent_state->logfile, agent_state->agent);
	} else {
		dbglog("[server] Failed to create logfile '%s' for agent: '%s'\n",
			agent_state->logfile, agent_state->agent);

		strcpy(agent_state->logfile, "/dev/null");
	}

	pid_t child_pid = fork();

	if (child_pid == 0) { /* child process, exec() agent */
		pid_t pid = getpid();

		dbglog("[server] Child process '%" PRIi32 "', setting uid\n", pid);

		if (setuid(agent_state->agent_uid) == -1) {
			perror("setuid");
			exit(EXIT_FAILURE); /* fork()-d process can die without issue */
		}

		dbglog("[server] Child process '%" PRIi32 "', setting resource limits\n", pid);

		struct rlimit limit;

		limit.rlim_cur = limit.rlim_max = args.thread_limit;
		prlimit(pid, RLIMIT_NPROC, &limit, NULL);

		limit.rlim_cur = limit.rlim_max = args.mem_limit_mib * 1024 * 1024;
		prlimit(pid, RLIMIT_DATA, &limit, NULL);

		char *args[] = {
			agent_state->agent,
			state->serv_host,
			state->serv_port,
			NULL,
		};

		char *env[] = {
			NULL,
		};

		dbglog("[server] Child process '%" PRIi32 "', exec()-ing agent: '%s'\n",
			pid, agent_state->agent);

		freopen("/dev/null", "rb", stdin);
		freopen(agent_state->logfile, "wb", stdout);
		freopen(agent_state->logfile, "wb", stderr);

		if (execve(agent_state->agent, args, env)) {
			perror("execve");
			exit(EXIT_FAILURE); /* fork()-d process can die without issue */
		}
	} else if (child_pid == -1) { /* parent process, fork() error */
		perror("fork");
		errlog("[server] Failed to fork() to agent process: '%s'\n", agent_state->agent);
		goto error;
	}

	/* parent process, fork() success */

	/* accept() the agent socket */
	struct pollfd pollfds[] = {
		{ .fd = state->servfd, .events = POLLIN, },
	};

	int ready = poll(pollfds, 1, HEX_AGENT_ACCEPT_TIMEOUT_MS);

	if (ready == -1) {
		perror("poll");
		goto error;
	} else if (ready == 0) {
		errlog("[server] %s (%s) timed out during accept() period, assuming forfeit\n",
			hexplayerstr(agent_state->player), agent_state->agent);
		goto error;
	}

	int sockflags = SOCK_CLOEXEC;
	agent_state->sockfd = accept4(state->servfd,
				      (struct sockaddr *) &agent_state->sock_addr,
				      &agent_state->sock_addrlen,
				      sockflags);

	if (agent_state->sockfd == -1) {
		perror("accept4");
		goto error;
	}

	return true;

error:
	kill(0, SIGKILL);

	int wpid, wstatus;
	while ((wpid = wait(&wstatus)) > 0); /* wait for all children to die */

	return false;
}

void
server_wait_all_agents(struct server_state *state)
{
	assert(state);

	int wpid, wstatus;
	while ((wpid = wait(&wstatus)) > 0) {
		dbglog("[server] Child process '%" PRIi32 "' returned code: %d\n",
			wpid, WEXITSTATUS(wstatus));
	}
}

static enum hex_error
send_msg(struct agent_state *agent, struct hex_msg *msg, b32 force);

static enum hex_error
recv_msg(struct agent_state *agent, struct hex_msg *out, enum hex_msg_type *expected, size_t len);

static enum hex_error
play_round(struct server_state *state, size_t turn, enum hex_player *winner);

void
server_run(struct server_state *state, struct statistics *statistics)
{
	assert(state);

	enum hex_error err;

	enum hex_player winner;

	/* setup common statistics */
	statistics->agent_1 = state->black_agent.agent;
	statistics->agent_2 = state->white_agent.agent;

	/* send a start message to both agents, including all game parameters
	 */
	struct hex_msg msg;
	msg.type = HEX_MSG_START;
	msg.data.start.board_size = args.board_dimensions;
	msg.data.start.game_secs = args.game_secs;
	msg.data.start.thread_limit = args.thread_limit;
	msg.data.start.mem_limit_mib = args.mem_limit_mib;

	msg.data.start.player = HEX_PLAYER_BLACK;
	if ((err = send_msg(&state->black_agent, &msg, true))) goto forfeit_black;

	msg.data.start.player = HEX_PLAYER_WHITE;
	if ((err = send_msg(&state->white_agent, &msg, true))) goto forfeit_white;

	size_t round = 0;
	while ((err = play_round(state, round++, &winner)) == HEX_ERROR_OK);

	msg.type = HEX_MSG_END;
	msg.data.end.winner = winner;

	send_msg(&state->black_agent, &msg, true);
	send_msg(&state->white_agent, &msg, true);

	/* calculate game statistics
	 */
	statistics->agent_1_won = state->black_agent.player == winner;
	statistics->agent_2_won = state->white_agent.player == winner;

	statistics->agent_1_rounds = (round + 1) / 2;
	statistics->agent_2_rounds = round / 2;

	statistics->agent_1_secs = state->black_agent.timer.tv_sec
				 + state->black_agent.timer.tv_nsec / (f32) NANOSECS;
	statistics->agent_2_secs = state->white_agent.timer.tv_sec
				 + state->white_agent.timer.tv_nsec / (f32) NANOSECS;

	if (winner == HEX_PLAYER_BLACK) {
		statistics->agent_1_err = HEX_ERROR_OK;
		statistics->agent_2_err = err;
	} else {
		statistics->agent_1_err = err;
		statistics->agent_2_err = HEX_ERROR_OK;
	}

	return;

forfeit_black:
	statistics->agent_1_won = false;
	statistics->agent_2_won = true;

	statistics->agent_1_rounds = statistics->agent_2_rounds = 0;

	statistics->agent_1_secs = state->black_agent.timer.tv_sec
				 + state->black_agent.timer.tv_nsec / (f32) NANOSECS;
	statistics->agent_2_secs = state->white_agent.timer.tv_sec
				 + state->white_agent.timer.tv_nsec / (f32) NANOSECS;

	return;

forfeit_white:
	statistics->agent_1_won = true;
	statistics->agent_2_won = false;

	statistics->agent_1_rounds = statistics->agent_2_rounds = 0;

	statistics->agent_1_secs = state->black_agent.timer.tv_sec
				 + state->black_agent.timer.tv_nsec / (f32) NANOSECS;
	statistics->agent_2_secs = state->white_agent.timer.tv_sec
				 + state->white_agent.timer.tv_nsec / (f32) NANOSECS;

	return;
}

static enum hex_error
send_msg(struct agent_state *agent, struct hex_msg *msg, b32 force)
{
	assert(agent);
	assert(msg);

	size_t nbytes_sent = 0;

	u8 buf[HEX_MSG_SZ];
	if (!hex_msg_try_serialise(msg, buf)) return HEX_ERROR_BAD_MSG;

	struct pollfd pollfd = { .fd = agent->sockfd, .events = POLLOUT, };

	struct timespec start, end, diff, temp;
	if (clock_gettime(CLOCK_MONOTONIC, &start) < 0) {
		perror("clock_gettime");
		return HEX_ERROR_SERVER;
	}

	int res;
	while (nbytes_sent < ARRLEN(buf) && (res = ppoll(&pollfd, 1, force ? NULL : &agent->timer, NULL)) > 0) {
		ssize_t curr = send(pollfd.fd, buf + nbytes_sent, ARRLEN(buf) - nbytes_sent, 0);

		if (curr <= 0) /* connection closed or error */
			return HEX_ERROR_DISCONNECT;

		if (clock_gettime(CLOCK_MONOTONIC, &end) < 0) {
			perror("clock_gettime");
			return HEX_ERROR_SERVER;
		}

		difftimespec(&end, &start, &diff);
		difftimespec(&agent->timer, &diff, &temp);

		start = end;
		agent->timer = temp;

		nbytes_sent += curr;
	}

	if (res == 0) { /* timeout */
		dbglog("[server] Timeout when sending message to %s\n",
			hexplayerstr(agent->player));
		return HEX_ERROR_TIMEOUT;
	}

	if (res == -1) {
		perror("ppoll");
		return HEX_ERROR_SERVER;
	}

	return HEX_ERROR_OK;
}

static enum hex_error
recv_msg(struct agent_state *agent, struct hex_msg *out, enum hex_msg_type *expected, size_t len)
{
	assert(agent);
	assert(out);
	assert(expected);

	size_t nbytes_received = 0;

	u8 buf[HEX_MSG_SZ];

	struct pollfd pollfd = { .fd = agent->sockfd, .events = POLLIN, };

	struct timespec start, end, diff, temp;
	if (clock_gettime(CLOCK_MONOTONIC, &start) < 0) {
		perror("clock_gettime");
		return HEX_ERROR_SERVER;
	}

	int res;
	while (nbytes_received < ARRLEN(buf) && (res = ppoll(&pollfd, 1, &agent->timer, NULL)) > 0) {
		ssize_t curr = recv(pollfd.fd, buf + nbytes_received, ARRLEN(buf) - nbytes_received, 0);

		if (curr <= 0) /* connection closed or error */
			return HEX_ERROR_DISCONNECT;

		if (clock_gettime(CLOCK_MONOTONIC, &end) < 0) {
			perror("clock_gettime");
			return HEX_ERROR_SERVER;
		}

		difftimespec(&end, &start, &diff);
		difftimespec(&agent->timer, &diff, &temp);

		start = end;
		agent->timer = temp;

		nbytes_received += curr;
	}

	if (res == 0) { /* timeout */
		dbglog("[server] Timeout while receiving message from %s\n",
			hexplayerstr(agent->player));
		return HEX_ERROR_TIMEOUT;
	}

	if (res == -1) {
		perror("ppoll");
		return HEX_ERROR_SERVER;
	}

	if (!hex_msg_try_deserialise(buf, out)) return HEX_ERROR_BAD_MSG;

	for (size_t i = 0; i < len; i++) {
		if (out->type == expected[i]) return HEX_ERROR_OK;
	}

	return HEX_ERROR_BAD_MSG;
}

static enum hex_error
play_round(struct server_state *state, size_t turn, enum hex_player *winner)
{
	assert(state);
	assert(winner);

	enum hex_error err;

	struct agent_state *agents[] = {
		[HEX_PLAYER_BLACK] = &state->black_agent,
		[HEX_PLAYER_WHITE] = &state->white_agent,
	};

	struct agent_state *player = agents[turn % 2];
	struct agent_state *opponent = agents[(turn + 1) % 2];

	dbglog("[server] round %zu, to-play: %s, opponent: %s\n",
		turn, hexplayerstr(player->player), hexplayerstr(opponent->player));

	/* on the first turn for white (i.e. turn 1 when 0-addressed), white
	 * can respond with either a MSG_MOVE, or a MSG_SWAP, but for all
	 * other turns (for both black and white), only a MSG_MOVE can be
	 * played, thus implementing the swap rule.
	 */
	enum hex_msg_type expected_msg_types[] = { HEX_MSG_MOVE, HEX_MSG_SWAP, };
	size_t expected_msg_types_len = (turn == 1) ? 2 : 1;

	struct hex_msg msg;

	if ((err = recv_msg(player, &msg, expected_msg_types, expected_msg_types_len))) {
		*winner = opponent->player;
		return err;
	}

	switch (msg.type) {
	case HEX_MSG_MOVE:
		dbglog("[server] %s made move (%u,%u)\n",
			hexplayerstr(player->player), msg.data.move.board_x, msg.data.move.board_y);

		if (!board_play(state->board, player->player, msg.data.move.board_x, msg.data.move.board_y)) {
			*winner = opponent->player;
			return HEX_ERROR_BAD_MOVE;
		}

		if (board_completed(state->board, winner)) {
			board_print(state->board);
			return HEX_ERROR_GAME_OVER;
		}
		break;

	case HEX_MSG_SWAP:
		dbglog("[server] %s swapped board\n", hexplayerstr(player->player));

		board_swap(state->board);
		break;
	}

	if ((err = send_msg(opponent, &msg, false))) {
		*winner = player->player;
		return err;
	}

	board_print(state->board);

	return HEX_ERROR_OK;
}
