#ifndef HEXES_AGENT_MCTS_H
#define HEXES_AGENT_MCTS_H

#include "hexes.h"

#include "hexes/board.h"
#include "hexes/threadpool.h"
#include "hexes/utils.h"

#define RESERVED_MEM (MiB)

typedef s64 mcts_node_relptr_t;

struct mcts_node {
	mcts_node_relptr_t parent;
	enum hex_player player;
	u8 x, y;

	s32 wins, rave_wins;
	u32 plays, rave_plays;

	u16 children_cap, children_len;
	mcts_node_relptr_t children[];
};

inline size_t
mcts_node_sizeof(size_t children)
{
	return sizeof(struct mcts_node) + children * sizeof(mcts_node_relptr_t);
}

inline mcts_node_relptr_t
mcts_node_abs2rel(void *base, struct mcts_node *absptr)
{
	return RELPTR_ABS2REL(mcts_node_relptr_t, base, absptr);
}

inline struct mcts_node *
mcts_node_rel2abs(void *base, mcts_node_relptr_t relptr)
{
	return RELPTR_REL2ABS(struct mcts_node *, mcts_node_relptr_t, base, relptr);
}

struct agent_mcts {
	struct board const *board;
	struct threadpool *threadpool;

	struct board shadow_board;

	struct mem_pool pool;
	struct mcts_node *root;
};

bool
agent_mcts_init(struct agent_mcts *self, struct board const *board, struct threadpool *threadpool,
		u32 mem_limit_mib, enum hex_player player);

void
agent_mcts_free(struct agent_mcts *self);

void
agent_mcts_play(struct agent_mcts *self, enum hex_player player, u32 x, u32 y);

void
agent_mcts_swap(struct agent_mcts *self);

bool
agent_mcts_next(struct agent_mcts *self, struct timespec timeout, u32 *out_x, u32 *out_y);


#endif /* HEXES_AGENT_MCTS_H */
