#ifndef HEXES_AGENT_RANDOM_H
#define HEXES_AGENT_RANDOM_H

#include "hexes.h"

#include "hexes/board.h"
#include "hexes/utils.h"

struct agent_random {
	size_t len;
	struct move *moves;
};

bool
agent_random_init(struct agent_random *self, struct board const *board);

void
agent_random_free(struct agent_random *self);

void
agent_random_play(struct agent_random *self, enum hex_player player, u32 x, u32 y);

void
agent_random_swap(struct agent_random *self);

bool
agent_random_next(struct agent_random *self, struct timespec timeout, u32 *out_x, u32 *out_y);

#endif /* HEXES_AGENT_RANDOM_H */
