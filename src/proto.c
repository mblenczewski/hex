#include "hex.h"

extern inline b32
hex_msg_try_serialise(struct hex_msg const *msg, u8 out[static HEX_MSG_SZ]);

extern inline b32
hex_msg_try_deserialise(u8 buf[static HEX_MSG_SZ], struct hex_msg *out);
