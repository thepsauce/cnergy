#include "cnergy.h"

// Returns:
// 0 - Index was found
// 1 - Element that exactly matches was found
static inline int
sortedlist_getbestindex(struct sortedlist *s, const char *word, size_t nWord, unsigned *pIndex)
{
	unsigned left, right;

	left = 0;
	right = s->nEntries;
	while(left < right) {
		const unsigned middle = (left + right) / 2;
		const int cmp = strncmp(s->entries[middle].word, word, nWord);
		if(!cmp && !s->entries[middle].word[nWord]) {
			if(pIndex)
				*pIndex = middle;
			return 1;
		}
		if(cmp > 0)
			right = middle;
		else
			left = middle + 1;
	}
	if(pIndex)
		*pIndex = right;
	return 0;
}

int
sortedlist_add(struct sortedlist *s, const char *word, size_t nWord, void *param)
{
	unsigned index;
	char *w;
	struct sortedlist_entry *newEntries;

	if(sortedlist_getbestindex(s, word, nWord, &index))
		return 1;
	newEntries = realloc(s->entries, sizeof(*s->entries) * (s->nEntries + 1));
	if(!newEntries)
		return -1;
	s->entries = newEntries;
	w = malloc(nWord + 1);
	if(!w)
		return -1;
	memcpy(w, word, nWord);
	w[nWord] = 0;
	memmove(s->entries + index + 1, s->entries + index, sizeof(*s->entries) * (s->nEntries - index));
	s->nEntries++;
	s->entries[index].word = w;
	s->entries[index].param = param;
	return 0;
}

int
sortedlist_remove(struct sortedlist *s, const char *word, size_t nWord)
{
	unsigned index;

	if(!sortedlist_getbestindex(s, word, nWord, &index))
		return 1;
	s->nEntries--;
	memmove(s->entries + index, s->entries + index + 1, sizeof(*s->entries) * (s->nEntries - index));
	return 0;
}

bool
sortedlist_exists(struct sortedlist *s, const char *word, size_t nWord, void **pParam)
{
	unsigned index;
	
	if(!sortedlist_getbestindex(s, word, nWord, &index))
		return false;
	if(pParam)
		*pParam = s->entries[index].param;
	return true;
}
