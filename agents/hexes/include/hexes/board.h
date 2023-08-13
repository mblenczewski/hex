#ifndef HEXES_BOARD_H
#define HEXES_BOARD_H

#include "hexes.h"

enum cell {
	CELL_BLACK = HEX_PLAYER_BLACK,
	CELL_WHITE = HEX_PLAYER_WHITE,
	CELL_EMPTY,
};

typedef s16 segment_relptr_t;

struct segment {
	enum cell occupant;
	u32 rank;
	segment_relptr_t parent;
};

inline segment_relptr_t
segment_abs2rel(void const *base, struct segment *absptr)
{
	return RELPTR_ABS2REL(segment_relptr_t, base, absptr);
}

inline struct segment *
segment_rel2abs(void const *base, segment_relptr_t relptr)
{
	return RELPTR_REL2ABS(struct segment *, segment_relptr_t, base, relptr);
}

enum board_edges {
	BLACK_SOURCE,
	BLACK_SINK,
	WHITE_SOURCE,
	WHITE_SINK,
	_BOARD_EDGE_COUNT,
};

struct board {
	u32 size;
	struct segment *segments;
};

inline struct segment *
board_black_source(struct board *self)
{
	return &self->segments[self->size * self->size + BLACK_SOURCE];
}

inline struct segment *
board_black_sink(struct board *self)
{
	return &self->segments[self->size * self->size + BLACK_SINK];
}

inline struct segment *
board_white_source(struct board *self)
{
	return &self->segments[self->size * self->size + WHITE_SOURCE];
}

inline struct segment *
board_white_sink(struct board *self)
{
	return &self->segments[self->size * self->size + WHITE_SINK];
}

struct move {
	u8 x, y;
};

bool
board_init(struct board *self, u32 size);

void
board_free(struct board *self);

void
board_copy(struct board const *restrict self, struct board *restrict other);

bool
board_play(struct board *self, enum hex_player player, u32 x, u32 y);

void
board_swap(struct board *self);

size_t
board_available_moves(struct board const *self, struct move *buf);

bool
board_winner(struct board *self, enum hex_player *out);

#endif /* HEXES_BOARD_H */
