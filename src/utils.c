#include "hex.h"

extern inline char const *
hexerrorstr(enum hex_error val);

extern inline void
errlog(char *fmt, ...);

extern inline void
dbglog(char *fmt, ...);

extern inline void
difftimespec(struct timespec *restrict lhs, struct timespec *restrict rhs, struct timespec *restrict out);

extern inline char const *
hexplayerstr(enum hex_player val);
