#ifndef HEX_TYPES_H
#define HEX_TYPES_H

#ifdef __cplusplus
	#include <cinttypes>
	#include <cstdbool>
	#include <cstddef>
	#include <cstdint>
#else
	#include <inttypes.h>
	#include <stdbool.h>
	#include <stddef.h>
	#include <stdint.h>
#endif /* __cplusplus */

typedef int32_t b32;

typedef unsigned char c8;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef float f32;
typedef double f64;

#define ARRLEN(arr) (sizeof (arr) / sizeof (arr)[0])

#define MIN(a, b) ((a) > (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))

#define RELPTR_NULL (0)

#define _RELPTR_MASK(ty_relptr) ((ty_relptr)1 << ((sizeof(ty_relptr) * 8) - 1))
#define _RELPTR_ENC(ty_relptr, ptroff) \
	((ty_relptr)((ptroff) ^ _RELPTR_MASK(ty_relptr)))
#define _RELPTR_DEC(ty_relptr, relptr) \
	((ty_relptr)((relptr) ^ _RELPTR_MASK(ty_relptr)))

#define RELPTR_ABS2REL(ty_relptr, base, absptr) \
	((absptr) \
	 ? _RELPTR_ENC(ty_relptr, (u8 *) absptr - (u8 *) base) \
	 : RELPTR_NULL)

#define RELPTR_REL2ABS(ty_absptr, ty_relptr, base, relptr) \
	((relptr) \
	 ? ((ty_absptr)((u8 *) base + _RELPTR_DEC(ty_relptr, relptr))) \
	 : NULL)

#define NANOSECS (1000000000ULL)

#define TIMESPEC_TO_NANOS(sec, nsec) (((u64) (sec) * NANOSECS) + (nsec))

#define KiB (1024ULL)
#define MiB (1024ULL * KiB)
#define GiB (1024ULL * MiB)

#endif /* HEX_TYPES_H */
