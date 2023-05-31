/**
 * The base includes standard libraries and useful macros and types to use
 * by all other files
 */

#ifndef INCLUDED_BASE_H
#define INCLUDED_BASE_H

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX(a, b) ({ \
	__auto_type _a = (a); \
	__auto_type _b = (b); \
	_a > _b ? _a : _b; \
})
#define MIN(a, b) ({ \
	__auto_type _a = (a); \
	__auto_type _b = (b); \
	_a < _b ? _a : _b; \
})
#define ABS(a) ({ \
	__auto_type _a = (a); \
	_a < 0 ? -_a : _a; \
})
#define SAFE_ADD(a, b) ({ \
	__auto_type _a = (a); \
	__auto_type _b = (b); \
	typeof(_a) _c; \
	if(__builtin_add_overflow(_a, _b, &_c)) \
		_c = _a > 0 && _b > 0 ? INT_MAX : INT_MIN; \
	_c; \
})
#define SAFE_MUL(a, b) ({ \
	__auto_type _a = (a); \
	__auto_type _b = (b); \
	typeof(_a) _c; \
	if(__builtin_mul_overflow(_a, _b, &_c)) \
		_c = (a > 0 && b > 0) || (a < 0 && b < 0) ? INT_MAX : INT_MIN; \
	_c; \
})

#define ARRLEN(a) (sizeof(a)/sizeof*(a))

typedef int8_t I8;
typedef int16_t I16;
typedef int32_t I32;
typedef int64_t I64;
typedef uint8_t U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;

void *const_alloc(const void *data, U32 sz);

#endif
