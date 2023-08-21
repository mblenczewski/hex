#ifndef HEX_PROTO_H
#define HEX_PROTO_H

#include "hex/types.h"

#ifdef __cplusplus
	#include <cassert>
	#include <cstring>
#else
	#include <assert.h>
	#include <string.h>
#endif /* __cplusplus */

#include <arpa/inet.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum hex_player {
	HEX_PLAYER_BLACK	= 0,
	HEX_PLAYER_WHITE	= 1,
};

inline enum hex_player
hexopponent(enum hex_player player)
{
	switch (player) {
	case HEX_PLAYER_BLACK: return HEX_PLAYER_WHITE;
	case HEX_PLAYER_WHITE: return HEX_PLAYER_BLACK;
	default: assert(false); return HEX_PLAYER_BLACK;
	}
}

inline char const *
hexplayerstr(enum hex_player val)
{
	switch (val) {
	case HEX_PLAYER_BLACK:	return "black";
	case HEX_PLAYER_WHITE:	return "white";
	default:		return "(err)";
	}
}

enum hex_msg_type {
	HEX_MSG_START		= 0,
	HEX_MSG_MOVE		= 1,
	HEX_MSG_SWAP		= 2,
	HEX_MSG_END		= 3,
};

struct hex_msg_start {
	u32 player;
	u32 board_size;
	u32 game_secs;
	u32 thread_limit;
	u32 mem_limit_mib;
};

struct hex_msg_move {
	u32 board_x;
	u32 board_y;
};

struct hex_msg_end {
	u32 winner;
};

union hex_msg_data {
	struct hex_msg_start start;
	struct hex_msg_move move;
	struct hex_msg_end end;
};

struct hex_msg {
	u32 type;
	union hex_msg_data data;
};

#define HEX_MSG_SZ 32

inline b32
#ifdef __cplusplus
hex_msg_try_serialise(struct hex_msg const *msg, u8 (&out)[HEX_MSG_SZ])
#else
hex_msg_try_serialise(struct hex_msg const *msg, u8 out[static HEX_MSG_SZ])
#endif
{
	assert(msg);
	assert(out);

	u32 *bufp = (u32 *) out;

	*bufp++ = htonl(msg->type);

	switch (msg->type) {
	case HEX_MSG_START:
		*bufp++ = htonl(msg->data.start.player);
		*bufp++ = htonl(msg->data.start.board_size);
		*bufp++ = htonl(msg->data.start.game_secs);
		*bufp++ = htonl(msg->data.start.thread_limit);
		*bufp++ = htonl(msg->data.start.mem_limit_mib);
		break;

	case HEX_MSG_MOVE:
		*bufp++ = htonl(msg->data.move.board_x);
		*bufp++ = htonl(msg->data.move.board_y);
		break;

	case HEX_MSG_SWAP:
		break;

	case HEX_MSG_END:
		*bufp++ = htonl(msg->data.end.winner);
		break;
	}

	/* zero out remaining all message bytes */
	u8 *remaining = (u8 *) bufp;
	assert(remaining < out + HEX_MSG_SZ);
	memset(remaining, 0, (out + HEX_MSG_SZ) - remaining);

	return true;
}

inline b32
#ifdef __cplusplus
hex_msg_try_deserialise(u8 (&buf)[HEX_MSG_SZ], struct hex_msg *out)
#else
hex_msg_try_deserialise(u8 buf[static HEX_MSG_SZ], struct hex_msg *out)
#endif
{
	assert(buf);
	assert(out);

	u32 *bufp = (u32 *) buf;

	struct hex_msg msg;
	msg.type = ntohl(*bufp++);

	switch (msg.type) {
	case HEX_MSG_START:
		msg.data.start.player = ntohl(*bufp++);
		msg.data.start.board_size = ntohl(*bufp++);
		msg.data.start.game_secs = ntohl(*bufp++);
		msg.data.start.thread_limit = ntohl(*bufp++);
		msg.data.start.mem_limit_mib = ntohl(*bufp++);
		break;

	case HEX_MSG_MOVE:
		msg.data.move.board_x = ntohl(*bufp++);
		msg.data.move.board_y = ntohl(*bufp++);
		break;

	case HEX_MSG_SWAP:
		break;

	case HEX_MSG_END:
		msg.data.end.winner = ntohl(*bufp++);
		break;

	default:
		return false;
	}

	*out = msg;

	return true;
}

#ifdef __cplusplus
};
#endif /* __cplusplus */

#endif /* HEX_PROTO_H */
