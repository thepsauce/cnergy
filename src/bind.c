#include "cnergy.h"

static char str_space[65536];
static char *str_ptr = str_space;
static uint8_t mode_space[262144];
static int *mode_keys = (int*) mode_space;
static struct binding_call *mode_calls = (struct binding_call*) (mode_space + sizeof(mode_space));
struct binding_mode *all_modes[WINDOW_MAX];

char *
mode_allocstr(const char *str, size_t nStr)
{
	char *s;

	for(s = str_space; s < str_ptr - nStr; s += strlen(s) + 1)
		if(!strncmp(s, str, nStr) && !s[nStr])
			return s;
	if(str_ptr + nStr + 1 > str_space + sizeof(str_space))
		return NULL;
	s = str_ptr;
	memcpy(str_ptr, str, nStr);
	str_ptr += nStr;
	*(str_ptr++) = 0;
	return s;
}

int *
mode_allockeys(const int *keys, unsigned nKeys)
{
	int *k;

	for(k = (int*) mode_space; k < mode_keys - nKeys; k++)
		if(!memcmp(k, keys, sizeof(*keys) * nKeys))
			return k;
	if((void*) (mode_keys + nKeys) > (void*) mode_calls)
	   return NULL;
	k = mode_keys;
	memcpy(mode_keys, keys, sizeof(*keys) * nKeys);
	mode_keys += nKeys;
	return k;
}

struct binding_call *
mode_alloccalls(const struct binding_call *calls, unsigned nCalls)
{
	struct binding_call *c;

	for(c = (struct binding_call*) (mode_space + sizeof(mode_space));
			c > mode_calls + nCalls; c--)
	   if(!memcmp(c, calls, sizeof(*calls) * nCalls))
		   return c;
	if((void*) (mode_calls - nCalls) < (void*) mode_keys)
		return NULL;
	mode_calls -= nCalls;
	c = mode_calls;
	memcpy(mode_calls, calls, sizeof(*calls) * nCalls);
	return c;
}

static struct binding_mode *
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
		for(b1 = m1->bindings, n1 = m1->nBindings; n1; n1--, b1++) {
			struct binding_call *c1, *c2;
			unsigned n;

			if(b1->nCalls != b2->nCalls)
				continue;
			c1 = b1->calls;
			c2 = b2->calls;
			for(n = b1->nCalls; n; n--, c1++, c2++)
				if(c1->type != c2->type || c1->param != c2->param)
					break;
			if(!n) {
				if(mergekeys(b1, b2) < 0)
					return -1;
				break;
			}
		}
		if(n1)
			continue;
		newBindings = realloc(m1->bindings, sizeof(*m1->bindings) * (m1->nBindings + 1));
		if(newBindings)
			return -1;
		m1->bindings = newBindings;
		m1->bindings[m1->nBindings++] = (struct binding) {
			.nKeys = b2->nKeys,
			.nCalls = b2->nCalls,
			.keys = mode_allockeys(b2->keys, b2->nKeys),
			.calls = mode_alloccalls(b2->calls, b2->nCalls),
		};
	}
	return 0;
}

static void
print_escaped_string(const char *str) {
	if(!str) {
		printf("(null)");
		return;
	}
	putchar('"');
	for(const char *p = str; *p; p++) {
		if(*p == '\n') {
			putchar('\\');
			putchar('n');
		} else if(*p == '\t') {
			putchar('\\');
			putchar('t');
		} else if(*p == '\r') {
			putchar('\\');
			putchar('r');
		} else if(*p == '\f') {
			putchar('\\');
			putchar('f');
		} else if(*p == '\v') {
			putchar('\\');
			putchar('v');
		} else if(*p == '\a') {
			putchar('\\');
			putchar('a');
		} else if(*p == '\b') {
			putchar('\\');
			putchar('b');
		} else if(*p == '\\') {
			putchar('\\');
			putchar('\\');
		} else if(*p == '"') {
			putchar('\\');
			putchar('"');
		} else if(isprint(*p)) {
			putchar(*p);
		} else {
			printf("\\x%02x", (unsigned char) *p);
		}
	}
	putchar('"');
}

void
print_binding_calls(struct binding_call *calls, unsigned nCalls) {
	printf("{ ");
	for(; nCalls; nCalls--, calls++) {
		if(calls->flags & FBIND_CALL_AND)
			printf("& ");
		if(calls->flags & FBIND_CALL_OR)
			printf("| ");
		switch(calls->type) {
		case BIND_CALL_NULL:
			printf("NULL ");
			break;
		case BIND_CALL_ASSERT:
			printf("ASSERT ");
			print_escaped_string(calls->str);
			break;
		case BIND_CALL_STARTLOOP:
			printf("((");
			break;
		case BIND_CALL_ENDLOOP:
			printf("))");
			break;
		case BIND_CALL_REGISTER:
			printf("REGISTER %zd", calls->param);
			break;
		case BIND_CALL_MOVECURSOR:
			printf("MOVECURSOR %zd", calls->param);
			break;
		case BIND_CALL_MOVEHORZ:
			printf("MOVEHORZ %zd", calls->param);
			break;
		case BIND_CALL_MOVEVERT:
			printf("MOVEVERT %zd", calls->param);
			break;
		case BIND_CALL_INSERT:
			printf("INSERT ");
			print_escaped_string(calls->str);
			break;
		case BIND_CALL_DELETE:
			printf("DELETE %zd", calls->param);
			break;
		case BIND_CALL_DELETELINE:
			printf("DELETELINE %zd", calls->param);
			break;
		case BIND_CALL_DELETESELECTION:
			printf("DELETESELECTION");
			break;
		case BIND_CALL_SETMODE:
			printf("SETMODE %s", calls->str);
			break;
		case BIND_CALL_COPY:
			printf("COPY");
			break;
		case BIND_CALL_PASTE:
			printf("PASTE");
			break;
		case BIND_CALL_UNDO:
			printf("UNDO");
			break;
		case BIND_CALL_REDO:
			printf("REDO");
			break;
		default:
			printf("UNKWOWN[%u]", calls->type);
		}
		if(calls->flags & FBIND_CALL_USEKEY)
			printf("%%%zd", calls->param);
		if(calls->flags & FBIND_CALL_USECACHE)
			printf("[%zd", calls->param);
		if(calls->flags & FBIND_CALL_USENUMBER)
			printf("N");
		putchar(' ');
	}
	printf("}");
}

void
print_modes(struct binding_mode *m)
{
	if(!m)
		return;
	printf("MODE: %s (%#x)(%u bindings)\n", m->name, m->flags, m->nBindings);
	for(unsigned j = 0; j < m->nBindings; j++) {
		const struct binding bind = m->bindings[j];
		printf("  BIND([%p]%u, [%p]%u): ", bind.keys, bind.nKeys, bind.calls, bind.nCalls);
		fflush(stdout);
		for(int *k = bind.keys, *e = k + bind.nKeys;;) {
			for(;;) {
				printf("%s", keyname(*k));
				k++;
				if(!*k)
					break;
				putchar(' ');
			}
			k++;
			if(k == e)
				break;
			printf(", ");
		}
		putchar(' ');
		print_binding_calls(bind.calls, bind.nCalls);
		putchar('\n');
	}
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
			for(unsigned b = 0; b < m->nBindings; b++) {
				const struct binding bind = m->bindings[b];
				newMode->bindings[b] = (struct binding) {
					.nKeys = bind.nKeys,
					.nCalls = bind.nCalls,
					.keys = mode_allockeys(bind.keys, bind.nKeys),
					.calls = mode_alloccalls(bind.calls, bind.nCalls),
				};
			}
			newMode->nBindings = m->nBindings;
			if(prev_e)
				prev_e->next = newMode;
			else
				all_modes[i] = newMode;
		}
	}
	return 0;
}

/*static void
print_events(const struct event *events, U32 iEvent, U32 nEvents) {
	move(11, 0);
	printw("Index | Type | Flags | iIns | iDel | vct | data\n");
	printw("%2d,%2d--------------------------------------------\n", iEvent, nEvents);
	for (U32 i = 0; i < nEvents; i++) {
		printw(i == iEvent ? "(%-3d)" : "%-5d", i);
		printw(" | %-4d | %-5d | %-4u | %-4u | %-3u | ", events[i].type,
			events[i].flags, events[i].vct, events[i].nIns, events[i].nDel);
		printw("\n");
	}
}*/
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
bind_exec(const int *keys, ptrdiff_t amount)
{
	struct binding *bind;
	const struct binding_call *bcs;
	unsigned nbc;
	struct state_frame {
		bool state;
		bool run;
	} stateStack[32], frame;
	unsigned nStateStack = 0;
	ptrdiff_t reg = 0;
	ptrdiff_t cached = 0;
	unsigned nKeys = 0;

	bind = mode_findbind(focus_window->bindMode, keys);
	if(bind == (struct binding*) focus_window->bindMode)
		return 1;
	if(!bind) {
		struct binding_mode *const mode = mode_find(WINDOW_ALL, focus_window->bindMode->name);
		if(mode && (bind = mode_findbind(mode, keys)) == (struct binding*) mode)
			return 1;
	}
	if(!bind)
		return 2;

	for(const int *k = keys; *k; k++)
		nKeys++;
	frame.state = true;
	frame.run = false;
	for(bcs = bind->calls, nbc = bind->nCalls; nbc; nbc--, bcs++) {
		struct binding_call bc;
		bool b = true;
		ptrdiff_t m;

		bc = *bcs;
		if(bc.flags & FBIND_CALL_USEKEY)
			m = bc.param >= 0 && bc.param < (ptrdiff_t) nKeys ? keys[bc.param] : 0;
		else if(bc.flags & FBIND_CALL_USECACHE)
			m = cached;
		else
			m = bc.param;
		if(bc.flags & FBIND_CALL_USENUMBER)
			m = SAFE_MUL(amount, bc.param);
		if((bc.flags & FBIND_CALL_AND) && !frame.state)
			goto end_frame;
		if((bc.flags & FBIND_CALL_XOR) && frame.state) {
			// note that startloop and endloop are not allowed to have the xor flag, this is checked for in the parser
			frame.state = false;
			continue;
		}
		switch(bc.type) {
		case BIND_CALL_STARTLOOP:
			frame.state = true;
			stateStack[nStateStack++] = frame;
			frame.run = false;
			// continue to ignore flags like and/or (they wouldn't do anything for startloop anyway)
			continue;
		case BIND_CALL_ENDLOOP:
			bcs -= bc.param;
			nbc += bc.param;
			frame.state = true;
			frame.run = true;
			// continue to ignore flags like and/or
			continue;
		case BIND_CALL_REGISTER:
			b = !!reg;
			reg += m;
			break;
		case BIND_CALL_SETMODE:
			focus_window->bindMode = mode_find(focus_window->type, bc.str);
			break;
		case BIND_CALL_CLOSEWINDOW:
			window_close(focus_window);
			b = !!focus_window;
			break;
		case BIND_CALL_MOVEWINDOW_RIGHT:
		case BIND_CALL_MOVEWINDOW_BELOW: {
			int i = 0;
			// get the right directional function
			struct window *(*const next)(const struct window*) =
				bc.type == BIND_CALL_MOVEWINDOW_RIGHT ?
					(m > 0 ? window_right : window_left) :
				m > 0 ? window_below : window_above;
			const int di = m < 0 ? -1 : 1;
			for(struct window *w = focus_window; i != m && (w = (*next)(w)); i += di)
				focus_window = w;
			b = i == m;
			break;
		}
		case BIND_CALL_VSPLIT: {
			struct window *win;

			win = window_dup(focus_window);
			if(!win) {
				b = false;
				break;
			}
			window_attach(focus_window, win, ATT_WINDOW_HORIZONTAL);
			break;
		}
		case BIND_CALL_HSPLIT: {
			struct window *win;

			win = window_dup(focus_window);
			if(!win) {
				b = false;
				break;
			}
			window_attach(focus_window, win, ATT_WINDOW_VERTICAL);
			break;
		}
		case BIND_CALL_OPENWINDOW: {
			const fileid_t file = 0; // TODO: add choosefile(); again
			if(file) {
				struct buffer *buf;
				struct window *win;

				buf = buffer_new(file);
				if(!buf) {
					b = false;
					break;
				}
				win = edit_new(buf, edit_statesfromfiletype(file));
				if(win) {
					window_attach(focus_window, win, ATT_WINDOW_UNSPECIFIED);
					focus_window = win;
				} else {
					buffer_free(buf);
					b = false;
				}
			}
			break;
		}
		case BIND_CALL_NEWWINDOW: {
			struct buffer *buf;
			struct window *win;

			buf = buffer_new(0);
			if(!buf) {
				b = false;
				break;
			}
			win = edit_new(buf, NULL);
			if(win) {
				window_attach(focus_window, win, ATT_WINDOW_UNSPECIFIED);
				focus_window = win;
			} else {
				buffer_free(buf);
				b = false;
			}
			break;
		}
		case BIND_CALL_COLORWINDOW:
			// TODO: open a color window or focus an existing one
			break;
		default:
			// not a general bind call
			break;
		}
		b &= window_types[focus_window->type].bindcall(focus_window, &bc, m, &cached);
		while(((bc.flags & FBIND_CALL_OR) && !(frame.state |= b))
				|| ((bc.flags & FBIND_CALL_AND) && !(frame.state &= b))) {
		end_frame:
			do {
				nbc--;
				bcs++;
				if(!nbc)
					return 0; // found end of program without any endloop in our way
				bc = *bcs;
			} while(bc.type != BIND_CALL_ENDLOOP);
			b = frame.run;
			frame = stateStack[--nStateStack];
		}
		if(!(bc.flags & (FBIND_CALL_AND | FBIND_CALL_OR)))
			frame.state = b;
	}
	return 0; // program finished normally
}
