#ifndef HEXES_THREADPOOL_H
#define HEXES_THREADPOOL_H

#include "hexes.h"

struct threadpool {
	u32 threads;
};

bool
threadpool_init(struct threadpool *self, u32 threads);

void
threadpool_free(struct threadpool *self);

#endif /* HEXES_THREADPOOL_H */
