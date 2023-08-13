#include "hex.h"

struct args args = {
	.agent_1 = NULL,
	.agent_1_uid = 0,
	.agent_2 = NULL,
	.agent_2_uid = 0,
	.board_dimensions = 11,
	.game_secs = 300,
	.thread_limit = 4,
	.mem_limit_mib = 1024,
	.verbose = false,
};

static void
parse_args(s32 argc, char **argv);

static void
usage(char **argv)
{
	fprintf(stderr, "Usage: %s -a <agent-1> -ua <uid> -b <agent-2> -ub <uid> [-d 11] [-s 300] [-t 4] [-m 1024] [-v] [-h]\n", argv[0]);
	fprintf(stderr, "\t-a: The command to execute for the first agent (black)\n");
	fprintf(stderr, "\t-ua: The user id to set for the first agent (black)\n");
	fprintf(stderr, "\t-b: The command to execute for the second agent (white)\n");
	fprintf(stderr, "\t-ub: The user id to set for the second agent (white)\n");
	fprintf(stderr, "\t-d: The dimensions for the game board (default: 11)\n");
	fprintf(stderr, "\t-s: The per-agent game timer, in seconds (default: 300 seconds)\n");
	fprintf(stderr, "\t-t: The per-agent thread hard-limit (default: 4 threads)\n");
	fprintf(stderr, "\t-m: The per-agent memory hard-limit, in MiB (default: 1024 MiB)\n");
	fprintf(stderr, "\t-v: Enables verbose logging on the server\n");
	fprintf(stderr, "\t-h: Prints this help information\n");
}

s32
main(s32 argc, char **argv)
{
	parse_args(argc, argv);

	if (!args.agent_1 || !args.agent_2) {
		errlog("Must provide execution targets for both agent-1 and agent-2\n");
		usage(argv);
		exit(EXIT_FAILURE);
	}

	if (!args.agent_1_uid || !args.agent_2_uid) {
		errlog("Must provide (non-root) user ids for both agent-1 and agent-2\n");
		usage(argv);
		exit(EXIT_FAILURE);
	}

	struct board_state *board = board_alloc(args.board_dimensions);
	if (!board) {
		errlog("Failed to allocate board of size %" PRIu32 "\n", args.board_dimensions);
		exit(EXIT_FAILURE);
	}

	struct server_state state = {
		.black_agent = {
			.player = HEX_PLAYER_BLACK,
			.agent = args.agent_1,
			.agent_uid = args.agent_1_uid,
			.logfile = HEX_AGENT_LOGFILE_TEMPLATE,
			.timer = { .tv_sec = args.game_secs, .tv_nsec = 0, },
			.sock_addrlen = sizeof(struct sockaddr_storage),
		},
		.white_agent = {
			.player = HEX_PLAYER_WHITE,
			.agent = args.agent_2,
			.agent_uid = args.agent_2_uid,
			.logfile = HEX_AGENT_LOGFILE_TEMPLATE,
			.timer = { .tv_sec = args.game_secs, .tv_nsec = 0, },
			.sock_addrlen = sizeof(struct sockaddr_storage),
		},
		.board = board,
	};

	if (!server_init(&state)) {
		errlog("Failed to initialise server state\n");
		exit(EXIT_FAILURE);
	}

	if (!server_spawn_agent(&state, &state.black_agent)) {
		errlog("Failed to spawn black user agent: %s\n", state.black_agent.agent);
		exit(EXIT_FAILURE);
	}

	if (!server_spawn_agent(&state, &state.white_agent)) {
		errlog("Failed to spawn white user agent: %s\n", state.white_agent.agent);
		exit(EXIT_FAILURE);
	}

	struct statistics stats;
	server_run(&state, &stats);

	server_wait_all_agents(&state);

	server_free(&state);

	board_free(board);

	fprintf(stdout,	"agent_1,agent_1_won,agent_1_rounds,agent_1_secs,agent_1_err,agent_1_logfile,agent_2,agent_2_won,agent_2_rounds,agent_2_secs,agent_2_err,agent_2_logfile,\n");
	fprintf(stdout,
		"%s,%i,%u,%f,%s,%s,%s,%i,%u,%f,%s,%s,\n",
		stats.agent_1, stats.agent_1_won, stats.agent_1_rounds, stats.agent_1_secs, hexerrorstr(stats.agent_1_err), state.black_agent.logfile,
		stats.agent_2, stats.agent_2_won, stats.agent_2_rounds, stats.agent_2_secs, hexerrorstr(stats.agent_2_err), state.white_agent.logfile);

	return 0;
}

static u32
try_parse_u32(char *src, s32 base, u32 *out)
{
	char *endptr = NULL;
	u32 result = strtoul(src, &endptr, base);
	if (*endptr || errno)
		return false;

	*out = result;

	return true;
}

static void
parse_args(s32 argc, char **argv)
{
	for (s32 i = 1; i < argc; i++) {
		char *arg = argv[i];

		if (arg[0] != '-') continue;

		switch (arg[1]) {
		case 'a':
			args.agent_1 = argv[++i];
			break;

		case 'b':
			args.agent_2 = argv[++i];
			break;

		case 'u':
			if (arg[2] == 'a' && !try_parse_u32(argv[++i], 10, &args.agent_1_uid)) {
				errlog("-ua takes a positive, unsigned integer argument, was given: '%s'\n",
					argv[i]);
				exit(EXIT_FAILURE);
			} else if (arg[2] == 'b' && !try_parse_u32(argv[++i], 10, &args.agent_2_uid)) {
				errlog("-ub takes a positive, unsigned integer argument, was given: '%s'\n",
					argv[i]);
				exit(EXIT_FAILURE);
			} else if (arg[2] != 'a' && arg[2] != 'b') {
				goto unknown_arg;
			}
			break;

		case 'd': {
			if (!try_parse_u32(argv[++i], 10, &args.board_dimensions)) {
				errlog("-d takes a positive, unsigned integer argument, was given: '%s'\n",
					argv[i]);
				exit(EXIT_FAILURE);
			}
		} break;

		case 's': {
			if (!try_parse_u32(argv[++i], 10, &args.game_secs)) {
				errlog("-s takes a positive, unsigned integer argument, was given: '%s'\n",
					argv[i]);
				exit(EXIT_FAILURE);
			}
		} break;

		case 't': {
			if (!try_parse_u32(argv[++i], 10, &args.thread_limit)) {
				errlog("-t takes a positive, unsigned integer argument, was given: '%s'\n",
					argv[i]);
				exit(EXIT_FAILURE);
			}
		} break;

		case 'm': {
			if (!try_parse_u32(argv[++i], 10, &args.mem_limit_mib)) {
				errlog("-m takes a positive, unsigned integer argument, was given: '%s'\n",
					argv[i]);
				exit(EXIT_FAILURE);
			}
		} break;

		case 'v':
			args.verbose = true;
			break;

		case 'h':
			usage(argv);
			exit(EXIT_SUCCESS);
			break;

		default: {
unknown_arg:
			errlog("[server] Unknown argument: %s\n", &arg[1]);
			usage(argv);
			exit(EXIT_FAILURE);
		} break;
		}
	}
}
