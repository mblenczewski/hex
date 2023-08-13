#include "agent.h"

int
net_init(char *restrict host, char *restrict port);

bool
net_recv_msg(int sock, struct hex_msg *out, enum hex_msg_type expected[], size_t len);

bool
net_send_msg(int sock, struct hex_msg *msg);

enum board_cell {
	CELL_BLACK = HEX_PLAYER_BLACK,
	CELL_WHITE = HEX_PLAYER_WHITE,
	CELL_EMPTY,
};

struct move {
	u32 x, y;
};

void
move_swap(struct move *restrict lhs, struct move *restrict rhs);

struct board {
	u32 size;

	enum board_cell *cells;

	size_t moves_len;
	struct move *moves;
};

bool
board_init(struct board *self, u32 size);

bool
board_play(struct board *self, enum hex_player player, u32 x, u32 y);

void
board_swap(struct board *self);

bool
board_next(struct board *self, u32 *out_x, u32 *out_y);

enum game_state {
	GAME_START,
	GAME_RECV,
	GAME_SEND,
	GAME_END,
};

int
main(int argc, char **argv)
{
	srandom(getpid());

	if (argc < 3) {
		fprintf(stderr, "Not enough args: %s <host> <port>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	char *host = argv[1], *port = argv[2];

	int sockfd = net_init(host, port);
	if (sockfd == -1) {
		fprintf(stderr, "Failed to initialise network\n");
		exit(EXIT_FAILURE);
	}

	enum game_state game_state = GAME_START;
	struct board board;

	enum hex_player player, opponent, winner;

	u32 game_secs, thread_limit, mem_limit_mib; // currently unused
	(void) game_secs; (void) thread_limit; (void) mem_limit_mib;

	bool game_over = false, first_round = true;
	while (!game_over) {
		switch (game_state) {
		case GAME_START: {
			enum hex_msg_type expected_msg_types[] = {
				HEX_MSG_START,
			};

			struct hex_msg msg;
			if (!net_recv_msg(sockfd, &msg, expected_msg_types, ARRLEN(expected_msg_types))) {
				fprintf(stderr, "Failed to receive message from hex server\n");
				exit(EXIT_FAILURE);
			}

			// unpack all parameters
			player = msg.data.start.player;
			game_secs = msg.data.start.game_secs;
			thread_limit = msg.data.start.thread_limit;
			mem_limit_mib = msg.data.start.mem_limit_mib;

			u32 board_size = msg.data.start.board_size;

			if (!board_init(&board, board_size)) {
				fprintf(stderr, "Failed to allocate game board of size %" PRIu32 "x%" PRIu32 "\n",
						board_size, board_size);
				exit(EXIT_FAILURE);
			}

			switch (player) {
			case HEX_PLAYER_BLACK:
				opponent = HEX_PLAYER_WHITE;
				game_state = GAME_SEND;
				break;

			case HEX_PLAYER_WHITE:
				opponent = HEX_PLAYER_BLACK;
				game_state = GAME_RECV;
				break;
			}

			printf("[%s] Starting game: %" PRIu32 "x%" PRIu32 ", %" PRIu32 " secs\n",
				hexplayerstr(player), board_size, board_size, game_secs);
		} break;

		case GAME_RECV: {
			enum hex_msg_type expected_msg_types[] = {
				HEX_MSG_MOVE,
				HEX_MSG_SWAP,
				HEX_MSG_END,
			};

			struct hex_msg msg;
			if (!net_recv_msg(sockfd, &msg, expected_msg_types, ARRLEN(expected_msg_types))) {
				fprintf(stderr, "Failed to receive message from hex server\n");
				exit(EXIT_FAILURE);
			}

			switch (msg.type) {
			case HEX_MSG_MOVE:
				board_play(&board, opponent, msg.data.move.board_x, msg.data.move.board_y);

				if (first_round && random() % 2) {
					board_swap(&board);

					msg.type = HEX_MSG_SWAP;
					if (!net_send_msg(sockfd, &msg)) {
						fprintf(stderr, "Failed to send swap message to hex server\n");
						exit(EXIT_FAILURE);
					}

					game_state = GAME_RECV;
				} else {
					game_state = GAME_SEND;
				}
				break;

			case HEX_MSG_SWAP:
				board_swap(&board);
				game_state = GAME_SEND;
				break;

			case HEX_MSG_END:
				winner = msg.data.end.winner;
				game_state = GAME_END;
				break;
			}

			first_round = false;
		} break;

		case GAME_SEND: {
			struct hex_msg msg = {
				.type = HEX_MSG_MOVE,
			};

			if (!board_next(&board, &msg.data.move.board_x, &msg.data.move.board_y)) {
				fprintf(stderr, "Failed to generate next board move\n");
				exit(EXIT_FAILURE);
			}

			board_play(&board, player, msg.data.move.board_x, msg.data.move.board_y);

			if (!net_send_msg(sockfd, &msg)) {
				fprintf(stderr, "Failed to send message to hex server\n");
				exit(EXIT_FAILURE);
			}

			game_state = GAME_RECV;
			first_round = false;
		} break;

		case GAME_END: {
			printf("[%s] Player %s has won the game\n",
				hexplayerstr(player), hexplayerstr(winner));
			game_over = true;
		} break;

		default:
			fprintf(stderr, "Unknown game state: %d\n", game_state);
			exit(EXIT_FAILURE);
			break;
		}
	}

	exit(EXIT_SUCCESS);
}

int
net_init(char *restrict host, char *restrict port)
{
	assert(host);
	assert(port);

	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	}, *addrinfo, *ptr;

	int res;
	if ((res = getaddrinfo(host, port, &hints, &addrinfo))) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(res));
		return -1;
	}

	int sockfd;
	for (ptr = addrinfo; ptr; ptr = ptr->ai_next) {
		sockfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (sockfd == -1) continue;
		if (connect(sockfd, ptr->ai_addr, ptr->ai_addrlen) != -1) break;
		close(sockfd);
	}

	freeaddrinfo(addrinfo);

	if (!ptr) {
		fprintf(stderr, "Failed to connect to %s:%s\n", host, port);
		return -1;
	}

	return sockfd;
}

static inline size_t
net_recv_all(int sock, u8 *buf, size_t len)
{
	assert(buf);

	size_t nbytes_received = 0;

	do {
		ssize_t res = recv(sock, buf + nbytes_received, len - nbytes_received, 0);
		if (res <= 0) break; // error or socket shutdown
		nbytes_received += res;
	} while (nbytes_received < len);

	return nbytes_received;
}

bool
net_recv_msg(int sock, struct hex_msg *out, enum hex_msg_type expected[], size_t len)
{
	assert(out);
	assert(expected);

	u8 buf[HEX_MSG_SZ];
	if (!(net_recv_all(sock, buf, HEX_MSG_SZ) == HEX_MSG_SZ)) return false;

	struct hex_msg msg;
	if (!hex_msg_try_deserialise(buf, &msg)) return false;

	for (size_t i = 0; i < len; i++) {
		if (msg.type == expected[i]) {
			*out = msg;
			return true;
		}
	}

	return false;
}

static inline size_t
net_send_all(int sock, u8 *buf, size_t len)
{
	assert(buf);

	size_t nbytes_sent = 0;

	do {
		ssize_t res = send(sock, buf + nbytes_sent, len - nbytes_sent, 0);
		if (res <= 0) break; // error or socket shutdown
		nbytes_sent += res;
	} while (nbytes_sent < len);

	return nbytes_sent;
}

bool
net_send_msg(int sock, struct hex_msg *msg)
{
	assert(msg);

	u8 buf[HEX_MSG_SZ];
	if (!hex_msg_try_serialise(msg, buf)) return false;

	return net_send_all(sock, buf, HEX_MSG_SZ) == HEX_MSG_SZ;
}

void
move_swap(struct move *restrict lhs, struct move *restrict rhs)
{
	assert(lhs);
	assert(rhs);

	struct move tmp = *lhs;
	*lhs = *rhs;
	*rhs = tmp;
}

static void
shuffle_moves(struct move *arr, size_t len)
{
	for (size_t i = 0; i < len - 2; i++) {
		size_t j = (i + random()) % len;
		move_swap(&arr[i], &arr[j]);
	}
}

bool
board_init(struct board *self, u32 size)
{
	assert(self);

	self->size = size;

	if (!(self->cells = malloc(size * size * sizeof *self->cells)))
		return false;

	self->moves_len = size * size;
	if (!(self->moves = malloc(size * size * sizeof *self->moves))) {
		free(self->cells);
		return false;
	}

	for (size_t j = 0; j < size; j++) {
		for (size_t i = 0; i < size; i++) {
			size_t idx = j * size + i;

			self->cells[idx] = CELL_EMPTY;

			self->moves[idx].x = i;
			self->moves[idx].y = j;
		}
	}

	shuffle_moves(self->moves, self->moves_len);

	return true;
}

bool
board_play(struct board *self, enum hex_player player, u32 x, u32 y)
{
	assert(self);

	enum board_cell *cell = &self->cells[y * self->size + x];
	if (*cell != CELL_EMPTY) return false;

	switch (player) {
	case HEX_PLAYER_BLACK:
		*cell = CELL_BLACK;
		break;

	case HEX_PLAYER_WHITE:
		*cell = CELL_WHITE;
		break;

	default:
		return false;
	}

	for (size_t i = 0; i < self->moves_len; i++) {
		if (self->moves[i].x == x && self->moves[i].y == y) {
			move_swap(&self->moves[i], &self->moves[--self->moves_len]);
			break;
		}
	}

	return true;
}

void
board_swap(struct board *self)
{
	assert(self);

	self->moves_len = 0;

	for (size_t j = 0; j < self->size; j++) {
		for (size_t i = 0; i < self->size; i++) {
			enum board_cell *cell = &self->cells[j * self->size + i];

			switch (*cell) {
			case CELL_BLACK:
				*cell = CELL_WHITE;
				break;

			case CELL_WHITE:
				*cell = CELL_BLACK;
				break;

			default: {
				struct move *move = &self->moves[self->moves_len++];
				move->x = i;
				move->y = j;
			} break;
			}
		}
	}

	shuffle_moves(self->moves, self->moves_len);
}

bool
board_next(struct board *self, u32 *out_x, u32 *out_y)
{
	assert(self);
	assert(out_x);
	assert(out_y);

	if (self->moves_len == 0) return false;

	struct move move = self->moves[--self->moves_len];
	*out_x = move.x;
	*out_y = move.y;

	return true;
}

extern inline b32
hex_msg_try_serialise(struct hex_msg const *msg, u8 out[static HEX_MSG_SZ]);

extern inline b32
hex_msg_try_deserialise(u8 buf[static HEX_MSG_SZ], struct hex_msg *out);

extern inline char const *
hexplayerstr(enum hex_player val);
