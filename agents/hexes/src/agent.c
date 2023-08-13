#include "hexes/agent.h"

bool
agent_init(struct agent *self, enum agent_type type, struct board const *board,
	   struct threadpool *threadpool, u32 mem_limit_mib, enum hex_player player)
{
	assert(self);

	self->type = type;

	(void) player;

	switch (type) {
	case AGENT_RANDOM:
		return agent_random_init(&self->backend.random, board);

	case AGENT_MCTS:
		return agent_mcts_init(&self->backend.mcts, board, threadpool, mem_limit_mib, player);
	}

	return false;
}

void
agent_free(struct agent *self)
{
	assert(self);

	switch (self->type) {
	case AGENT_RANDOM:	agent_random_free(&self->backend.random); break;
	case AGENT_MCTS:	agent_mcts_free(&self->backend.mcts); break;
	}
}

void
agent_play(struct agent *self, enum hex_player player, u32 x, u32 y)
{
	assert(self);

	switch (self->type) {
	case AGENT_RANDOM:	agent_random_play(&self->backend.random, player, x, y); break;
	case AGENT_MCTS:	agent_mcts_play(&self->backend.mcts, player, x, y); break;
	}
}

void
agent_swap(struct agent *self)
{
	assert(self);

	switch (self->type) {
	case AGENT_RANDOM:	agent_random_swap(&self->backend.random); break;
	case AGENT_MCTS:	agent_mcts_swap(&self->backend.mcts); break;
	}
}

bool
agent_next(struct agent *self, struct timespec timeout, u32 *out_x, u32 *out_y)
{
	assert(self);
	assert(out_x);
	assert(out_y);

	switch (self->type) {
	case AGENT_RANDOM:	return agent_random_next(&self->backend.random, timeout, out_x, out_y);
	case AGENT_MCTS:	return agent_mcts_next(&self->backend.mcts, timeout, out_x, out_y);
	}

	return false;
}
