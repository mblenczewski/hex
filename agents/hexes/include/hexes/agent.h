#ifndef HEXES_AGENT_H
#define HEXES_AGENT_H

#include "hexes.h"

#include "hexes/board.h"
#include "hexes/threadpool.h"

#include "hexes/agent/random.h"
#include "hexes/agent/mcts.h"

enum agent_type {
	AGENT_RANDOM,
	AGENT_MCTS,
};

struct agent {
	enum agent_type type;
	union {
		struct agent_random random;
		struct agent_mcts mcts;
	} backend;
};

bool
agent_init(struct agent *self, enum agent_type type, struct board const *board,
	   struct threadpool *threadpool, u32 mem_limit_mib, enum hex_player player);

void
agent_free(struct agent *self);

void
agent_play(struct agent *self, enum hex_player player, u32 x, u32 y);

void
agent_swap(struct agent *self);

bool
agent_next(struct agent *self, struct timespec timeout, u32 *out_x, u32 *out_y);

#endif /* HEXES_AGENT_H */
