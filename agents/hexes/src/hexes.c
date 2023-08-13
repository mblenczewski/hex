#include "hexes.h"
#include "hexes/agent.h"
#include "hexes/board.h"
#include "hexes/log.h"
#include "hexes/network.h"
#include "hexes/threadpool.h"

struct opts opts = {
	.log_level = LOG_INFO,
	.agent_type = AGENT_MCTS,
};

enum game_state {
	GAME_START,
	GAME_RECV,
	GAME_SEND,
	GAME_END,
};

struct game {
	struct network network;
	struct threadpool threadpool;
	struct board board;
	struct agent agent;

	size_t round, thread_limit, mem_limit_mib;
	struct timespec timer;
	enum hex_player player, opponent;

	enum game_state state;
	bool game_over;
};

static struct game game = {
	.state = GAME_START,
};

static void
start_handler(struct game *game);

static void
recv_handler(struct game *game);

static void
send_handler(struct game *game);

static void
end_handler(struct game *game);

int
main(int argc, char **argv)
{
	srandom(getpid());

	if (argc < 3) {
		dbglog(LOG_ERROR, "Usage: %s <host> <port>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	char *host = argv[1], *port = argv[2];
	if (!network_init(&game.network, host, port)) {
		dbglog(LOG_ERROR, "Failed to initialise network (connecting to %s:%s)\n", host, port);
		exit(EXIT_FAILURE);
	}

	while (!game.game_over) {
		dbglog(LOG_INFO, "==============================\n");

		switch (game.state) {
		case GAME_START:	start_handler(&game); break;
		case GAME_RECV:		recv_handler(&game); break;
		case GAME_SEND:		send_handler(&game); break;
		case GAME_END:		end_handler(&game); break;
		}

		game.round++;
	}

	agent_free(&game.agent);
	board_free(&game.board);
	threadpool_free(&game.threadpool);

	network_free(&game.network);

	exit(EXIT_SUCCESS);
}

static void
start_handler(struct game *game)
{
	assert(game);

	enum hex_msg_type expected[] = { HEX_MSG_START, };

	struct hex_msg msg;
	if (!network_recv(&game->network, &msg, expected, ARRLEN(expected))) {
		dbglog(LOG_ERROR, "Failed to receive message from server\n");
		goto error;
	}

	game->player = msg.data.start.player;
	game->opponent = hexopponent(game->player);
	game->timer.tv_sec = msg.data.start.game_secs;
	game->thread_limit = msg.data.start.thread_limit;
	game->mem_limit_mib = msg.data.start.mem_limit_mib;

	dbglog(LOG_INFO, "Received game parameters: player: %s, board size: %" PRIu32 ", game secs: %" PRIu32 ", thread limit: %" PRIu32 ", mem limit (MiB): %" PRIu32 "\n",
			hexplayerstr(game->player), msg.data.start.board_size, game->timer.tv_sec, game->thread_limit, game->mem_limit_mib);

	if (!threadpool_init(&game->threadpool, msg.data.start.thread_limit - 1)) {
		dbglog(LOG_ERROR, "Failed to initialise threadpool\n");
		goto error;
	}

	if (!board_init(&game->board, msg.data.start.board_size)) {
		dbglog(LOG_ERROR, "Failed to initialise board\n");
		goto error;
	}

	if (!agent_init(&game->agent, (enum agent_type) opts.agent_type, &game->board,
			&game->threadpool, game->mem_limit_mib, game->player)) {
		dbglog(LOG_ERROR, "Failed to initialise agent\n");
		goto error;
	}

	switch (game->player) {
	case HEX_PLAYER_BLACK: game->state = GAME_SEND; break;
	case HEX_PLAYER_WHITE: game->state = GAME_RECV; break;
	}

	return;

error:
	game->state = GAME_END;
}

static void
recv_handler(struct game *game)
{
	assert(game);

	enum hex_msg_type expected[] = { HEX_MSG_MOVE, HEX_MSG_SWAP, HEX_MSG_END, };

	struct hex_msg msg;
	if (!network_recv(&game->network, &msg, expected, ARRLEN(expected))) {
		dbglog(LOG_ERROR, "Failed to receive message from server\n");
		goto error;
	}

	switch (msg.type) {
	case HEX_MSG_MOVE: {
		dbglog(LOG_INFO, "Received move {x=%" PRIu32 ", y=%" PRIu32 "} from opponent\n",
				msg.data.move.board_x, msg.data.move.board_y);

		if (!board_play(&game->board, game->opponent, msg.data.move.board_x,
				msg.data.move.board_y)) {
			dbglog(LOG_ERROR, "Failed to play received move on board\n");
			goto error;
		}

		agent_play(&game->agent, game->opponent, msg.data.move.board_x, msg.data.move.board_y);

		if (game->round == 1 && /* TODO: calculate when to attempt to swap board */ false) {
			game->state = GAME_RECV;
		} else {
			game->state = GAME_SEND;
		}
	} break;

	case HEX_MSG_SWAP: {
		dbglog(LOG_INFO, "Received swap msg from opponent\n");

		board_swap(&game->board);
		agent_swap(&game->agent);

		game->state = GAME_SEND;
	} break;

	case HEX_MSG_END: {
		dbglog(LOG_INFO, "Player %s has won the game\n", hexplayerstr(msg.data.end.winner));

		game->state = GAME_END;
	} break;
	}

	return;

error:
	game->state = GAME_END;
}

static void
send_handler(struct game *game)
{
	assert(game);

	struct hex_msg msg = {
		.type = HEX_MSG_MOVE,
	};

	size_t total_rounds = (game->board.size * game->board.size) / 2;

	struct timespec timeout = {
		.tv_sec = game->timer.tv_sec / (total_rounds - game->round),
	}, start, end, diff, new_timer;

	clock_gettime(CLOCK_MONOTONIC, &start);
	if (!agent_next(&game->agent, timeout, &msg.data.move.board_x, &msg.data.move.board_y)) {
		dbglog(LOG_ERROR, "Failed to generate next move\n");
		goto error;
	}
	clock_gettime(CLOCK_MONOTONIC, &end);

	difftimespec(&end, &start, &diff);
	difftimespec(&game->timer, &diff, &new_timer);
	game->timer = new_timer;

	dbglog(LOG_INFO, "Generated move: {x=%" PRIu32 ", y=%" PRIu32 "}\n", msg.data.move.board_x, msg.data.move.board_y);

	if (!board_play(&game->board, game->player, msg.data.move.board_x, msg.data.move.board_y)) {
		dbglog(LOG_ERROR, "Failed to play generated move on board\n");
		goto error;
	}

	agent_play(&game->agent, game->player, msg.data.move.board_x, msg.data.move.board_y);

	if (!network_send(&game->network, &msg)) {
		dbglog(LOG_ERROR, "Failed to send message to server\n");
		goto error;
	}

	game->state = GAME_RECV;

	return;

error:
	game->state = GAME_END;
}

static void
end_handler(struct game *game)
{
	assert(game);

	dbglog(LOG_INFO, "Game over. Goodbye, World!\n");

	game->game_over = true;
}
