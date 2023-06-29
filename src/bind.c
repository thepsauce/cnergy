#include "cnergy.h"

static char str_space[65536];
static size_t n_strings;
static char mode_keys[262144];
static size_t n_keys;
static uint8_t program_space[0xffff];
static size_t sz_programs;
struct binding_mode *all_modes[WINDOW_MAX];

char *
mode_allockeys(const char *keys, size_t nKeys)
{
	char *k;

	if(n_keys >= nKeys)
		for(k = mode_keys; k <= mode_keys + n_keys - nKeys; k++)
			if(!memcmp(k, keys, nKeys))
				return k;
	if(mode_keys + nKeys > mode_keys + ARRLEN(mode_keys))
		return NULL;
	k = mode_keys + n_keys;
	memcpy(k, keys, nKeys);
	n_keys += nKeys;
	return k;
}

char *
mode_allocstring(const char *str, size_t nStr)
{
	char *s;

	if(n_strings >= nStr)
		for(s = str_space; s < str_space + n_strings - nStr; s += strlen(s) + 1)
			if(!strncmp(s, str, nStr) && !s[nStr])
				return s;
	if(n_strings + nStr + 1 > sizeof(str_space))
		return NULL;
	s = str_space + n_strings;
	memcpy(s, str, nStr);
	s[nStr] = 0;
	n_strings += nStr + 1;
	return s;
}

void *
mode_allocprogram(const void *program, size_t szProgram)
{
	void *p;
	if(sz_programs >= szProgram)
		for(p = program_space; p < (void*) program_space + sz_programs - szProgram; p++)
			if(!memcmp(p, program, szProgram))
				return p;
	if(sz_programs + szProgram > sizeof(program_space))
		return NULL;
	p = program_space + sz_programs;
	memcpy(p, program, szProgram);
	sz_programs += szProgram;
	return p;
}

struct binding_mode *
mode_find(window_type_t windowType, const char *name)
{
	for(struct binding_mode *m = all_modes[windowType]; m; m = m->next)
		if(!strcmp(m->name, name))
			return m;
	return NULL;
}

static inline int
mergekeys(struct binding *b1, struct binding *b2)
{
	char *k1, *k2;
	char *e1, *e2;
	char *newKeys;

	for(k2 = b2->keys, e2 = k2 + b2->nKeys; k2 != e2; k2++) {
		bool contained = false;
		char *k;

		for(k1 = b1->keys, e1 = k1 + b1->nKeys; k1 != e1; k1++) {
			for(k = k2; *k; k++, k1++)
				if(*k1 != *k)
					break;
			if(!*k1 && !*k) {
				contained = true;
				break;
			}
		}
		if(!contained) {
			for(; *k; k++);
			const size_t n = (k - k2) + 1;
			char joined[b1->nKeys + n];
			memcpy(joined, b1->keys, sizeof(*b1->keys) * b1->nKeys);
			memcpy(joined + b1->nKeys, k2, sizeof(*k2) * n);
			newKeys = mode_allockeys(joined, b1->nKeys + n);
			if(!newKeys)
				return -1;
			b1->keys = newKeys;
			b1->nKeys += n;
		}
		k2 = k;
	}
	return 0;
}

static inline int
mergebinds(struct binding_mode *m1, struct binding_mode *m2)
{
	struct binding *b1, *b2;
	unsigned n1, n2;
	struct binding *newBindings;

	for(b2 = m2->bindings, n2 = m2->nBindings; n2; n2--, b2++) {
		for(b1 = m1->bindings, n1 = m1->nBindings; n1; n1--, b1++)
			if(b1->szProgram == b2->szProgram &&
					!memcmp(b1->program, b2->program, b2->szProgram)) {
				if(mergekeys(b1, b2) < 0)
					return -1;
				break;
			}
		if(n1)
			continue;
		newBindings = realloc(m1->bindings, sizeof(*m1->bindings) * (m1->nBindings + 1));
		if(newBindings)
			return -1;
		m1->bindings = newBindings;
		memcpy(m1->bindings + m1->nBindings, b2, sizeof(*b2));
		m1->nBindings++;
	}
	return 0;
}

int
modes_add(struct binding_mode *modes[WINDOW_MAX])
{
	for(window_type_t i = 0; i < WINDOW_MAX; i++) {
		struct binding_mode *m = modes[i];
		for(; m; m = m->next) {
			struct binding_mode *e, *prev_e;
			struct binding_mode *newMode;

			for(e = all_modes[i], prev_e = NULL; e; prev_e = e, e = e->next) {
				if(!strcmp(m->name, e->name)) {
					if(mergebinds(m, e) < 0)
						return -1;
					break;
				}
			}
			if(e)
				continue;
			newMode = malloc(sizeof(*newMode));
			if(!newMode)
				return -1;
			memset(newMode, 0, sizeof(*newMode));
			strcpy(newMode->name, m->name);
			newMode->flags = m->flags;
			newMode->bindings = malloc(sizeof(*newMode->bindings) * m->nBindings);
			if(!newMode->bindings) {
				free(newMode);
				return -1;
			}
			memcpy(newMode->bindings, m->bindings, sizeof(*m->bindings) * m->nBindings);
			newMode->nBindings = m->nBindings;
			if(prev_e)
				prev_e->next = newMode;
			else
				all_modes[i] = newMode;
		}
	}
	return 0;
}

int
bind_find(const char *keys, struct binding **pBind)
{
	bool startsWith = false;
	struct binding *binds;
	unsigned nBinds;
	struct binding_mode *mode, *nextMode;

	mode = window_getbindmode(focus_window);
	nextMode = mode ? mode_find(WINDOW_ALL, window_getbindmode(focus_window)->name) : NULL;
	while(mode) {
		for(binds = mode->bindings, nBinds = mode->nBindings; nBinds; binds++, nBinds--)
		for(const char *k, *ks = binds->keys, *es = ks + binds->nKeys; ks != es; ks++) {
			for(k = keys; *ks; ks++, k++)
				/* -1 means that any key is allowed and not any specific one
				 * see the usage of the ** inside parse.h
				 */
				if(*ks != *k && (!*k || *ks != -1))
					break;
			if(!*ks && !*k) {
				*pBind = binds;
				return 0;
			}
			do ks++; while(*ks);
			if(!*k)
				startsWith = true;
		}
		mode = nextMode;
		nextMode = NULL;
	}
	return 2 - startsWith;
}
