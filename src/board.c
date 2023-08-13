#include "hex.h"

extern inline s16
board_segment_abs2rel(struct board_segment *base, struct board_segment *absptr);

extern inline struct board_segment *
board_segment_rel2abs(struct board_segment *base, s16 relptr);

struct board_segment *
board_segment_root(struct board_segment *self)
{
	assert(self);

	struct board_segment *parent, *grandparent;
	while ((parent = board_segment_rel2abs(self, self->parent_relptr))) {
		grandparent = board_segment_rel2abs(parent, parent->parent_relptr);
		if (!grandparent) return parent;

		/* compress the path from self->parent->grandparent to
		 * self->grandparent for faster future traversal
		 */
		self->parent_relptr = board_segment_abs2rel(self, grandparent);

		self = grandparent;
	}

	return self;
}

void
board_segment_merge(struct board_segment *restrict self, struct board_segment *restrict elem)
{
	assert(self);
	assert(elem);

	struct board_segment *self_root = board_segment_root(self);
	struct board_segment *elem_root = board_segment_root(elem);

	if (self_root == elem_root) return; /* NOTE: already merged */

	if (self_root->rank < elem_root->rank) {
		self_root->parent_relptr = board_segment_abs2rel(self_root, elem_root);
	} else if (self_root->rank > elem_root->rank) {
		elem_root->parent_relptr = board_segment_abs2rel(elem_root, self_root);
	} else {
		self_root->parent_relptr = board_segment_abs2rel(self_root, elem_root);
		elem_root->rank++;
	}
}

b32
board_segment_joined(struct board_segment *self, struct board_segment *elem)
{
	return board_segment_root(self) == board_segment_root(elem);
}

#define NEIGHBOURS_COUNT 6

static s8 neighbour_dx[NEIGHBOURS_COUNT] = { -1, -1, 0, 0, +1, +1, };
static s8 neighbour_dy[NEIGHBOURS_COUNT] = { 0, +1, -1, +1, -1, 0, };

static inline struct board_segment *
board_get_segment(struct board_state *self, s32 x, s32 y)
{
	assert(self);

	if (-1 < x && x < (s32) self->size && -1 < y && y < (s32) self->size)
		return &self->segments[y * self->size + x];

	return NULL;
}

static size_t
board_neighbours(struct board_state *self, s32 x, s32 y, struct board_segment *buf[static NEIGHBOURS_COUNT])
{
	assert(self);
	assert(buf);

	size_t count = 0;

	for (size_t i = 0; i < NEIGHBOURS_COUNT; i++) {
		s32 px = x + neighbour_dx[i], py = y + neighbour_dy[i];

		struct board_segment *seg;
		if ((seg = board_get_segment(self, px, py)))
			buf[count++] = seg;
	}

	return count;
}

struct board_state *
board_alloc(size_t size)
{
	struct board_state *board;
	size_t segments = size * size;
	size_t sz = sizeof *board + segments * sizeof *board->segments;

	if (!(board = malloc(sz))) return NULL;

	memset(board, 0, sz);

	board->size = size;

	return board;
}

void
board_free(struct board_state *self)
{
	assert(self);

	free(self);
}

void
board_print(struct board_state *self)
{
	assert(self);

	for (size_t y = 0; y < self->size; y++) {
		for (size_t k = 0; k < y; k++)
			dbglog("  ");

		struct board_segment *seg;
		for (size_t x = 0; x < self->size; x++) {
			seg = board_get_segment(self, x, y);

			switch (seg->cell) {
			case CELL_EMPTY: dbglog(". "); break;
			case CELL_BLACK: dbglog("B "); break;
			case CELL_WHITE: dbglog("W "); break;
			}
		}

		dbglog("\n");
	}
}

b32
board_play(struct board_state *self, enum hex_player player, s32 x, s32 y)
{
	assert(self);

	struct board_segment *seg = board_get_segment(self, x, y);
	if (!seg) {
		dbglog("[server] Agent %u played invalid move: (%" PRIi32 ", %" PRIi32 "); out of bounds\n",
			player, x, y);

		return false;
	}

	if (seg->cell != CELL_EMPTY) {
		char *cell_strs[] = {
			[CELL_EMPTY] = "empty", [CELL_BLACK] = "black", [CELL_WHITE] = "white",
		};

		dbglog("[server] Agent %u played invalid move: (%" PRIi32 ", %" PRIi32 "); previously occupied by %s\n",
			player, x, y, cell_strs[seg->cell]);

		return false;
	}

	switch (player) {
	case HEX_PLAYER_BLACK: seg->cell = CELL_BLACK; break;
	case HEX_PLAYER_WHITE: seg->cell = CELL_WHITE; break;
	}

	/* NOTE: handle case where player plays a cell connected to their
	 * relevant edged (source/sink)
	 */
	if (player == HEX_PLAYER_BLACK) {
		if (x == 0) {
			board_segment_merge(seg, &self->black_source);
		} else if (x == (s32) self->size - 1) {
			board_segment_merge(seg, &self->black_sink);
		}
	} else if (player == HEX_PLAYER_WHITE) {
		if (y == 0) {
			board_segment_merge(seg, &self->white_source);
		} else if (y == (s32) self->size - 1) {
			board_segment_merge(seg, &self->white_sink);
		}
	}

	/* NOTE: merge the played cell with all of its neighbours played by
	 * the same player
	 */
	struct board_segment *neighbours[NEIGHBOURS_COUNT];
	size_t neighbours_count = board_neighbours(self, x, y, neighbours);

	for (size_t i = 0; i < neighbours_count; i++) {
		if (neighbours[i]->cell == seg->cell)
			board_segment_merge(seg, neighbours[i]);
	}

	return true;
}

void
board_swap(struct board_state *self)
{
	assert(self);

	for (size_t i = 0; i < self->size * self->size; i++) {
		struct board_segment *seg = &self->segments[i];

		s32 x = i % self->size;
		s32 y = i / self->size;

		switch (seg->cell) {
		case CELL_EMPTY:
			break; /* skip empty cells */

		default: {
			u8 old_cell_value = seg->cell;

			seg->cell = CELL_EMPTY;
			seg->parent_relptr = RELPTR_NULL;
			seg->rank = 0;

			switch (old_cell_value) {
			case CELL_BLACK: board_play(self, HEX_PLAYER_WHITE, x, y); break;
			case CELL_WHITE: board_play(self, HEX_PLAYER_BLACK, x, y); break;
			}
		} break;
		}
	}
}

b32
board_completed(struct board_state *self, enum hex_player *winner)
{
	assert(self);
	assert(winner);

	if (board_segment_joined(&self->black_source, &self->black_sink)) {
		*winner = HEX_PLAYER_BLACK;
		return true;
	}

	if (board_segment_joined(&self->white_source, &self->white_sink)) {
		*winner = HEX_PLAYER_WHITE;
		return true;
	}

	return false;
}
