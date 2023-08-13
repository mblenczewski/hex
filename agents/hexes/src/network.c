#include "hexes/network.h"

bool
network_init(struct network *self, char const *host, char const *port)
{
	assert(self);
	assert(host);
	assert(port);

	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	}, *addrinfo, *ptr;

	int res;
	if ((res = getaddrinfo(host, port, &hints, &addrinfo))) {
		dbglog(LOG_ERROR, "Failed to get address info: %s\n", gai_strerror(res));
		return false;
	}

	for (ptr = addrinfo; ptr; ptr = ptr->ai_next) {
		self->sockfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (self->sockfd == -1) continue;
		if (connect(self->sockfd, ptr->ai_addr, ptr->ai_addrlen) != -1) break;
		close(self->sockfd);
	}

	freeaddrinfo(addrinfo);

	if (!ptr) {
		dbglog(LOG_ERROR, "Failed to connect to %s:%s\n", host, port);
		return false;
	}

	return true;
}

void
network_free(struct network *self)
{
	assert(self);

	close(self->sockfd);
}

bool
network_send(struct network *self, struct hex_msg const *msg)
{
	assert(self);
	assert(msg);

	u8 buf[HEX_MSG_SZ];

	if (!hex_msg_try_serialise(msg, buf)) return false;

	size_t count = 0;
	do {
		ssize_t curr = send(self->sockfd, buf + count, ARRLEN(buf) - count, 0);
		if (curr <= 0) return false; /* error or socket shutdown */
		count += curr;
	} while (count < ARRLEN(buf));

	return true;
}

bool
network_recv(struct network *self, struct hex_msg *out, enum hex_msg_type *expected, size_t len)
{
	assert(self);
	assert(out);
	assert(expected);
	assert(len);

	u8 buf[HEX_MSG_SZ];

	size_t count = 0;
	do {
		ssize_t curr = recv(self->sockfd, buf + count, ARRLEN(buf) - count, 0);
		if (curr <= 0) return false; /* error or socket shutdown */
		count += curr;
	} while (count < ARRLEN(buf));

	struct hex_msg msg;
	if (!hex_msg_try_deserialise(buf, &msg)) return false;

	for (size_t i = 0; i < len; i++) {
		if (msg.type == expected[i]) {
			*out = msg;
			return true;
		}
	}

	return false;
}

extern inline b32
hex_msg_try_serialise(struct hex_msg const *msg, u8 out[static HEX_MSG_SZ]);

extern inline b32
hex_msg_try_deserialise(u8 buf[static HEX_MSG_SZ], struct hex_msg *out);
