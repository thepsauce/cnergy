#include "cnergy.h"

struct memptr {
	U16 off;
	U16 sz;
};

struct mempool {
	U8 data[8 * 1024]; // 8 * pow(2, 10)
	struct memptr *ptrs;
	U32 nPtrs;
	struct mempool *next;
};

static struct mempool internal[8] = {
	{ .next = internal + 1 },
	{ .next = internal + 2 },
	{ .next = internal + 3 },
	{ .next = internal + 4 },
	{ .next = internal + 5 },
	{ .next = internal + 6 },
	{ .next = internal + 7 },
	{ .next = NULL },
};

static struct mempool *first = internal;

static inline struct memptr *
insertptr(struct mempool *pool, U32 off, U32 sz)
{
	struct memptr *newPtrs;

	off /= 4;
	sz /= 4;
	for(U32 i = 0; i < pool->nPtrs; i++) {
		const U32 l = pool->ptrs[i].off;
		const U32 r = l + pool->ptrs[i].sz;
		if(off >= l && off < r) {
			if(off == l)
				return pool->ptrs + i; // pointer already exists
			newPtrs = safe_realloc(pool->ptrs, sizeof(*pool->ptrs) * (pool->nPtrs + 1));
			if(!newPtrs)
				return NULL;
			pool->ptrs = newPtrs;
			i++;
			memmove(pool->ptrs + i + 1, pool->ptrs + i, sizeof(*pool->ptrs) * (pool->nPtrs - i));
			pool->ptrs[i] = (struct memptr) { .off = off, .sz = sz };
			pool->nPtrs++;
			return pool->ptrs + i;
		}
	}
	return NULL;
}

void *
const_alloc(const void *data, U32 szData)
{
	struct mempool *pool;
	struct memptr *newPtrs;
	const U32 szAligned = (szData + sizeof(U32) - 1) & ~(sizeof(U32) - 1);
	for(pool = first; pool; pool = pool->next) {
		if(!pool->nPtrs)
			break;
		for(U32 *d = (U32*) pool->data; d != (U32*) (pool->data + sizeof(pool->data) - szAligned); d++) {
			if(!memcmp(d, data, szData)) {
				struct memptr *const ptr = insertptr(pool, (U8*) d - pool->data, szAligned);
				if(!ptr)
					return NULL;
				memcpy(d, data, szData);
				return d;
			}
		}
	}
	for(pool = first; pool; pool = pool->next) {
		const U32 off = !pool->nPtrs ? 0 : pool->ptrs[pool->nPtrs - 1].off + pool->ptrs[pool->nPtrs - 1].sz; 
		if(off + szAligned / sizeof(U32) <= sizeof(pool->data) / sizeof(U32)) {
			newPtrs = safe_realloc(pool->ptrs, sizeof(*pool->ptrs) * (pool->nPtrs + 1));
			if(!newPtrs)
				return NULL;
			pool->ptrs = newPtrs;
			pool->ptrs[pool->nPtrs++] = (struct memptr) { .off = off, .sz = szAligned / sizeof(U32) };
			void *const d = (U32*) pool->data + off;
			memcpy(d, data, szData);
			return d;
		}
	}
	pool = pool->next = safe_alloc(sizeof(*pool->next));
	if(!pool)
		return NULL;
	pool->next = NULL;
	pool->nPtrs = 0;
	pool->ptrs = safe_alloc(sizeof(*pool->ptrs));
	if(!pool->ptrs)
		return NULL;
	pool->nPtrs = 1;
	pool->ptrs[0].off = 0;
	pool->ptrs[0].sz = szAligned / sizeof(U32);
	memcpy(pool->data, data, szData);
	return NULL;
}
