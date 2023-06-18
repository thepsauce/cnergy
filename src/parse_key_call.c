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
	// any char and then blank or comma
	if(isblank(parser->c) || parser->c == ',')
		goto got_key;
	// ^...
	if(key == '^') {
		key = parser->c - ('A' - 1);
		parser_getc(parser);
		goto got_key;
	}
	// **
	if(key == '*' && parser->c == '*') {
		key = -1;
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
	parser->nKeys = 0;
	while(!(r = parser_getkey(parser))) {
		if(parser->c == ',') {
			newKeys = realloc(parser->keys, sizeof(*parser->keys) * (parser->nKeys + 1));
			if(!newKeys) {
				free(parser->keys);
				return OUTOFMEMORY;
			}
			parser->keys = newKeys;
			parser->keys[parser->nKeys++] = 0;
			parser_getc(parser);
		}
		while(isblank(parser->c))
			parser_getc(parser);
	}
	if(r < 0) {
		free(parser->keys);
		return r;
	}
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
parser_getinstr(struct parser *parser)
{
	// TODO: implement this
	return 0;
}

int
parser_getprogram(struct parser *parser)
{
	int r;

	parser->szProgram = 0;
	while(1) {
		while(isblank(parser->c))
			parser_getc(parser);
		if(parser->c == EOF || parser->c == '\n')
			break;
		if((r = parser_getinstr(parser)) < 0)
			return r;
		if(r == FAIL)
			return FAIL;
	}
	return parser->szProgram ? SUCCESS : FAIL;
}

int
parser_addbind(struct parser *parser)
{
	struct binding *newBindings;
	int *keys;
	void *prog;

	newBindings = realloc(parser->curMode->bindings, sizeof(*parser->curMode->bindings) * (parser->curMode->nBindings + 1));
	if(!newBindings)
		return OUTOFMEMORY;
	if(!(keys = mode_allockeys(parser->keys, parser->nKeys)))
		return OUTOFMEMORY;
	if(!(prog = malloc(parser->szProgram)))
		return OUTOFMEMORY;
	parser->curMode->bindings = newBindings;
	parser->curMode->bindings[parser->curMode->nBindings++] = (struct binding) {
		.nKeys = parser->nKeys,
		.szProgram = parser->szProgram,
		.keys = keys,
		.program = prog,
	};
	return FINISH;
}
