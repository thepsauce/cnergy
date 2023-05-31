#include "cnergy.h"

struct binding_mode *all_modes[WINDOW_MAX];

static struct binding_mode *
mode_find(U32 windowType, const char *name)
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
			const U32 n = (k - k2) + 1;
			int newKeys[b1->nKeys + n];
			memcpy(newKeys, b1->keys, sizeof(*b1->keys) * b1->nKeys);
			memcpy(newKeys + b1->nKeys, k2, sizeof(*k2) * n);
			b1->keys = const_alloc(newKeys, sizeof(newKeys));
			if(!b1->keys)
				return -1;
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
	U32 n1, n2;

	for(b2 = m2->bindings, n2 = m2->nBindings; n2; n2--, b2++) {
		for(b1 = m1->bindings, n1 = m1->nBindings; n1; n1--, b1++) {
			struct binding_call *c1, *c2;
			U32 n;

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
		m1->bindings = realloc(m1->bindings, sizeof(*m1->bindings) * (m1->nBindings + 1));
		if(!m1->nBindings)
			return -1;
		m1->bindings[m1->nBindings++] = (struct binding) {
			.nKeys = b2->nKeys,
			.nCalls = b2->nCalls,
			.keys = const_alloc(b2->keys, sizeof(*b2->keys) * b2->nKeys),
			.calls = const_alloc(b2->calls, sizeof(*b2->calls) * b2->nCalls),
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
print_binding_calls(struct binding_call *calls, U32 nCalls) {
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
			printf("ASSERT");
			print_escaped_string(calls->str);
			break;
		case BIND_CALL_STARTLOOP:
			printf("((");
			break;
		case BIND_CALL_ENDLOOP:
			printf("))");
			break;
		case BIND_CALL_REGISTER:
			printf("REGISTER %d", calls->param);
			break;
		case BIND_CALL_MOVECURSOR:
			printf("MOVECURSOR %d", calls->param);
			break;
		case BIND_CALL_MOVEHORZ:
			printf("MOVEHORZ %d", calls->param);
			break;
		case BIND_CALL_MOVEVERT:
			printf("MOVEVERT %d", calls->param);
			break;
		case BIND_CALL_INSERT:
			printf("INSERT ");
			print_escaped_string(calls->str);
			break;
		case BIND_CALL_DELETE:
			printf("DELETE %d", calls->param);
			break;
		case BIND_CALL_DELETELINE:
			printf("DELETELINE %d", calls->param);
			break;
		case BIND_CALL_DELETESELECTION:
			printf("DELETESELECTION");
			break;
		case BIND_CALL_SETMODE:
			printf("SETMODE %d", calls->param);
			break;
		case BIND_CALL_COPY:
			printf("COPY");
			break;
		case BIND_CALL_PASTE:
			printf("PASTE");
			break;
		default:
			printf("UNKWOWN[%u]", calls->type);
		}
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
	for(U32 j = 0; j < m->nBindings; j++) {
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
	for(U32 i = 0; i < WINDOW_MAX; i++) {
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
			for(U32 b = 0; b < m->nBindings; b++) {
				const struct binding bind = m->bindings[b];
				newMode->bindings[b] = (struct binding) {
					.nKeys = bind.nKeys,
					.nCalls = bind.nCalls,
					.keys = const_alloc(bind.keys, sizeof(*bind.keys) * bind.nKeys),
					.calls = const_alloc(bind.calls, sizeof(*bind.calls) * bind.nCalls),
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
	U32 nBinds;
	struct binding *ret = NULL;

	for(binds = mode->bindings, nBinds = mode->nBindings; nBinds; binds++, nBinds--) {
		for(const int *k, *ks = binds->keys, *es = ks + binds->nKeys; ks != es; ks++) {
			for(k = keys; *ks; ks++, k++)
				if(*ks != *k)
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
bind_exec(const int *keys, I32 amount)
{
	struct binding *bind;
	const struct binding_call *bcs;
	U32 nbc;
	struct state_frame {
		bool state;
		bool run;
	} stateStack[32], frame;
	U32 nStateStack = 0;
	int reg = 0;

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

	frame.state = true;
	frame.run = false;
	for(bcs = bind->calls, nbc = bind->nCalls; nbc; nbc--, bcs++) {
		struct binding_call bc;
		bool b = true;

		bc = *bcs;
		const I32 m = (bc.flags & FBIND_CALL_USENUMBER) ? SAFE_MUL(amount, bc.param) : bc.param;
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
			const char *const file = NULL; // TODO: add choosefile(); again
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

			buf = buffer_new(NULL);
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
		}
		b &= window_types[focus_window->type].bindcall(focus_window, &bc, m);
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
