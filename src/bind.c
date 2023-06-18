#include "cnergy.h"

static int mode_keys[262144];
size_t n_keys;
struct binding_mode *all_modes[WINDOW_MAX];

int *
mode_allockeys(const int *keys, unsigned nKeys)
{
	int *k;

	if(n_keys >= nKeys)
		for(k = mode_keys; k <= mode_keys + n_keys - nKeys; k++)
			if(!memcmp(k, keys, sizeof(*keys) * nKeys))
				return k;
	if(mode_keys + nKeys > mode_keys + ARRLEN(mode_keys))
		return NULL;
	k = mode_keys + n_keys;
	memcpy(k, keys, sizeof(*keys) * nKeys);
	n_keys += nKeys;
	return k;
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
	int *k1, *k2;
	int *e1, *e2;
	int *newKeys;

	for(k2 = b2->keys, e2 = k2 + b2->nKeys; k2 != e2; k2++) {
		bool contained = false;
		int *k;

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
			const unsigned n = (k - k2) + 1;
			int joined[b1->nKeys + n];
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

static inline struct binding *
mode_findbind(struct binding_mode *mode, const int *keys)
{
	struct binding *binds;
	unsigned nBinds;
	struct binding *ret = NULL;

	for(binds = mode->bindings, nBinds = mode->nBindings; nBinds; binds++, nBinds--) {
		for(const int *k, *ks = binds->keys, *es = ks + binds->nKeys; ks != es; ks++) {
			for(k = keys; *ks; ks++, k++)
				// -1 means that any key is allowed and not any specific one, see the usage of the ** inside parse.h
				if(*ks != *k && (!*k || *ks != -1))
					break;
			if(!*ks && !*k)
				return binds;
			do ks++; while(*ks);
			if(!*k)
				ret = (struct binding*) mode; // indicate that a key sequence starts with the given sequence
		}
	}
	return ret;
}

int
bind_find(const int *keys, struct binding **pBind)
{
	struct binding *bind;

	bind = mode_findbind(window_getbindmode(focus_window), keys);
	if(bind == (struct binding*) window_getbindmode(focus_window))
		return 1;
	if(!bind) {
		struct binding_mode *const mode = mode_find(WINDOW_ALL, window_getbindmode(focus_window)->name);
		if(mode && (bind = mode_findbind(mode, keys)) == (struct binding*) mode)
			return 1;
	}
	if(!bind)
		return 2;
	*pBind = bind;
	return 0;
}
