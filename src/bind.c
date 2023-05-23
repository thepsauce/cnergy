#include "cnergy.h"

struct binding_mode *modes;
U32 nModes;
U32 iMode;

inline U32
mode_has(U32 flags)
{
	return modes[iMode].flags & flags;
}

static inline struct binding_mode *
getmode(const char *name, U32 *pIndex)
{
	for(U32 i = 0; i < nModes; i++)
		if(!strcmp(modes[i].name, name)) {
			if(pIndex)
				*pIndex = i;
			return modes + i;
		}
	return NULL;
}

int
mode_getmergepos(struct binding_mode *m, U32 n, U32 *pos)
{
	for(U32 i = 0, next = 0; i < n; i++) {
		if(m[i].flags & FBIND_MODE_SUPPLEMENTARY)
			continue;
		if(!getmode(m[i].name, pos + i))
			pos[i] = nModes + next++;
	}
	return 0;
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
			const_free(b1->keys);
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

static void
print_binding_calls(struct binding_call *calls, U32 nCalls) {
	printf("{ ");
	for(; nCalls; nCalls--, calls++) {
		switch(calls->type) {
		case BIND_CALL_NULL:
			printf("NULL ");
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
			print_escaped_string(const_getdata(calls->param));
			break;
		case BIND_CALL_DELETE:
			printf("DELETE");
			break;
		case BIND_CALL_DELETELINE:
			printf("DELETELINE");
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
			printf("UNKWOWN[%d]", calls->type);
		}
		if(calls->flags & FBIND_CALL_USENUMBER)
			printf("N");
		if(calls->flags & FBIND_CALL_AND)
			printf("&");
		putchar(' ');
	}
	printf("}");
}

void
print_modes(struct binding_mode *m, U32 n)
{
	if(!m) {
		m = modes;
		n = nModes;
	}
	printf("Got %u modes\n", nModes);
	for(U32 i = 0; i < nModes; i++) {
		printf("MODE: %s (%#x)(%u bindings)\n", modes[i].name, modes[i].flags, modes[i].nBindings);
		for(U32 j = 0; j < modes[i].nBindings; j++) {
			const struct binding bind = modes[i].bindings[j];
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
}

int
mode_merge(struct binding_mode *m, U32 n)
{
	for(U32 i; n; n--, m++) {
		if(m->flags & FBIND_MODE_SUPPLEMENTARY)
			continue;
		for(i = 0; i < nModes; i++) {
			if(!strcmp(modes[i].name, m->name)) {
				if(mergebinds(modes + i, m) < 0)
					return -1;
				break;
			}
		}
		if(i != nModes)
			continue;
		modes = realloc(modes, sizeof(*modes) * (nModes + 1));
		if(!modes)
			return -1;
		strcpy(modes[nModes].name, m->name);
		modes[nModes].flags = m->flags;
		modes[nModes].bindings = malloc(sizeof(*modes[nModes].bindings) * m->nBindings);
		for(U32 b = 0; b < m->nBindings; b++) {
			const struct binding bind = m->bindings[b];
			modes[nModes].bindings[b] = (struct binding) {
				.nKeys = bind.nKeys,
				.nCalls = bind.nCalls,
				.keys = const_alloc(bind.keys, sizeof(*bind.keys) * bind.nKeys),
				.calls = const_alloc(bind.calls, sizeof(*bind.calls) * bind.nCalls),
			};
		}
		modes[nModes].nBindings = m->nBindings;
		nModes++;
	}
	return 0;
}

static void
print_events(const struct event *events, U32 iEvent, U32 nEvents) {
	move(11, 0);
	printw("Index | Type | Flags | iIns | iDel | vct | data\n");
	printw("%2d,%2d--------------------------------------------\n", iEvent, nEvents);
	for (U32 i = 0; i < nEvents; i++) {
		printw(i == iEvent ? "(%-3d)" : "%-5d", i);
		printw(" | %-4d | %-5d | %-4u | %-4u | %-3u | ", i, events[i].type,
			events[i].flags, events[i].vct, events[i].nIns, events[i].nDel);
		printw("\n");
	}
}

int exec_bind(const int *keys, I32 amount, struct window *window)
{
	const struct binding *binds;
	U32 nBinds;
	int ret = 2;
	const int *k;
	const struct binding_mode *const mode = modes + iMode;
	for(binds = mode->bindings, nBinds = mode->nBindings; nBinds; binds++, nBinds--) {
		// TODO: maybe too much code repetition
		for(const int *ks = binds->keys, *es = ks + binds->nKeys; ks != es; ks++) {
			for(k = keys; *ks; ks++, k++)
				if(*ks != *k)
					break;
			if(!*ks && !*k) {
				const struct binding_call *bcs;
				U32 nbc;

				for(bcs = binds->calls, nbc = binds->nCalls; nbc; nbc--, bcs++) {
					int r;
					const struct binding_call bc = *bcs;
					struct buffer *const buf = window->buffers + window->iBuffer;
					const I32 m = (bc.flags & FBIND_CALL_USENUMBER) ? amount * bc.param : bc.param;
					r = m;
					switch(bc.type) {
					case BIND_CALL_MOVECURSOR: r = buffer_movecursor(buf, m); break;
					case BIND_CALL_MOVEHORZ: r = buffer_movehorz(buf, m); break;
					case BIND_CALL_MOVEVERT: r = buffer_movevert(buf, m); break;
					case BIND_CALL_INSERT: {
						const void *const p = const_getdata(bc.param);
						if(p)
							buffer_insert(buf, p, strlen(p));
						break;
					}
					case BIND_CALL_DELETE: r = buffer_delete(buf, m); break;
					case BIND_CALL_DELETELINE: r = buffer_deleteline(buf, m); break;
					case BIND_CALL_DELETESELECTION: {
						U32 n;

						if(!buf->nData)
							break;
						if(window->selection > buf->iGap)
							n = window->selection - buf->iGap + 1;
						else {
							n = buf->iGap - window->selection + 1;
							buffer_movecursor(buf, window->selection - buf->iGap);
						}
						buffer_delete(buf, n);
						window->selection = buf->iGap;
						break;
					}
					case BIND_CALL_SETMODE:
						iMode = bc.param;
						if(modes[iMode].flags & FBIND_MODE_SELECTION)
							window->selection = buf->iGap;
						break;
					case BIND_CALL_COPY: {
						char *text;
						U32 nText;

						if(!(modes[iMode].flags & FBIND_MODE_SELECTION) || !buf->nData)
							break;
						if(window->selection > buf->iGap) {
							text = buf->data + buf->iGap + buf->nGap;
							nText = window->selection - buf->iGap + 1;
						} else {
							text = buf->data + window->selection;
							nText = buf->iGap - window->selection + 1;
						}
						clipboard_copy(text, nText);
						break;
					}
					case BIND_CALL_PASTE: {
						char *text;

						if(!clipboard_paste(&text)) {
							buffer_insert(buf, text, strlen(text));
							free(text);
						}
						break;
					}
					case BIND_CALL_UNDO:
						//print_events(buf->events, buf->iEvent, buf->nEvents);
						buffer_undo(buf);
						break;
					case BIND_CALL_REDO:
						buffer_redo(buf);
						break;
					case BIND_CALL_CLOSEWINDOW_RIGHT:
						break;
					case BIND_CALL_CLOSEWINDOW_BELOW:
						break;
					case BIND_CALL_MOVEWINDOW_RIGHT:
						r = 0;
						if(m > 0) {
							for(; r != m && focus_window->right; r++)
								focus_window = focus_window->right;
						} else {
							for(; r != m && focus_window->left; r--)
								focus_window = focus_window->left;
						}
						break;
					case BIND_CALL_MOVEWINDOW_BELOW:
						r = 0;
						if(m > 0) {
							for(; r != m && focus_window->below; r++)
								focus_window = focus_window->below;
						} else {
							for(; r != m && focus_window->above; r--)
								focus_window = focus_window->above;
						}
						break;
					case BIND_CALL_COLOR_TEST:
						break;
					}
					if((bc.flags & FBIND_CALL_AND) && r != m)
						break;
				}
				return 0;
			}
			do ks++; while(*ks);
			ret = !*k ? 1 : ret;
		}
	}
	return ret;
}

