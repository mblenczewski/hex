#ifndef HEXES_UTILS_H
#define HEXES_UTILS_H

#include "hexes.h"

inline void
swap(void *restrict lhs, void *restrict rhs, size_t size)
{
	assert(lhs);
	assert(rhs);
	assert(size);

	u8 tmp[size];

	memcpy(tmp, rhs, size);
	memcpy(rhs, lhs, size);
	memcpy(lhs, tmp, size);
}

inline void
shuffle(void *arr, size_t size, size_t len)
{
	assert(arr);
	assert(size);

	for (size_t i = 0; i < len - 2; i++) {
		size_t j = (i + random()) % len;

		swap((u8 *) arr + (i * size),
		     (u8 *) arr + (j * size),
		     size);
	}
}

inline void
difftimespec(struct timespec *restrict lhs, struct timespec *restrict rhs, struct timespec *restrict out)
{
	if (lhs->tv_sec <= rhs->tv_sec && lhs->tv_nsec < rhs->tv_nsec) {
		out->tv_sec = 0;
		out->tv_nsec = 0;
	} else {
		out->tv_sec = lhs->tv_sec - rhs->tv_sec - (lhs->tv_nsec < rhs->tv_nsec);
		out->tv_nsec = lhs->tv_nsec - rhs->tv_nsec + (lhs->tv_nsec < rhs->tv_nsec) * NANOSECS;
	}
}

struct mem_pool {
	void *ptr;
	size_t cap, len;
};

inline bool
mem_pool_init(struct mem_pool *self, size_t align, size_t capacity)
{
	assert(self);
	assert(align);
	assert(align % 2 == 0);
	assert(capacity % align == 0);

	self->ptr = aligned_alloc(align, capacity);
	if (!self->ptr) return false;

	self->cap = capacity;
	self->len = 0;

	return true;
}

inline void
mem_pool_free(struct mem_pool *self)
{
	assert(self);

	free(self->ptr);
}

inline void
mem_pool_reset(struct mem_pool *self)
{
	assert(self);

	self->len = 0;
}

inline void *
mem_pool_alloc(struct mem_pool *self, size_t align, size_t size)
{
	assert(self);
	assert(align);
	assert(align % 2 == 0);

	size_t align_off = align - 1, align_mask = ~align_off;
	size_t aligned_len = (self->len + align_off) & align_mask;

	if (aligned_len + size >= self->cap) return NULL;

	void *ptr = (u8 *) self->ptr + aligned_len;
	self->len = aligned_len + size;

	return ptr;
}

#endif /* HEXES_UTILS_H */
