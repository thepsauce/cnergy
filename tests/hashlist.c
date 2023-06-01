#include "cnergy.h"

struct hashlist {
	struct hashlist_bucket {
		U32 nEntries;
		struct hashlist_entry {
			U32 hash;
			char *item;
		} *entries;
	} *buckets;
	U32 nBuckets;
	U32 nEntries;
};

int hashlist_init(struct hashlist *hl, U32 nBuckets);
bool hashlist_exists(struct hashlist *hl, const char *item);
int hashlist_add(struct hashlist *hl, const char *item);


int
hashlist_init(struct hashlist *hl, U32 nBuckets)
{
	hl->buckets = malloc(sizeof(*hl->buckets) * nBuckets);
	if(!hl->buckets)
		return -1;
	memset(hl->buckets, 0, sizeof(*hl->buckets) * nBuckets);
	hl->nBuckets = nBuckets;
	hl->nEntries = 0;
	return 0;
}

static U32
hashlist_hash(const char *item)
{
	U32 hash = 5381; // prime number
	while(*item)
		hash = ((hash << 5) + hash) ^ *(item++);
	return hash;
}

static int
hashlist_addentry(struct hashlist_bucket *hb, struct hashlist_entry *entry)
{
	struct hashlist_entry *newEntries;

	// fill gap if there is one
	for(U32 i = 0; i < hb->nEntries; i++)
		if(!hb->entries[i].item) {
			hb->entries[i] = *entry;
			return 0;
		}
	// add new entry
	newEntries = realloc(hb->entries, sizeof(*hb->entries) * (hb->nEntries + 1));
	if(!newEntries)
		return -1;
	hb->entries = newEntries;
	hb->entries[hb->nEntries++] = *entry;
	return 0;
}

static int 
hashlist_resize(struct hashlist *hl)
{
	struct hashlist_bucket *newBuckets;
	const U32 nNew = hl->nBuckets * 2;
	newBuckets = realloc(hl->buckets, sizeof(*hl->buckets) * nNew);
	if(!newBuckets)
		return -1;
	hl->buckets = newBuckets;
	// find any entries that need to be repositioned and reposition them
	for(U32 n = hl->nBuckets; n; ) {
		struct hashlist_bucket *hb;
		
		hb = hl->buckets + --n;
		if(!hb->nEntries)
			continue;
		for(U32 i = 0; i < hb->nEntries; i++) {
			const U32 newPos = hb->entries[i].hash & (nNew - 1);
			// if it has a new position now, set old to gap and add to new
			if(newPos != n) {
				hashlist_addentry(hl->buckets + newPos, hb->entries + i);
				hb->entries[i].item = NULL;
			}
		}
	}
	hl->nBuckets = nNew;
	return 0;
}

bool
hashlist_exists(struct hashlist *hl, const char *item)
{
	U32 hash;
	struct hashlist_bucket *hb;

	hash = hashlist_hash(item);
	hb = hl->buckets + (hash & (hl->nBuckets - 1));
	for(U32 i = 0; i < hb->nEntries; i++)
		if(hb->entries[i].item && !strcmp(hb->entries[i].item, item))
			return true;
	return false;
}

int
hashlist_add(struct hashlist *hl, const char *item)
{
	U32 hash;
	struct hashlist_bucket *hb;
	char *dup;

	hash = hashlist_hash(item);
	hb = hl->buckets + (hash & (hl->nBuckets - 1));
	for(U32 i = 0; i < hb->nEntries; i++)
		if(hb->entries[i].item && !strcmp(hb->entries[i].item, item))
			return 1; // if the element already exists
	if(hl->nEntries / 2 > hl->nBuckets) {
		if(hashlist_resize(hl) < 0)
			return -1;
		hb = hl->buckets + (hash & (hl->nBuckets - 1));
	}
	dup = const_alloc(item, strlen(item) + 1);
	if(!dup)
		return -1;
	if(hashlist_addentry(hb, &(struct hashlist_entry) {
				.hash = hash,
				.item = dup,
			}) < 0)
		return -1;
	hl->nEntries++;
	return 0;
}

int
hashlist_remove(struct hashlist *hl, const char *item)
{
	U32 hash;
	struct hashlist_bucket *hb;

	hash = hashlist_hash(item);
	hb = hl->buckets + (hash & (hl->nBuckets - 1));
	for(U32 i = 0; i < hb->nEntries; i++) {
		if(!hb->entries[i].item)
			continue;
		if(!strcmp(hb->entries[i].item, item)) {
			hb->entries[i].item = NULL;
			hl->nEntries--;
			return 0;
		}
	}
	return 1;
}
