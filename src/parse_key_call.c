#include "cnergy.h"

int
parser_getkey(struct parser *parser)
{
	int r;
	int key;
	int *newKeys;

	static const struct {
		const char *word;
		int key;
	} keyTranslations[] = {
		{ "HOME", KEY_HOME },
		{ "END", KEY_END },
		{ "LEFT", KEY_LEFT },
		{ "RIGHT", KEY_RIGHT },
		{ "UP", KEY_UP},
		{ "DOWN", KEY_DOWN },
		{ "SPACE", ' ' },
		{ "BACKSPACE", KEY_BACKSPACE },
		{ "BACK", KEY_BACKSPACE },
		{ "DELETE", KEY_DC },
		{ "DEL", KEY_DC },
		{ "TAB", '\t' },
		{ "ENTER", '\n' },
		{ "ESCAPE", 0x1b },
		{ "ESC", 0x1b },
	};
	key = parser->c;
	parser_getc(parser);
	if(isblank(parser->c) || parser->c == ',')
		goto got_key;
	if(key == '^') {
		key = parser->c - ('A' - 1);
		parser_getc(parser);
		goto got_key;
	}
	parser_ungetc(parser);
	r = parser_getword(parser);
	if(r)
		return r;
	for(unsigned i = 0; i < ARRLEN(keyTranslations); i++)
		if(!strcasecmp(keyTranslations[i].word, parser->word)) {
			key = keyTranslations[i].key;
			goto got_key;
		}
	return FAIL;
got_key:
	newKeys = realloc(parser->keys, sizeof(*parser->keys) * (parser->nKeys + 1));
	if(!newKeys)
		return OUTOFMEMORY;
	parser->keys = newKeys;
	parser->keys[parser->nKeys++] = key;
	return SUCCESS;
}

int
parser_getkeylist(struct parser *parser)
{
	int r;
	int *newKeys;

	if(!parser->curMode) {
		parser_pusherror(parser, ERR_BIND_OUTSIDEOF_MODE);
		return FAIL;
	}
	parser->keys = NULL;
	parser->nKeys = 0;
	while(!(r = parser_getkey(parser))) {
		if(parser->c == ',') {
			newKeys = realloc(parser->keys, sizeof(*parser->keys) * (parser->nKeys + 1));
			if(!newKeys)
				return OUTOFMEMORY;
			parser->keys = newKeys;
			parser->keys[parser->nKeys++] = 0;
			parser_getc(parser);
		}
		while(isblank(parser->c))
			parser_getc(parser);
	}
	if(r < 0)
		return r;
	if(!parser->nKeys)
		return FAIL;
	newKeys = realloc(parser->keys, sizeof(*parser->keys) * (parser->nKeys + 1));
	if(!newKeys)
		return OUTOFMEMORY;
	parser->keys = newKeys;
	parser->keys[parser->nKeys++] = 0;
	return COMMIT;
}

int
parser_getcall(struct parser *parser)
{
	// TODO: when this list is complete, add binary search or hashing (same goes for translate_key)
	static const struct {
		const char *word;
		bind_call_t type;
	} namedCommands[] = {
		{ "COLOR", BIND_CALL_COLORWINDOW },
		{ "CLOSE", BIND_CALL_CLOSEWINDOW },
		{ "NEW", BIND_CALL_NEWWINDOW },
		{ "OPEN", BIND_CALL_OPENWINDOW },
		{ "HSPLIT", BIND_CALL_HSPLIT },
		{ "VSPLIT", BIND_CALL_VSPLIT },
		{ "QUIT", BIND_CALL_QUIT },

		{ "DELETE", BIND_CALL_DELETESELECTION },
		{ "COPY", BIND_CALL_COPY },
		{ "PASTE", BIND_CALL_PASTE },
		{ "UNDO", BIND_CALL_UNDO },
		{ "REDO", BIND_CALL_REDO },
		{ "WRITE", BIND_CALL_WRITEFILE },
		{ "READ", BIND_CALL_READFILE },

		{ "CHOOSE", BIND_CALL_CHOOSE },
		{ "SORT_ALPHABETICAL", BIND_CALL_SORTALPHABETICAL },
		{ "SORT_CHANGETIME", BIND_CALL_SORTCHANGETIME },
		{ "SORT_MODIFICATIONTIME", BIND_CALL_SORTMODIFICATIONTIME },
		{ "TOGGLE_SORT_TYPE", BIND_CALL_TOGGLESORTTYPE },
		{ "TOGGLE_HIDDEN", BIND_CALL_TOGGLEHIDDEN },
	};
	struct binding_call *newCalls;

	newCalls = realloc(parser->calls, sizeof(*parser->calls) * (parser->nCalls + 1));
	if(!newCalls)
		return OUTOFMEMORY;
	parser->calls = newCalls;
	switch(parser->c) {
	case '|':
	case '&':
	case '^':
		parser->calls[parser->nCalls].flags =
			parser->c == '|' ? FBIND_CALL_OR :
			parser->c == '&' ? FBIND_CALL_AND : FBIND_CALL_XOR;
		while(isblank(parser_getc(parser)));
		break;
	default:
		parser->calls[parser->nCalls].flags = 0;
	}
	switch(parser->c) {
	case '(':
		if(parser_getc(parser) != '(') {
			parser_pusherror(parser, ERR_INVALID_COMMAND);
			return FAIL;
		}
		parser_getc(parser);
		if(parser->nLoopStack == ARRLEN(parser->loopStack)) {
			parser_pusherror(parser, ERR_LOOP_TOO_DEEP);
			return FAIL;
		}
		if(parser->calls[parser->nCalls].flags & FBIND_CALL_XOR) {
			parser_pusherror(parser, ERR_XOR_LOOP);
			return FAIL;
		}
		parser->loopStack[parser->nLoopStack++] = parser->nCalls;
		parser->calls[parser->nCalls].type = BIND_CALL_STARTLOOP;
		break;
	case ')':
		if(parser_getc(parser) != ')') {
			parser_pusherror(parser, ERR_INVALID_COMMAND);
			return FAIL;
		}
		parser_getc(parser);
		if(!parser->nLoopStack) {
			parser_pusherror(parser, ERR_END_OF_LOOP_WITHOUT_START);
			return FAIL;
		}
		if(parser->calls[parser->nCalls].flags & FBIND_CALL_XOR) {
			parser_pusherror(parser, ERR_XOR_LOOP);
			return FAIL;
		}
		parser->calls[parser->nCalls].param = parser->nCalls - parser->loopStack[--parser->nLoopStack];
		if(!parser->calls[parser->nCalls].param) {
			parser_pusherror(parser, ERR_EMPTY_LOOP);
			return FAIL;
		}
		parser->calls[parser->nCalls].type = BIND_CALL_ENDLOOP;
		break;
	case '#':
		parser_getc(parser);
		parser->calls[parser->nCalls].type = BIND_CALL_REGISTER;
		if(parser_getnumber(parser) != SUCCESS) {
			if(parser_getword(parser) != SUCCESS) {
				parser_pusherror(parser, ERR_EXPECTED_PARAM);
				break;
			}
			if(!strcasecmp(parser->word, "N")) {
				parser->num = 1;
				parser->calls[parser->nCalls].flags = FBIND_CALL_USENUMBER;
			} else {
				parser_pusherror(parser, ERR_INVALID_PARAM);
				break;
			}
		} else {
			if(parser_getword(parser) == SUCCESS) {
				if(!strcmp(parser->word, "N"))
					parser->calls[parser->nCalls].flags = FBIND_CALL_USENUMBER;
				else
					parser_pusherror(parser, ERR_INVALID_PARAM);
			}
		}
		parser->calls[parser->nCalls].param = parser->num;
		break;
	case '$':
	case '`':
	case '.':
	case '<':
	case '>': {
		bool del = false;
		bool window = false;
		const int sign = parser->c == '<' || parser->c == '`' ? -1 : 1;
		const bool horz = parser->c == '<' || parser->c == '>';
		const bool vert = parser->c == '.' || parser->c == '`';

		parser_getc(parser);
		if(parser->c == 'x') {
			del = true;
			parser_getc(parser);
		}
		if(parser->c == 'w') {
			window = true;
			parser_getc(parser);
		}
		if(parser_getnumber(parser) == FAIL) {
			if(parser_getword(parser) != SUCCESS) {
				parser_pusherror(parser, ERR_EXPECTED_PARAM);
				break;
			}
			if(!strcasecmp(parser->word, "home"))
				parser->num = INT_MIN;
			else if(!strcasecmp(parser->word, "end"))
				parser->num = INT_MAX;
			else if(!strcasecmp(parser->word, "N")) {
				parser->num = 1;
				parser->calls[parser->nCalls].flags = FBIND_CALL_USENUMBER;
			} else {
				parser_pusherror(parser, ERR_INVALID_PARAM);
				break;
			}
		} else {
			if(parser_getword(parser) == SUCCESS) {
				if(!strcmp(parser->word, "N"))
					parser->calls[parser->nCalls].flags = FBIND_CALL_USENUMBER;
				else
					parser_pusherror(parser, ERR_INVALID_PARAM);
			}
		}
		if(window) {
			if(del) {
				parser_pusherror(parser, ERR_WINDOW_DEL);
				break;
			}
			parser->calls[parser->nCalls].type = vert ? BIND_CALL_MOVEWINDOW_BELOW : BIND_CALL_MOVEWINDOW_RIGHT;
		} else if(del) {
			parser->calls[parser->nCalls].type = vert ? BIND_CALL_DELETELINE : BIND_CALL_DELETE;
		} else {
			parser->calls[parser->nCalls].type = vert ? BIND_CALL_MOVEVERT : horz ? BIND_CALL_MOVEHORZ : BIND_CALL_MOVECURSOR;
		}
		parser->calls[parser->nCalls].param = SAFE_MUL(sign, parser->num);
		break;
	}
	case '?':
	case '+': {
		parser->calls[parser->nCalls].type = parser->c == '+' ? BIND_CALL_INSERT : BIND_CALL_ASSERT;
		parser_getc(parser);
		if(parser_getstring(parser) < 0)
			return OUTOFMEMORY;
		char *const str = const_alloc(parser->str, parser->nStr);
		if(!str)
			return OUTOFMEMORY;
		parser->calls[parser->nCalls].str = str;
		break;
	}
	case '!':
	case ':': {
		const bool setMode = parser->c == ':';
		parser_getc(parser);
		if(parser_getword(parser) != SUCCESS) {
			parser_pusherror(parser, ERR_EXPECTED_PARAM);
			return FAIL;
		}
		if(setMode) {
			parser->calls[parser->nCalls].type = BIND_CALL_SETMODE;
			parser->calls[parser->nCalls].str = const_alloc(parser->word, strlen(parser->word) + 1);
		} else {
			unsigned i;
			for(i = 0; i < ARRLEN(namedCommands); i++)
				if(!strcasecmp(namedCommands[i].word, parser->word)) {
					parser->calls[parser->nCalls].type = namedCommands[i].type;
					break;
				}
			if(i == ARRLEN(namedCommands)) {
				parser_pusherror(parser, ERR_INVALID_COMMAND);
				return FAIL;
			}
		}
		break;
	}
	default:
		parser_pusherror(parser, ERR_INVALID_COMMAND);
		return FAIL;
	}
	parser->nCalls++;
	return SUCCESS;
}

int
parser_getcalllist(struct parser *parser)
{
	int r;

	parser->calls = NULL;
	parser->nCalls = 0;
	while(1) {
		while(isblank(parser->c))
			parser_getc(parser);
		if(parser->c == EOF || parser->c == '\n')
			break;
		if((r = parser_getcall(parser)) < 0)
			return r;
	}
	return parser->nCalls ? SUCCESS : FAIL;
}

int
parser_addbind(struct parser *parser)
{
	struct binding *newBindings;

	newBindings = realloc(parser->curMode->bindings, sizeof(*parser->curMode->bindings) * (parser->curMode->nBindings + 1));
	if(!newBindings)
		return OUTOFMEMORY;
	parser->curMode->bindings = newBindings;
	parser->curMode->bindings[parser->curMode->nBindings++] = (struct binding) {
		.nKeys = parser->nKeys,
		.nCalls = parser->nCalls,
		.keys = parser->keys,
		.calls = parser->calls,	
	};
	return FINISH;
}
