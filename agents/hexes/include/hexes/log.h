#ifndef HEXES_LOG_H
#define HEXES_LOG_H

#include "hexes.h"

enum log_level {
	LOG_ERROR,
	LOG_WARN,
	LOG_INFO,
	LOG_DEBUG,
};

inline void
dbglog(enum log_level log_level, char const *fmt, ...)
{
	if (opts.log_level < log_level) return;

	switch (log_level) {
	case LOG_ERROR:	fputs("[ERROR]", stderr); break;
	case LOG_WARN:	fputs("[WARN] ", stderr); break;
	case LOG_INFO:	fputs("[INFO] ", stderr); break;
	case LOG_DEBUG:	fputs("[DEBUG]", stderr); break;
	}

	fputc(' ', stderr);

	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#endif /* HEXES_LOG_H */
