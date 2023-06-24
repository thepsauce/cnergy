#include "cnergy.h"

static inline int
parser_getkeys(struct parser *parser)
{
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
	parser->nKeys = 0;
	while(!isspace(parser->c)) {
		if(parser->c == EOF)
			break;
		key = parser->c;
		parser_getc(parser);
		switch(key) {
		case '\\':
			key = parser->c;
			switch(key) {
			case EOF:
				return parser_pusherror(parser, ERRK_EOF_BACKSLASH);
			case 'a': key = '\a'; break;
			case 'b': key = '\b'; break;
			case 'e': key = 0x1b; break;
			case 'f': key = '\f'; break;
			case 'n': key = '\n'; break;
			case 'r': key = '\r'; break;
			case 't': key = '\t'; break;
			case 'v': key = '\v'; break;
			}
			break;
		case '^':
			key -= 'A' - 1;
			break;
		case '*':
			if(parser->c != '*') {
				key = '*';
				break;
			}
			key = -1;
			break;
		case '<': {
			unsigned i;

			if(parser_getword(parser) != SUCCESS)
				return parser_pusherror(parser, ERRK_EXPECTED_WORD);
			for(i = 0; i < ARRLEN(keyTranslations); i++)
				if(!strcasecmp(keyTranslations[i].word, parser->word)) {
					key = keyTranslations[i].key;
					break;
				}
			if(parser->c != '>')
				return parser_pusherror(parser, ERRK_MISSING_CLOSING_ANGLE_BRACKET);
			parser_getc(parser);
			if(i == ARRLEN(keyTranslations))
				return parser_pusherror(parser, ERRK_INVALID);
			break;
		}
		case ',':
			if(isspace(parser->c)) {
				while(isspace(parser_getc(parser)));
				key = 0;
			}
			break;
		}
		newKeys = realloc(parser->keys, sizeof(*parser->keys) * (parser->nKeys + 1));
		if(!newKeys)
			return ERRK_ALLOCATING;
		parser->keys = newKeys;
		parser->keys[parser->nKeys++] = key;
	}
	newKeys = realloc(parser->keys, sizeof(*parser->keys) * (parser->nKeys + 1));
	if(!newKeys)
		return ERRK_ALLOCATING;
	parser->keys = newKeys;
	parser->keys[parser->nKeys++] = 0;
	return SUCCESS;
}

static int
parser_getintparam(struct parser *parser, ptrdiff_t mul)
{
	if(parser_getword(parser) == SUCCESS) {
		if(parser->word[1] == '\0') {
			switch(parser->word[0]) {
			case 'A': break;
			case 'B':
				*(instr_t*) (parser->program + parser->szProgram) = INSTR_LDAB;
				parser->szProgram += sizeof(instr_t);
				break;
			case 'C':
				*(instr_t*) (parser->program + parser->szProgram) = INSTR_LDAC;
				parser->szProgram += sizeof(instr_t);
				break;
			case 'D':
				*(instr_t*) (parser->program + parser->szProgram) = INSTR_LDAD;
				parser->szProgram += sizeof(instr_t);
				break;
			default:
				return parser_pusherror(parser, ERRIP_REGISTER);
			}
		} else if(!strcmp(parser->word, "home")) {
			*(instr_t*) (parser->program + parser->szProgram) = INSTR_LDA;
			parser->szProgram += sizeof(instr_t);
			*(ptrdiff_t*) (parser->program + parser->szProgram) = PTRDIFF_MIN;
			parser->szProgram += sizeof(ptrdiff_t);
		} else if(!strcmp(parser->word, "end")) {
			*(instr_t*) (parser->program + parser->szProgram) = INSTR_LDA;
			parser->szProgram += sizeof(instr_t);
			*(ptrdiff_t*) (parser->program + parser->szProgram) = PTRDIFF_MAX;
			parser->szProgram += sizeof(ptrdiff_t);
		} else if(!strcmp(parser->word, "key")) {
			if(parser->c != '#')
				return parser_pusherror(parser, ERRIP_HASH);
			parser_getc(parser);
			*(instr_t*) (parser->program + parser->szProgram) = INSTR_LDAINT;
			parser->szProgram += sizeof(instr_t);
			if(parser_getnumber(parser) != SUCCESS)
				return parser_pusherror(parser, ERRIP_NUMBER);
			*(ptrdiff_t*) (parser->program + parser->szProgram) = (ptrdiff_t) main_environment.memory + sizeof(int) * parser->num;
			parser->szProgram += sizeof(ptrdiff_t);
		} else {
			return parser_pusherror(parser, ERRIP_WORD);
		}
	} else if(parser_getnumber(parser) == SUCCESS) {
		*(instr_t*) (parser->program + parser->szProgram) = INSTR_LDA;
		parser->szProgram += sizeof(instr_t);
		*(ptrdiff_t*) (parser->program + parser->szProgram) = parser->num;
		parser->szProgram += sizeof(ptrdiff_t);
	} else
		return FAIL;
	if(mul != 1) {
		*(instr_t*) (parser->program + parser->szProgram) = INSTR_MULA;
		parser->szProgram += sizeof(instr_t);
		*(ptrdiff_t*) (parser->program + parser->szProgram) = mul;
		parser->szProgram += sizeof(ptrdiff_t);
	}
	return SUCCESS;
}

static int
parser_getstringparam(struct parser *parser)
{
	int r;

	if((r = parser_getstring(parser)))
		return r;
	char *const str = mode_allocstring(parser->str, parser->nStr);
	if(!str)
		return parser_pusherror(parser, ERR_OUTOFMEMORY);
	*(instr_t*) (parser->program + parser->szProgram) = INSTR_LDA;
	parser->szProgram += sizeof(instr_t);
	*(ptrdiff_t*) (parser->program + parser->szProgram) = (ptrdiff_t) str;
	parser->szProgram += sizeof(ptrdiff_t);
	return SUCCESS;
}

static inline int
parser_getinstruction(struct parser *parser)
{
	void *newProgram;
	int r;

	newProgram = realloc(parser->program, parser->szProgram + 128);
	if(!newProgram)
		return parser_pusherror(parser, ERR_OUTOFMEMORY);
	parser->program = newProgram;
	switch(parser->c) {
	case 'A' ... 'Z': case 'a' ... 'z': case '_': {
		instr_t instr;

		parser_getword(parser);
		if(parser->c == ':') { // label declaration
			struct parser_label *newLabels;

			newLabels = realloc(parser->labels, sizeof(*parser->labels) * (parser->nLabels + 1));
			if(!newLabels)
				return parser_pusherror(parser, ERR_OUTOFMEMORY);
			parser->labels = newLabels;
			strcpy(parser->labels[parser->nLabels].name, parser->word);
			parser->labels[parser->nLabels].address = parser->szProgram;
			parser->nLabels++;
			parser_getc(parser);
			break;
		}
		for(instr = 0; instr < INSTR_MAX; instr++)
			if(!strcasecmp(parser->word, instrNames[instr])) {
				*(instr_t*) (parser->program + parser->szProgram) = instr;
				parser->szProgram += sizeof(instr_t);
				// uhhh, special handling for all instruction that need it I guess...
				if(!strcasecmp(parser->word, "JMP") || !strcasecmp(parser->word, "JZ")) {
					struct parser_label_request *newLabelRequests;

					while(isblank(parser->c))
						parser_getc(parser);
					if(parser_getword(parser))
						return parser_pusherror(parser, ERRINSTR_JMPWORD);
					newLabelRequests = realloc(parser->labelRequests, sizeof(*parser->labelRequests) * (parser->nLabelRequests + 1));
					if(!newLabelRequests)
						return parser_pusherror(parser, ERR_OUTOFMEMORY);
					parser->labelRequests = newLabelRequests;
					strcpy(parser->labelRequests[parser->nLabelRequests].name, parser->word);
					parser->labelRequests[parser->nLabelRequests].address = parser->szProgram;
					parser->nLabelRequests++;
					*(ptrdiff_t*) (parser->program + parser->szProgram) = 0;
					parser->szProgram += sizeof(ptrdiff_t);
				}
				break;
			}
		if(instr == INSTR_MAX)
			return parser_pusherror(parser, ERRINSTR_INVALIDNAME);
		break;
	}
	case '&': {
		struct parser_label_request *newLabelRequests;

		parser_getc(parser);
		newLabelRequests = realloc(parser->labelRequests, sizeof(*parser->labelRequests) * (parser->nLabelRequests + 1));
		if(!newLabelRequests)
			return parser_pusherror(parser, ERR_OUTOFMEMORY);
		parser->labelRequests = newLabelRequests;
		parser->labelRequests[parser->nLabelRequests].name[0] = 0;
		parser->labelRequests[parser->nLabelRequests].address = parser->szProgram;
		parser->nLabelRequests++;
		break;
	}
	case '`':
	case '.':
	case '<':
	case '>':
	case '$': {
		bool del = false;
		bool window = false;
		const ptrdiff_t sign = parser->c == '<' || parser->c == '`' ? -1 : 1;
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
		if((r = parser_getintparam(parser, sign)) == FAIL)
			return parser_pusherror(parser, ERRINSTR_INVALIDIP);
		else if(r)
			return r;
		*(instr_t*) (parser->program + parser->szProgram) = INSTR_CALL;
		parser->szProgram += sizeof(instr_t);
		if(window) {
			if(del)
				return parser_pusherror(parser, ERRINSTR_WINDOW_DEL);
			*(call_t*) (parser->program + parser->szProgram) = vert ? CALL_MOVEWINDOW_BELOW : CALL_MOVEWINDOW_RIGHT;
		} else if(del) {
			*(call_t*) (parser->program + parser->szProgram) = vert ? CALL_DELETELINE : CALL_DELETE;
		} else {
			*(call_t*) (parser->program + parser->szProgram) = vert ? CALL_MOVEVERT : horz ? CALL_MOVEHORZ : CALL_MOVECURSOR;
		}
		parser->szProgram += sizeof(call_t);
		break;
	}
	case '?':
	case '+': {
		const bool assert = parser->c == '?';
		if(parser_getc(parser) != '\"') {
			if((r = parser_getintparam(parser, 1)) == FAIL)
				return parser_pusherror(parser, ERRINSTR_INVALIDIP);
			else if(r)
				return r;
			*(instr_t*) (parser->program + parser->szProgram) = INSTR_CALL;
			parser->szProgram += sizeof(instr_t);
			*(call_t*) (parser->program + parser->szProgram) = assert ? CALL_ASSERTCHAR : CALL_INSERTCHAR;
			parser->szProgram += sizeof(call_t);
			break;
		}
		if((r = parser_getstringparam(parser)) == FAIL)
			return r;
		*(instr_t*) (parser->program + parser->szProgram) = INSTR_CALL;
		parser->szProgram += sizeof(instr_t);
		*(call_t*) (parser->program + parser->szProgram) = assert ? CALL_ASSERT : CALL_INSERT;
		parser->szProgram += sizeof(call_t);
		break;
	}
	case ':': {
		parser_getc(parser);
		if(parser_getword(parser) == FAIL)
			return parser_pusherror(parser, ERRINSTR_COLONWORD);
		char *const mode = mode_allocstring(parser->word, strlen(parser->word));
		if(!mode)
			return parser_pusherror(parser, ERR_OUTOFMEMORY);
		*(instr_t*) (parser->program + parser->szProgram) = INSTR_LDA;
		parser->szProgram += sizeof(instr_t);
		*(ptrdiff_t*) (parser->program + parser->szProgram) = (ptrdiff_t) mode;
		parser->szProgram += sizeof(ptrdiff_t);
		*(instr_t*) (parser->program + parser->szProgram) = INSTR_CALL;
		parser->szProgram += sizeof(instr_t);
		*(call_t*) (parser->program + parser->szProgram) = CALL_SETMODE;
		parser->szProgram += sizeof(call_t);
		break;
	}
	default:
		return parser_pusherror(parser, ERRINSTR_INVALID);
	}
	return SUCCESS;
}

int
parser_getbind(struct parser *parser)
{
	int r;
	bool multi = true;
	struct binding *newBindings;
	int *keys;
	void *prog;

	if((r = parser_getkeys(parser)))
		return r;
	while(isblank(parser->c))
		parser_getc(parser);
	if(parser->c == '{') {
		multi = true;
		parser_getc(parser);
	}
	parser->szProgram = 0;
	while(parser->c != EOF) {
		while(isblank(parser->c))
			parser_getc(parser);
		if(parser->c == ';') {
			parser_getc(parser);
			continue;
		}
		if(parser->c == '}') {
			parser_getc(parser);
			if(multi) {
				multi = false;
				break;
			}
			return parser_pusherror(parser, ERRBIND_MISSING_OPENING);
		}
		if(parser->c == '\n') {
			parser_getc(parser);
			if(multi)
				continue;
			break;
		}
		if((r = parser_getinstruction(parser)))
			return r;
	}
	if(multi)
		return parser_pusherror(parser, ERRBIND_MISSING_CLOSING);
	/* resolve accumulated label requests */
	for(size_t i = 0; i < parser->nLabelRequests; i++) {
		size_t j = 0;

		if(!parser->labelRequests[i].name[0]) { // end of program was requested
			*(ptrdiff_t*) (parser->program + parser->labelRequests[i].address) =
				parser->szProgram - parser->labelRequests[i].address;
			continue;
		}
		for(j = 0; j < parser->nLabels; j++)
			if(!strcmp(parser->labelRequests[i].name, parser->labels[j].name)) {
				*(ptrdiff_t*) (parser->program + parser->labelRequests[i].address) =
					parser->labels[j].address - parser->labelRequests[i].address;
				break;
			}
		if(j == parser->nLabels)
			parser_pusherror(parser, ERRBIND_LABEL);
	}
	/* allocate the keys, program etc. for permanency */
	newBindings = realloc(parser->curMode->bindings, sizeof(*parser->curMode->bindings) * (parser->curMode->nBindings + 1));
	if(!newBindings)
		return parser_pusherror(parser, ERR_OUTOFMEMORY);
	if(!(keys = mode_allockeys(parser->keys, parser->nKeys)))
		return parser_pusherror(parser, ERR_OUTOFMEMORY);
	if(!(prog = mode_allocprogram(parser->program, parser->szProgram)))
		return parser_pusherror(parser, ERR_OUTOFMEMORY);
	parser->curMode->bindings = newBindings;
	parser->curMode->bindings[parser->curMode->nBindings++] = (struct binding) {
		.nKeys = parser->nKeys,
		.szProgram = parser->szProgram,
		.keys = keys,
		.program = prog,
	};
	return SUCCESS;
}
