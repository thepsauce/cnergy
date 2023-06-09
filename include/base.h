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
#include <stddef.h>
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
#define TYPE_MIN(t) _Generic((t), \
	ptrdiff_t: PTRDIFF_MIN, \
	default: 0)
#define TYPE_MAX(t) _Generic((t), \
	ptrdiff_t: PTRDIFF_MAX, \
	default: 0)
#define SAFE_ADD(a, b) ({ \
	__auto_type _a = (a); \
	__auto_type _b = (b); \
	typeof(_a) _c; \
	if(__builtin_add_overflow(_a, _b, &_c)) { \
		__auto_type _min = TYPE_MIN(_c); \
		__auto_type _max = TYPE_MAX(_c); \
		assert(_max && "invalid type, you must add it to the index in include/base.h TYPE_MIN/TYPE_MAX"); \
		_c = _a > 0 && _b > 0 ? _max : _min; \
	} \
	_c; \
})
#define SAFE_MUL(a, b) ({ \
	__auto_type _a = (a); \
	__auto_type _b = (b); \
	typeof(_a) _c; \
	if(__builtin_mul_overflow(_a, _b, &_c)) { \
		__auto_type _min = TYPE_MIN(_c); \
		__auto_type _max = TYPE_MAX(_c); \
		assert(_max && "invalid type, you must add it to the index in include/base.h TYPE_MIN/TYPE_MAX"); \
		_c = (a > 0 && b > 0) || (a < 0 && b < 0) ? _max : _min; \
	} \
	_c; \
})

#define ARRLEN(a) (sizeof(a)/sizeof*(a))

#endif
