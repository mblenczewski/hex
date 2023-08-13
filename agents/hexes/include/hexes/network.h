#ifndef HEXES_NETWORK_H
#define HEXES_NETWORK_H

#include "hexes.h"

struct network {
	int sockfd;
};

bool
network_init(struct network *self, char const *host, char const *port);

void
network_free(struct network *self);

bool
network_send(struct network *self, struct hex_msg const *msg);

bool
network_recv(struct network *self, struct hex_msg *out, enum hex_msg_type *expected, size_t len);

#endif /* HEXES_NETWORK_H */
