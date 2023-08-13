#include "hexes/agent/random.h"

bool
agent_random_init(struct agent_random *self, struct board const *board)
{
	assert(self);
	assert(board);

	self->len = board->size * board->size;

	if (!(self->moves = malloc(self->len * sizeof *self->moves))) return false;

	for (u32 j = 0; j < board->size; j++) {
		for (u32 i = 0; i < board->size; i++) {
			size_t idx = j * board->size + i;

			self->moves[idx].x = i;
			self->moves[idx].y = j;
		}
	}

	shuffle(self->moves, sizeof *self->moves, self->len);

	return true;
}

void
agent_random_free(struct agent_random *self)
{
	assert(self);

	free(self->moves);
}

void
agent_random_play(struct agent_random *self, enum hex_player player, u32 x, u32 y)
{
	assert(self);

	(void) player;

	for (size_t i = 0; i < self->len; i++) {
		if (self->moves[i].x == x && self->moves[i].y == y) {
			swap(&self->moves[i], &self->moves[--self->len], sizeof *self->moves);
			break;
		}
	}
}

void
agent_random_swap(struct agent_random *self)
{
	assert(self);

	/* NOTE: this would affect nothing, as we remove moves made by both players
	 */
	(void) self;
}

bool
agent_random_next(struct agent_random *self, struct timespec timeout, u32 *out_x, u32 *out_y)
{
	assert(self);
	assert(out_x);
	assert(out_y);

	(void) timeout;

	if (!self->len) return false;

	size_t idx = --self->len;
	*out_x = self->moves[idx].x;
	*out_y = self->moves[idx].y;

	return true;
}
