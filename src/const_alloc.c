#include "cnergy.h"

struct memptr {
	tiny_size_t off;
	tiny_size_t sz;
};

struct mempool {
	uint8_t data[TINY_SIZE_MAX / 4];
	struct memptr *ptrs;
	size_t nPtrs;
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
insertptr(struct mempool *pool, small_size_t off, small_size_t sz)
{
	struct memptr *newPtrs;

	off /= 4;
	sz /= 4;
	for(size_t i = 0; i < pool->nPtrs; i++) {
		const tiny_size_t l = pool->ptrs[i].off;
		const tiny_size_t r = l + pool->ptrs[i].sz;
		if(off >= l && off < r) {
			if(off == l)
				return pool->ptrs + i; // pointer already exists
			newPtrs = realloc(pool->ptrs, sizeof(*pool->ptrs) * (pool->nPtrs + 1));
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
const_alloc(const void *data, small_size_t szData)
{
	struct mempool *pool;
	struct memptr *newPtrs;
	const small_size_t szAligned = (szData + sizeof(small_size_t) - 1) & ~(sizeof(small_size_t) - 1);
	for(pool = first; pool; pool = pool->next) {
		if(!pool->nPtrs)
			break;
		for(small_size_t *d = (small_size_t*) pool->data; 
				d <= (small_size_t*) (pool->data + sizeof(pool->data) - szAligned); 
				d++) {
			if(!memcmp(d, data, szData)) {
				struct memptr *const ptr = insertptr(pool, (uint8_t*) d - pool->data, szAligned);
				if(!ptr)
					return NULL;
				memcpy(d, data, szData);
				return d;
			}
		}
	}
	for(pool = first; pool; pool = pool->next) {
		const small_size_t off = !pool->nPtrs ? 0 : pool->ptrs[pool->nPtrs - 1].off + pool->ptrs[pool->nPtrs - 1].sz;
		if(off + szAligned / sizeof(small_size_t) <= sizeof(pool->data) / sizeof(small_size_t)) {
			newPtrs = realloc(pool->ptrs, sizeof(*pool->ptrs) * (pool->nPtrs + 1));
			if(!newPtrs)
				return NULL;
			pool->ptrs = newPtrs;
			pool->ptrs[pool->nPtrs++] = (struct memptr) { .off = off, .sz = szAligned / sizeof(small_size_t) };
			void *const d = (small_size_t*) pool->data + off;
			memcpy(d, data, szData);
			return d;
		}
	}
	pool = pool->next = malloc(sizeof(*pool->next));
	if(!pool)
		return NULL;
	pool->next = NULL;
	pool->nPtrs = 0;
	pool->ptrs = malloc(sizeof(*pool->ptrs));
	if(!pool->ptrs)
		return NULL;
	pool->nPtrs = 1;
	pool->ptrs[0].off = 0;
	pool->ptrs[0].sz = szAligned / sizeof(small_size_t);
	memcpy(pool->data, data, szData);
	return NULL;
}
