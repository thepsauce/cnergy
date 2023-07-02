#include "cnergy.h"

static char str_space[65536];
static size_t n_strings;
static char mode_keys[262144];
static size_t n_keys;
static uint8_t program_space[0xffff];
static size_t sz_programs;
struct binding_mode *all_modes;
unsigned n_modes;

void
debug_modes_print(struct binding_mode *modes, unsigned nModes)
{
	if(!modes) {
		modes = all_modes;
		nModes = n_modes;
	}
	for(unsigned i = 0; i < nModes; i++) {
		struct binding_mode *const m = modes + i;
		printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
		printf("MODE: %s (%u bindings)\n", m->name, m->nBindings);
		printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
		for(unsigned j = 0; j < m->nBindings; j++) {
			const struct binding bind = m->bindings[j];
				printf("  BIND([%p]%zu): ", (void*) bind.keys, bind.nKeys);
			fflush(stdout);
			for(char *k = bind.keys, *e = k + bind.nKeys;;) {
				for(;;) {
					const unsigned l = utf8_len(k, e - k);
					const int uc = utf8_tounicode(k, e - k);
					if(uc >= 0xf6b1 && uc <= 0xf6b1 + 0xff)
						printf("<%s>", keyname(uc - (0xf6b1 - 0xff)) + 4);
					else if(uc <= 0x7f)
						printf("%s", keyname(uc));
					else
						printf("%.*s", l, k);
					k += l;
					if(!*k)
						break;
				}
				k++;
				if(k == e)
					break;
				printf(", ");
			}
			putchar('\n');
			printf("    ");
			for(size_t sz = 0; sz < bind.szProgram; sz++) {
				const uint8_t b = *(uint8_t*) (bind.program + sz);
				const uint8_t
					l = b & 0xf,
					h = b >> 4;
				printf("0x");
				putchar(h >= 0xa ? (h - 0xa + 'a') : h + '0');
				putchar(l >= 0xa ? (l - 0xa + 'a') : l + '0');
				putchar(' ');
			}
			printf("\n========\n");
			environment_loadandprint(bind.program, bind.szProgram);
			printf("========");
			putchar('\n');
		}
	}
}

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
mode_find(const char *name)
{
	for(unsigned i = 0; i < n_modes; i++)
		if(!strcmp(all_modes[i].name, name))
			return all_modes + i;
	return NULL;
}

struct binding_mode *
mode_new(const char *name)
{
	struct binding_mode *mode;

	mode = mode_find(name);
	if(!mode) {
		mode = realloc(all_modes, sizeof(*all_modes) *
				(n_modes + 1));
		if(!mode)
			return NULL;
		all_modes = mode;
		mode += n_modes;
		n_modes++;
		memset(mode, 0, sizeof(*mode));
		strcpy(mode->name, name);
	}
	return mode;
}

static inline int
mergekeys(struct binding *b1, const struct binding *b2)
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

int
mode_addbind(struct binding_mode *mode, const struct binding *bind)
{
	struct binding *b;
	unsigned n;
	struct binding *newBindings;

	for(b = mode->bindings, n = mode->nBindings; n; n--, b++)
		if(b->szProgram == bind->szProgram &&
				!memcmp(b->program,
					bind->program,
					bind->szProgram))
			return mergekeys(b, bind);

	newBindings = realloc(mode->bindings, sizeof(*mode->bindings) *
			(mode->nBindings + 1));
	if(newBindings == NULL)
		return -1;
	mode->bindings = newBindings;
	memcpy(mode->bindings + mode->nBindings, bind, sizeof(*bind));
	mode->nBindings++;
	return 0;
}

int
mode_addallbinds(struct binding_mode *mode, const struct binding_mode *from)
{
	const struct binding *b;
	unsigned n;

	for(b = from->bindings, n = from->nBindings; n; n--, b++)
		mode_addbind(mode, b);
	return 0;
}

int
bind_find(const char *keys, struct binding **pBind)
{
	bool startsWith = false;
	struct binding *binds;
	unsigned nBinds;
	struct binding_mode *mode, *nextMode;

	//mode = window_getbindmode(focus_window);
	//nextMode = mode ? mode_find(window_getbindmode(focus_window)->name) : NULL;
	mode = nextMode = NULL;
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
