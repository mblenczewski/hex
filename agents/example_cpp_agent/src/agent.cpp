#include "agent.hpp"

enum class Cell {
	BLACK = HEX_PLAYER_BLACK,
	WHITE = HEX_PLAYER_WHITE,
	EMPTY,
};

struct Move {
	u32 x, y;

	bool operator==(const Move &rhs) {
		return this->x == rhs.x && this->y == rhs.y;
	}
};

void swap(Move &lhs, Move &rhs) {
	std::swap(lhs.x, rhs.x);
	std::swap(lhs.y, rhs.y);
}

class Board {
	u32 size;
	std::vector<Cell> cells;
	std::vector<Move> moves;

public:
	template <class URBG>
	Board(u32 size, URBG &&rng) : size(size) {
		cells.reserve(size * size);
		moves.reserve(size * size);

		for (u32 j = 0; j < this->size; j++) {
			for (u32 i = 0; i < this->size; i++) {
				this->cells.push_back(Cell::EMPTY);

				Move move{i, j};
				this->moves.push_back(move);
			}
		}

		std::shuffle(this->moves.begin(), this->moves.end(), rng);
	}

	bool play(enum hex_player player, u32 x, u32 y) {
		Cell &cell = this->cells.at(y * this->size + x);
		if (cell != Cell::EMPTY) return false;

		switch (player) {
		case HEX_PLAYER_BLACK:
			cell = Cell::BLACK;
			break;

		case HEX_PLAYER_WHITE:
			cell = Cell::WHITE;
			break;
		}

		Move move{x, y};
		auto it = std::find(this->moves.begin(), this->moves.end(), move);
		if (it != std::end(this->moves)) {
			::swap(*it, this->moves.back());
			this->moves.pop_back();
		}

		return true;
	}

	template <class URBG>
	void swap(URBG &&rng) {
		this->moves.clear();

		for (u32 j = 0; j < this->size; j++) {
			for (u32 i = 0; i < this->size; i++) {
				Cell &cell = this->cells.at(j * this->size + i);

				switch (cell) {
				case Cell::BLACK: cell = Cell::WHITE; break;
				case Cell::WHITE: cell = Cell::BLACK; break;
				case Cell::EMPTY:
					Move move{i, j};
					this->moves.push_back(move);
					break;
				}
			}
		}

		std::shuffle(this->moves.begin(), this->moves.end(), rng);
	}

	bool next(Move &out) {
		if (this->moves.empty()) return false;

		out = this->moves.back();
		this->moves.pop_back();

		return true;
	}
};

class Net {
	int sockfd;
public:
	Net() : sockfd(-1) {}

	bool init(char *host, char *port) {
		struct addrinfo hints, *addrinfo, *ptr;

		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		int res;
		if ((res = getaddrinfo(host, port, &hints, &addrinfo))) {
			std::cerr << "getaddrinfo: " << gai_strerror(res) << std::endl;
			return false;
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
			std::cerr << "Failed to connect to " << host << ":" << port << std::endl;
			return false;
		}

		this->sockfd = sockfd;

		return true;
	}

	bool recv_msg(struct hex_msg &out, const std::vector<enum hex_msg_type> &expected) {
		u8 buf[HEX_MSG_SZ];

		size_t nbytes_recv = 0, len = HEX_MSG_SZ;

		do {
			ssize_t curr = recv(this->sockfd, buf + nbytes_recv, len - nbytes_recv, 0);
			if (curr <= 0) return false; // error or socket shutdown
			nbytes_recv += curr;
		} while (nbytes_recv < len);

		struct hex_msg msg;
		if (!hex_msg_try_deserialise(buf, &msg)) return false;

		if (std::find(expected.begin(), expected.end(), msg.type) != std::end(expected)) {
			out = msg;
			return true;
		}

		return false;
	}

	bool send_msg(const struct hex_msg &msg) {
		u8 buf[HEX_MSG_SZ];
		if (!hex_msg_try_serialise(&msg, buf)) return false;

		size_t nbytes_sent = 0, len = HEX_MSG_SZ;

		do {
			ssize_t curr = send(this->sockfd, buf + nbytes_sent, len - nbytes_sent, 0);
			if (curr <= 0) return false; // error or socket shutdown
			nbytes_sent += curr;
		} while (nbytes_sent < len);

		return true;
	}
};

enum class State {
	START,
	RECV,
	SEND,
	END,
};

std::ostream &operator<<(std::ostream &os, const State &self) {
	return os << static_cast<std::underlying_type<State>::type>(self);
}

int
main(int argc, char *argv[])
{
	std::minstd_rand rand;
	rand.seed(getpid());

	if (argc < 3) {
		std::cerr << "Not enough args: "  << argv[0] << " <host> <port>" << std::endl;
		exit(EXIT_FAILURE);
	}

	char *host = argv[1], *port = argv[2];

	Net net;
	if (!net.init(host, port)) {
		std::cerr << "Failed to initialise network" << std::endl;
		exit(EXIT_FAILURE);
	}

	State state = State::START;
	std::unique_ptr<Board> board;

	/* initialised to satisfy GCC's linter and sanitiser */
	enum hex_player player = HEX_PLAYER_BLACK;
	enum hex_player opponent = HEX_PLAYER_WHITE;
	enum hex_player winner = HEX_PLAYER_BLACK;

	// game parameters (unused)
	u32 game_secs, thread_limit, mem_limit_mib;
	(void) game_secs; (void) thread_limit; (void) mem_limit_mib;

	bool game_over = false, first_round = true;
	while (!game_over) {
		switch (state) {
		case State::START: {
			std::vector<enum hex_msg_type> expected_msg_types = {HEX_MSG_START};

			struct hex_msg msg;
			if (!net.recv_msg(msg, expected_msg_types)) {
				std::cerr << "Failed to receive message from hex server" << std::endl;
				exit(EXIT_FAILURE);
			}

			player = static_cast<enum hex_player>(msg.data.start.player);
			opponent = hexopponent(player);
			game_secs = msg.data.start.game_secs;
			thread_limit = msg.data.start.thread_limit;
			mem_limit_mib = msg.data.start.mem_limit_mib;

			u32 board_size = msg.data.start.board_size;

			board = std::make_unique<Board>(board_size, rand);

			std::cout << "[" << hexplayerstr(player) << "] Starting game: "
				  << board_size << "x" << board_size << ", "
				  << game_secs << "secs" << std::endl;

			switch (player) {
			case HEX_PLAYER_BLACK: state = State::SEND; break;
			case HEX_PLAYER_WHITE: state = State::RECV; break;
			}
		} break;

		case State::RECV: {
			std::vector<enum hex_msg_type> expected_msg_types = {HEX_MSG_MOVE, HEX_MSG_SWAP, HEX_MSG_END};

			struct hex_msg msg;
			if (!net.recv_msg(msg, expected_msg_types)) {
				std::cerr << "Failed to receive message from hex server" << std::endl;
				exit(EXIT_FAILURE);
			}

			switch (msg.type) {
			case HEX_MSG_MOVE:
				board->play(opponent, msg.data.move.board_x, msg.data.move.board_y);

				if (first_round && rand() % 2) {
					board->swap(rand);

					msg.type = HEX_MSG_SWAP;
					if (!net.send_msg(msg)) {
						std::cerr << "Failed to send message to hex server" << std::endl;
						exit(EXIT_FAILURE);
					}

					state = State::RECV;
				} else {
					state = State::SEND;
				}
				break;

			case HEX_MSG_SWAP:
				board->swap(rand);
				state = State::SEND;
				break;

			case HEX_MSG_END:
				winner = static_cast<enum hex_player>(msg.data.end.winner);
				state = State::END;
				break;
			}

			first_round = false;
		} break;

		case State::SEND: {
			struct hex_msg msg;
			msg.type = HEX_MSG_MOVE;

			Move move;
			if (!board->next(move)) {
				std::cerr << "Failed to generate next board move" << std::endl;
				exit(EXIT_FAILURE);
			}

			board->play(player, move.x, move.y);

			msg.data.move.board_x = move.x;
			msg.data.move.board_y = move.y;

			if (!net.send_msg(msg)) {
				std::cerr << "Failed to send message to hex server" << std::endl;
				exit(EXIT_FAILURE);
			}

			state = State::RECV;
			first_round = false;
		} break;

		case State::END: {
			std::cout << "[" << hexplayerstr(player) << "] Player " << hexplayerstr(winner) << " has won the game" << std::endl;
			game_over = true;
		} break;

		default:
			std::cerr << "Unknown game state: " << state << std::endl;
			exit(EXIT_FAILURE);
			break;
		}
	}

	exit(EXIT_SUCCESS);
}
