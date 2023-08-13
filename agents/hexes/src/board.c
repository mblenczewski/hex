#include "hexes/board.h"

#define NEIGHBOUR_COUNT 6

extern inline segment_relptr_t
segment_abs2rel(void const *base, struct segment *absptr);

extern inline struct segment *
segment_rel2abs(void const *base, segment_relptr_t relptr);

static s8 neighbour_dx[NEIGHBOUR_COUNT] = { -1, -1, 0, 0, +1, +1, };
static s8 neighbour_dy[NEIGHBOUR_COUNT] = { 0, +1, -1, +1, -1, 0, };

struct segment *
segment_root(struct segment *self)
{
	assert(self);

	struct segment *parent, *grandparent;
	while ((parent = segment_rel2abs(self, self->parent))) {
		if (!(grandparent = segment_rel2abs(parent, parent->parent)))
			return parent;

		self->parent = segment_abs2rel(self, grandparent);

		self = grandparent;
	}

	return self;
}

bool
segment_merge(struct segment *self, struct segment *elem)
{
	assert(self);
	assert(elem);

	struct segment *self_root = segment_root(self);
	struct segment *elem_root = segment_root(elem);

	if (self_root == elem_root) return false;

	if (self_root->rank <= elem_root->rank) {
		self_root->parent = segment_abs2rel(self_root, elem_root);
	} else if (self_root->rank > elem_root->rank) {
		elem_root->parent = segment_abs2rel(elem_root, self_root);
	}

	/* disambiguate between self and element ranks */
	if (self_root->rank == elem_root->rank) elem_root->rank++;

	return true;
}

extern inline struct segment *
board_black_source(struct board *self);

extern inline struct segment *
board_black_sink(struct board *self);

extern inline struct segment *
board_white_source(struct board *self);

extern inline struct segment *
board_white_sink(struct board *self);

bool
board_init(struct board *self, u32 size)
{
	assert(self);

	self->size = size;

	size_t segments = (size * size) + _BOARD_EDGE_COUNT;
	if (!(self->segments = malloc(segments * sizeof *self->segments)))
		return false;

	for (size_t i = 0; i < segments; i++) {
		struct segment *segment = &self->segments[i];

		segment->occupant = CELL_EMPTY;
		segment->rank = 0;
		segment->parent = RELPTR_NULL;
	}

	struct segment *black_source = board_black_source(self);
	struct segment *black_sink = board_black_sink(self);
	struct segment *white_source = board_white_source(self);
	struct segment *white_sink = board_white_sink(self);

	black_source->occupant = black_sink->occupant = CELL_BLACK;
	white_source->occupant = white_sink->occupant = CELL_WHITE;

	return true;
}

void
board_free(struct board *self)
{
	assert(self);

	free(self->segments);
}

void
board_copy(struct board const *restrict self, struct board *restrict other)
{
	assert(self);
	assert(other);

	assert(self->size == other->size);

	size_t segments = (self->size * self->size) + _BOARD_EDGE_COUNT;
	memcpy(other->segments, self->segments, segments * sizeof *self->segments);
}

bool
board_play(struct board *self, enum hex_player player, u32 x, u32 y)
{
	assert(self);

	struct segment *segment = &self->segments[y * self->size + x];

	if (segment->occupant != CELL_EMPTY) return false;

	segment->occupant = (enum cell) player;

	/* handle connection to source/sink for given player at edge of board
	 */
	if (player == HEX_PLAYER_BLACK) {
		if (x == 0)
			segment_merge(board_black_source(self), segment);
		else if (x == self->size - 1)
			segment_merge(board_black_sink(self), segment);
	} else if (player == HEX_PLAYER_WHITE) {
		if (y == 0)
			segment_merge(board_white_source(self), segment);
		else if (y == self->size - 1)
			segment_merge(board_white_sink(self), segment);
	}

	/* handle connecting to neighbouring segments with same occupant
	 */
	for (size_t i = 0; i < NEIGHBOUR_COUNT; i++) {
		s64 px = x + neighbour_dx[i];
		s64 py = y + neighbour_dy[i];

		if (0 <= px && px < self->size && 0 <= py && py < self->size) {
			struct segment *neighbour = &self->segments[py * self->size + px];

			if (segment->occupant == neighbour->occupant)
				segment_merge(segment, neighbour);
		}
	}

	return true;
}

void
board_swap(struct board *self)
{
	assert(self);

	for (u32 j = 0; j < self->size; j++) {
		for (u32 i = 0; i < self->size; i++) {
			struct segment *segment = &self->segments[j * self->size + i];

			switch (segment->occupant) {
			case CELL_BLACK:
				segment->occupant = CELL_EMPTY;
				board_play(self, HEX_PLAYER_WHITE, i, j);
				break;

			case CELL_WHITE:
				segment->occupant = CELL_EMPTY;
				board_play(self, HEX_PLAYER_BLACK, i, j);
				break;

			default: break;
			}
		}
	}
}

size_t
board_available_moves(struct board const *self, struct move *buf)
{
	assert(self);

	size_t idx = 0;
	for (u32 j = 0; j < self->size; j++) {
		for (u32 i = 0; i < self->size; i++) {
			if (self->segments[j * self->size + i].occupant == CELL_EMPTY) {
				if (buf) {
					buf[idx].x = i;
					buf[idx].y = j;
				}

				idx++;
			}
		}
	}

	return idx;
}

bool
board_winner(struct board *self, enum hex_player *out)
{
	assert(self);

	if (segment_root(board_black_source(self)) == segment_root(board_black_sink(self))) {
		*out = HEX_PLAYER_BLACK;
		return true;
	} else if (segment_root(board_white_source(self)) == segment_root(board_white_sink(self))) {
		*out = HEX_PLAYER_WHITE;
		return true;
	}

	return false;
}
