#include "hexes/agent/mcts.h"

extern inline size_t
mcts_node_sizeof(size_t children);

extern inline mcts_node_relptr_t
mcts_node_abs2rel(void *base, struct mcts_node *absptr);

extern inline struct mcts_node *
mcts_node_rel2abs(void *base, mcts_node_relptr_t relptr);

static void
mcts_node_init(struct mcts_node *self, struct mcts_node *parent,
	       enum hex_player player, u8 x, u8 y, size_t children)
{
	assert(self);

	self->parent = mcts_node_abs2rel(self, parent);
	self->player = player;
	self->x = x;
	self->y = y;

	self->wins = self->rave_wins = 0;
	self->plays = self->rave_plays = 0;

	self->children_cap = children;
	self->children_len = 0;
}

static bool
mcts_node_expand(struct mcts_node *self, struct mem_pool *pool, u8 x, u8 y)
{
	assert(self);
	assert(pool);

	struct mcts_node *child = mem_pool_alloc(pool, alignof(struct mcts_node),
						 mcts_node_sizeof(self->children_cap - 1));

	if (!child) {
		dbglog(LOG_WARN, "Failed to allocate child node. Consider compacting memory pool\n");
		return false;
	}

	mcts_node_init(child, self, hexopponent(self->player), x, y, self->children_cap - 1);

	self->children[self->children_len++] = mcts_node_abs2rel(self, child);

	return true;
}

static struct mcts_node *
mcts_node_get_child(struct mcts_node *self, u8 x, u8 y)
{
	assert(self);

	if (!self->children_len) return NULL;

	for (size_t i = 0; i < self->children_cap; i++) {
		struct mcts_node *child = mcts_node_rel2abs(self, self->children[i]);
		if (!child) continue;

		if (child->x == x && child->y == y) return child;
	}

	return NULL;
}

static f32
mcts_node_calc_score(struct mcts_node *self)
{
	assert(self);

	/* MCTS-RAVE formula:
	 * ((1 - beta(n, n')) * (w / n)) + (beta(n, n') * (w' / n')) + (c * sqrt(ln t / n))
	 * ---
	 *  n = number of won playouts for this node
	 *  n' = number of won playouts for this node for a given move
	 *  w = total number of playouts for this node
	 *  w' = total number of playouts for this node for a given move
	 *  c = exploration parameter (sqrt(2), or found experimentally)
	 *  t = total number of playouts for parent node
	 *  beta(n, n') = function close to 1 for small n, and close to 0 for large n
	 */

	/* if this node has not yet been played, return the default maximum value
	 * so that it is picked during expansion
	 */
	if (!self->plays) return INFINITY;

	s64 exploration_rounds = 3000;
	f32 beta = MAX(0.0, (exploration_rounds - self->plays) / (f32) exploration_rounds);
	assert(0.0 <= beta && beta <= 1.0);

	dbglog(LOG_DEBUG, "beta: %lf, wins: %d, rave_wins: %d, plays: %u, rave_plays: %u\n",
			  beta, self->wins, self->rave_wins, self->plays, self->rave_plays);

	struct mcts_node *parent = mcts_node_rel2abs(self, self->parent);
	assert(parent);

	f32 exploration = M_SQRT2 * sqrtf(logf(parent->plays) / (f32) self->plays);

	f32 exploitation = (1 - beta) * ((f32) self->wins / (f32) self->plays);
	assert(-1.0 <= exploitation && exploitation <= 1.0);

	f32 rave_exploitation = beta * ((f32) self->rave_wins / (f32) self->rave_plays);
	assert(-1.0 <= rave_exploitation && rave_exploitation <= 1.0);

	dbglog(LOG_DEBUG, "exploration: %f, exploitation: %f, rave_exploitation: %f\n",
			exploration, exploitation, rave_exploitation);

	return exploration + exploitation + rave_exploitation;
}

static struct mcts_node *
mcts_node_best_child(struct mcts_node *self)
{
	assert(self);

	f32 max_score = -INFINITY;
	struct mcts_node *best_child = NULL;
	for (size_t i = 0; i < self->children_cap; i++) {
		struct mcts_node *child = mcts_node_rel2abs(self, self->children[i]);
		if (!child) continue;

		dbglog(LOG_DEBUG, "Node: {parent=%p, children=%" PRIu8 ", x=%" PRIu32 ", y=%" PRIu32 "}\n",
				mcts_node_rel2abs(child, child->parent), child->children_len, child->x, child->y);

		f32 score = mcts_node_calc_score(child);

		if (score > max_score) {
			max_score = score;
			best_child = child;
		}
	}

	return best_child;
}

bool
agent_mcts_init(struct agent_mcts *self, struct board const *board, struct threadpool *threadpool,
		u32 mem_limit_mib, enum hex_player player)
{
	assert(self);

	self->board = board;
	self->threadpool = threadpool;

	if (!board_init(&self->shadow_board, board->size)) return false;

	size_t align = alignof(struct mcts_node);
	size_t cap = ((mem_limit_mib * MiB) - RESERVED_MEM) & ~(align - 1);

	if (!mem_pool_init(&self->pool, align, cap)) {
		board_free(&self->shadow_board);
		return false;
	}

	size_t moves = board_available_moves(board, NULL);
	self->root = mem_pool_alloc(&self->pool, alignof(struct mcts_node), mcts_node_sizeof(moves));
	mcts_node_init(self->root, NULL, hexopponent(player), 0, 0, moves);

	return true;
}

void
agent_mcts_free(struct agent_mcts *self)
{
	assert(self);

	mem_pool_free(&self->pool);
}

void
agent_mcts_play(struct agent_mcts *self, enum hex_player player, u32 x, u32 y)
{
	assert(self);

	mem_pool_reset(&self->pool);

	size_t moves = board_available_moves(self->board, NULL);
	self->root = mem_pool_alloc(&self->pool, alignof(struct mcts_node), mcts_node_sizeof(moves));
	mcts_node_init(self->root, NULL, player, x, y, moves);

	// TODO: implement tree reuse, if it improves play
	//
	//       one possible issue is children containing stale board states,
	//       leading to potentially invalid moves being generated.

	// TODO: implement tree compaction
	//       one possible issue is the fact that walking the tree to
	//       compact it takes a significant amount of time and potentially
	//       outweighs simply resetting the pool and performing a few more
	//       rounds of MCTS.
	//
	//       another option would be to implement some form of cyclic
	//       memory pool, to allow allocations from memory behind the
	//       current root node, as well as in front of it, but this would
	//       require tracking stale leaf nodes and reclaiming them (hence
	//       transforms into a GC, and thus wastes the benefits of a
	//       simple memory pool)
}

void
agent_mcts_swap(struct agent_mcts *self)
{
	assert(self);

	struct mcts_node old_root = *self->root;

	mem_pool_reset(&self->pool);

	size_t moves = board_available_moves(self->board, NULL);
	self->root = mem_pool_alloc(&self->pool, alignof(struct mcts_node), mcts_node_sizeof(moves));
	mcts_node_init(self->root, NULL, hexopponent(old_root.player), old_root.x, old_root.y, moves);
}

static bool
mcts_search(struct agent_mcts *self, struct timespec timeout);

bool
agent_mcts_next(struct agent_mcts *self, struct timespec timeout, u32 *out_x, u32 *out_y)
{
	assert(self);
	assert(out_x);
	assert(out_y);

	if (!mcts_search(self, timeout)) return false;

	struct mcts_node *root = self->pool.ptr;
	assert(root->children_len);

	u32 max_plays = 0;
	struct mcts_node *best_child = NULL;
	for (size_t i = 0; i < root->children_cap; i++) {
		struct mcts_node *child = mcts_node_rel2abs(root, root->children[i]);
		if (!child) continue;

		if (child->plays > max_plays) {
			max_plays = child->plays;
			best_child = child;
		} else if (child->plays == max_plays && random() % 2) {
			best_child = child;
		}
	}

	assert(best_child);

	*out_x = best_child->x;
	*out_y = best_child->y;

	return true;
}

static bool
mcts_round(struct agent_mcts *self, struct move *moves)
{
	assert(self);

	board_copy(self->board, &self->shadow_board);

	dbglog(LOG_DEBUG, "Starting MCTS round\n");

	/* selection: we walk the mcts tree, picking the child with the highest
	 * mcts-rave score, until we hit a node with unexpanded children
	 */
	struct mcts_node *node = self->root;
	while (node->children_len == node->children_cap) {
		struct mcts_node *child = mcts_node_best_child(node);
		if (!child) break;

		if (!board_play(&self->shadow_board, child->player, child->x, child->y)) {
			dbglog(LOG_WARN, "Failed to play move (%" PRIu32 ", %" PRIu32 ") to shadow board\n", child->x, child->y);
			return false;
		}

		node = child;
	}

	dbglog(LOG_DEBUG, "Selected node {parent=%p, children=%" PRIu8 ", x=%" PRIu32 ", y=%" PRIu32 "} for expansion\n",
			  mcts_node_rel2abs(node, node->parent), node->children_len, node->x, node->y);

	size_t moves_len = board_available_moves(&self->shadow_board, moves);
	shuffle(moves, sizeof *moves, moves_len);

	/* expansion: we expand the chosen node, creating a new child for a
	 * random move
	 */
	enum hex_player winner;
	if (!board_winner(&self->shadow_board, &winner)) {
		struct move move = moves[--moves_len];

		if (!mcts_node_expand(node, &self->pool, move.x, move.y)) {
			dbglog(LOG_WARN, "Failed to expand selected node\n");
			return false;
		}

		struct mcts_node *child = mcts_node_get_child(node, move.x, move.y);
		assert(child);

		if (!board_play(&self->shadow_board, child->player, child->x, child->y)) {
			dbglog(LOG_WARN, "Failed to play move (%" PRIu32 ", %" PRIu32 ") to shadow board\n", child->x, child->y);
			return false;
		}
	}

	dbglog(LOG_DEBUG, "Expanded node {parent=%p, children=%" PRIu8 ", x=%" PRIu32 ", y=%" PRIu32 "}\n",
			  mcts_node_rel2abs(node, node->parent), node->children_len, node->x, node->y);

	/* simulation: we simulate the game using a uniform random walk of the
	 * game state space, until a winner is found
	 */
	enum hex_player player = node->player;
	while (!board_winner(&self->shadow_board, &winner)) {
		struct move move = moves[--moves_len];

		if (!board_play(&self->shadow_board, player, move.x, move.y)) {
			dbglog(LOG_WARN, "Failed to play move (%" PRIu32 ", %" PRIu32 ") to shadow board\n", move.x, move.y);
			return false;
		}

		player = hexopponent(player);
	}

	dbglog(LOG_DEBUG, "Completed playouts for node {parent=%p, children=%" PRIu8 ", x=%" PRIu32 ", y=%" PRIu32 "}\n",
			  mcts_node_rel2abs(node, node->parent), node->children_len, node->x, node->y);

	/* backpropagation: we update the state information in the mcts tree
	 * by walking backwards from the selected node
	 */
	do {
		s32 reward = winner == node->player ? +1 : -1;

		for (size_t i = 0; i < node->children_len; i++) {
			struct mcts_node *child = mcts_node_rel2abs(node, node->children[i]);
			if (!child) continue;

			struct segment *segment = &self->shadow_board.segments[child->y * self->shadow_board.size + child->x];
			if ((enum cell) child->player == segment->occupant) {
				child->rave_plays += 1;
				child->rave_wins += -reward;
			}
		}

		node->plays += 1;
		node->wins += reward;
	} while ((node = mcts_node_rel2abs(node, node->parent)));

	dbglog(LOG_DEBUG, "Completed backpropagation from selected node\n");

	dbglog(LOG_DEBUG, "Completed MCTS round\n");

	return true;
}

static bool
mcts_search(struct agent_mcts *self, struct timespec timeout)
{
	assert(self);

	struct move *moves = alloca(self->board->size * self->board->size * sizeof *moves);

	struct timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);

	u64 end_nanos = TIMESPEC_TO_NANOS(time.tv_sec, time.tv_nsec)
		      + TIMESPEC_TO_NANOS(timeout.tv_sec, timeout.tv_nsec);

	dbglog(LOG_INFO, "Starting MCTS tree search with %" PRIu32 " second timeout\n", timeout.tv_sec);

	size_t rounds = 0;
	while (true) {
		clock_gettime(CLOCK_MONOTONIC, &time);
		if (end_nanos <= TIMESPEC_TO_NANOS(time.tv_sec, time.tv_nsec)) {
			dbglog(LOG_DEBUG, "Search timeout elapsed\n");
			break;
		}

		if (!mcts_round(self, moves)) {
			dbglog(LOG_WARN, "Failed to perform MCTS round %zu\n", rounds + 1);
			break;
		}

		rounds++;
	}

	dbglog(LOG_INFO, "Completed %zu rounds of MCTS\n", rounds);
	dbglog(LOG_INFO, "MCTS node pool occupancy: %zu/%zu bytes allocated\n", self->pool.len, self->pool.cap);

	return true;
}
