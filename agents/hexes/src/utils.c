#include "hexes/utils.h"

extern inline void
swap(void *restrict lhs, void *restrict rhs, size_t size);

extern inline void
shuffle(void *arr, size_t size, size_t len);

extern inline void
difftimespec(struct timespec *restrict lhs, struct timespec *restrict rhs, struct timespec *restrict out);

extern inline bool
mem_pool_init(struct mem_pool *self, size_t align, size_t capacity);

extern inline void
mem_pool_free(struct mem_pool *self);

extern inline void
mem_pool_reset(struct mem_pool *self);

extern inline void *
mem_pool_alloc(struct mem_pool *self, size_t align, size_t size);

extern inline enum hex_player
hexopponent(enum hex_player player);

extern inline char const *
hexplayerstr(enum hex_player val);
