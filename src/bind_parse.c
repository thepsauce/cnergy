#include "cnergy.h"

/**
 * Most static inline functions here either return 0 (success), 1 (couldn't continue) or -1 (internal error)
 */

/**
 * Fatal internal errors have a negative value
 * Success has the value of 0
 * When a function has to abort it returns 1 (SKIP)
 * When a function has to abort and it's not possible to recover, it returns 2 (MUSTSKIP)
 */
enum {
	OUTOFMEMORY = -1,
	SUCCESS,
	SKIP,
	MUSTSKIP,
};

/**
 * Errors cause the parser to throw all away after it finished, warnings just give information
 */
enum {
	WARN_,
	WARN_MAX,

	ERR_INVALID_KEY,
	ERR_INVALID_COMMAND,
	ERR_EXPECTED_COLON_MODE,
	ERR_NEED_SPACE_KEYS,
	ERR_EXPECTED_LINE_BREAK_MODE,
	ERR_WORD_TOO_LONG,
	ERR_INVALID_KEY_NAME,
	ERR_BIND_OUTSIDEOF_MODE,
	ERR_BIND_NEEDS_MODE,
	ERR_EXPECTED_PARAM,
	ERR_INVALID_PARAM,
	ERR_WINDOW_DEL,
	ERR_INVALID_COMMAND_NAME,
	ERR_LOOP_TOO_DEEP,
	ERR_END_OF_LOOP_WITHOUT_START,
	ERR_EMPTY_LOOP,
};

static const char *error_messages[] = {
	[ERR_INVALID_KEY] = "invalid key",
	[ERR_INVALID_COMMAND] = "invalid command",
	[ERR_WORD_TOO_LONG] = "word is too long",
	[ERR_EXPECTED_LINE_BREAK_MODE] = "expected a line break after mode",
	[ERR_EXPECTED_COLON_MODE] = "expected a colon after a mode name",
	[ERR_NEED_SPACE_KEYS] = "need a space in between keys",
	[ERR_BIND_OUTSIDEOF_MODE] = "bind is outside of any mode",
	[ERR_BIND_NEEDS_MODE] = "a bind must be below a mode e.g. 'mode:\\n...'",
	[ERR_EXPECTED_PARAM] = "expected parameter after command",
	[ERR_INVALID_PARAM] = "invalid parameter after command",
	[ERR_WINDOW_DEL] = "can't use x with w",
	[ERR_LOOP_TOO_DEEP] = "loop is too deep",
	[ERR_END_OF_LOOP_WITHOUT_START] = "reached an end of a loop but there is no start",
	[ERR_EMPTY_LOOP] = "loop cannot be empty",
};

struct bind_parser {
	FILE *fp;
	int c, prev_c;
	int num;
	char word[64];
	char *str;
	U32 nStr;
	U32 nModes;
	struct binding_mode *modes;
	// the requests are necessary to allow usage before declaration
	// append requests are added when a mode should be extended by another
	struct append_request {
		U32 receiver;
		char donor[64];
	} *appendRequests;
	U32 nAppendRequests;
	int *keys;
	U32 nKeys;
	// param requests are added when a mode is used that is not yet defined
	struct param_request {
		U32 mode;
		U32 major, minor;
		char target[64];
	} *paramRequests;
	U32 nParamRequests;
	int loopStack[32];
	int nLoopStack;
	struct binding_call *calls;
	U32 nCalls;
	struct {
		int err;
		long pos;
	} errStack[32];
	int nErrStack;
	int nErrors;
	int nWarnings;
};

/** Read the next character from the file */
static inline int
parser_getc(struct bind_parser *parser)
{
	parser->prev_c = parser->c;
	return parser->c = fgetc(parser->fp);
}

/** Unread the last character read */
static inline void
parser_ungetc(struct bind_parser *parser)
{
	ungetc(parser->c, parser->fp);
	parser->c = parser->prev_c;
}

static void
parser_pusherror(struct bind_parser *parser, int err)
{
	if(parser->nErrStack == ARRLEN(parser->errStack)) {
		memmove(parser->errStack, parser->errStack + 1, sizeof(parser->errStack) - sizeof(*parser->errStack));
		parser->nErrStack--;
	}
	parser->errStack[parser->nErrStack].err = err;
	parser->errStack[parser->nErrStack].pos = ftell(parser->fp) - 1;
	parser->nErrStack++;
	if(err > WARN_MAX)
		parser->nErrors++;
	else
		parser->nWarnings++;
}

static inline int
parser_addappendrequest(struct bind_parser *parser, struct append_request *request)
{
	struct append_request *newRequests;

	newRequests = realloc(parser->appendRequests, sizeof(*parser->appendRequests) * (parser->nAppendRequests + 1));
	if(!newRequests)
		return OUTOFMEMORY;
	parser->appendRequests = newRequests;
	parser->appendRequests[parser->nAppendRequests++] = *request;
	return SUCCESS;
}

static inline int
parser_addparamrequest(struct bind_parser *parser, struct param_request *request)
{
	struct param_request *newRequests;

	newRequests = realloc(parser->paramRequests, sizeof(*parser->paramRequests) * (parser->nParamRequests + 1));
	if(!newRequests)
		return OUTOFMEMORY;
	parser->paramRequests = newRequests;
	parser->paramRequests[parser->nParamRequests++] = *request;
	return SUCCESS;
}

/**
 * Read the next word, must have a character ready before calling
 *
 * Examples of words:
 * hello
 * bana_na
 * __bread_12
 **/
static inline int
read_word(struct bind_parser *parser)
{
	int c;
	U32 nWord = 0;

	c = parser->c;
	if(!isalpha(c) && c != '_')
		return SKIP;
	do {
		if(nWord == sizeof(parser->word) - 1) {
			parser_pusherror(parser, ERR_WORD_TOO_LONG);
			nWord--;
		}
		parser->word[nWord++] = c;
		c = parser_getc(parser);
	} while(isalpha(c) || c == '_');
	parser->word[nWord] = 0;
	return SUCCESS;
}

/**
 * Read the next number, must have a character ready before calling
 *
 * Examples of numbers:
 * +
 * -
 * +123
 * 948
 * -2
 */
static inline int
read_number(struct bind_parser *parser)
{
	int c;
	int num = 0;
	int sign = 1;

	c = parser->c;
	if(c == '+') {
		sign = 1;
		c = parser_getc(parser);
	} else if(c == '-') {
		sign = -1;
		c = parser_getc(parser);
	} else if(!isdigit(c))
		return SKIP;
	if(!isdigit(c))
		num = 1;
	else
		do {
			num = SAFE_MUL(num, 10);
			num = SAFE_ADD(num, c - '0');
		} while(isdigit(c = parser_getc(parser)));
	parser->num = sign == -1 && num == INT32_MIN ? INT32_MAX : sign * num;
	parser->c = c;
	return SUCCESS;
}

/**
 * Read the next string, must have a character ready before calling
 *
 * Examples of strings:
 * "Hello there"
 * WithoutQuotes
 * \n
 * "Long\tstring\nWith\tEscape\tcharacters"
 * "\""
 * "no second quote
 */
static inline int
read_string(struct bind_parser *parser)
{
	int c;
	char *str;
	U32 nStr = 0;

	c = parser->c;
	if(isspace(c))
		return SKIP;
	str = parser->str;
	const bool quoted = c == '\"';
	if(quoted)
		c = parser_getc(parser);
	while(1) {
		if(c == EOF ||
			(quoted && (c == '\"' || c == '\n')) ||
			(!quoted && isspace(c)))
			break;
		str = realloc(str, nStr + 1);
		if(!str)
			return OUTOFMEMORY;
		parser->str = str; // need to update this after every realloc call, otherwise parser->str would become invalid
		if(c == '\\') {
			c = parser_getc(parser);
			switch(c) {
			case 'a': c = '\a'; break;
			case 'b': c = '\b'; break;
			case 'e': c = '\e'; break;
			case 'f': c = '\f'; break;
			case 'n': c = '\n'; break;
			case 'r': c = '\r'; break;
			case 't': c = '\t'; break;
			case 'v': c = '\v'; break;
			}
		}
		str[nStr++] = c;
		c = parser_getc(parser);
	}
	if(quoted && c == '\"')
		c = parser_getc(parser);
	// add null terminator
	str = realloc(str, nStr + 1);
	if(!str)
		return OUTOFMEMORY;
	str[nStr++] = 0;
	parser->str = str;
	parser->nStr = nStr;
	parser->c = c;
	return SUCCESS;
}

/** Skip comments and whitespace */
static inline int
parser_skip(struct bind_parser *parser)
{
	int c;

	do {
		c = parser_getc(parser);
		if(c == '#') {
			c = parser_getc(parser);
			if(c != '#') {
				parser_ungetc(parser);
				return SUCCESS;
			}
			while((c = parser_getc(parser)) != '\n' && c != EOF);
		}
		if(c == EOF)
			return SKIP;
	} while(isspace(c));
	return SUCCESS;
}

/**
 * Reads a mode and adds it to parser->modes
 *
 * Examples:
 * $insert:
 * normal:
 * .supp:
 * visual*:
 */
static inline int
read_mode(struct bind_parser *parser)
{
	struct binding_mode mode;
	struct binding_mode *newModes;
	const long pos = ftell(parser->fp);
	const int c = parser->c;
	const int prev_c = parser->prev_c;

	memset(&mode, 0, sizeof(mode));
	if(c == '.' || c == '$') {
		mode.flags = c == '.' ? FBIND_MODE_SUPPLEMENTARY : FBIND_MODE_TYPE;
		if(isblank(parser_getc(parser)))
			goto no_mode;
	}
	if(read_word(parser) == SUCCESS) {
		if(parser->c == '*') {
			mode.flags |= FBIND_MODE_SELECTION;
			parser_getc(parser);
		}
		if(parser->c != ':') {
			if(!mode.flags)
				goto no_mode;
			parser_pusherror(parser, ERR_EXPECTED_COLON_MODE);
			return MUSTSKIP;
		}
		strcpy(mode.name, parser->word);
		while(isblank(parser_getc(parser)));
		while(read_word(parser) == SUCCESS) {
			struct append_request req;

			req.receiver = parser->nModes;
			strcpy(req.donor, parser->word);
			parser_addappendrequest(parser, &req);
			while(isblank(parser->c))
				parser_getc(parser);
			if(parser->c != ',')
				break;
			while(isblank(parser_getc(parser)));
		}
		if(parser->c != '\n' && c != EOF) {
			parser_pusherror(parser, ERR_EXPECTED_LINE_BREAK_MODE);
			return MUSTSKIP;
		}
		newModes = realloc(parser->modes, sizeof(*parser->modes) * (parser->nModes + 1));
		if(!newModes)
			return OUTOFMEMORY;
		parser->modes = newModes;
		parser->modes[parser->nModes++] = mode;
		return SUCCESS;
	}
	if(mode.flags) {
		parser_pusherror(parser, ERR_NEED_SPACE_KEYS);
		return MUSTSKIP;
	}
no_mode:
	fseek(parser->fp, pos, SEEK_SET);
	parser->c = c;
	parser->prev_c = prev_c;
	return SKIP;
}

/**
 * Reads a key and adds it to the key list inside of parser
 *
 * Examples:
 * home
 * 4
 * a
 * >
 * ^A
 * ^
 *
 * So either there is a word, a character with a following blank or a character with a leading '^'
 */
static inline int
read_key(struct bind_parser *parser)
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
		{ "ESCAPE", 27 },
		{ "ESC", 27 },
	};
	key = parser->c;
	if(isblank(parser_getc(parser)) || parser->c == ',')
		goto got_key;
	if(key == '^') {
		key = parser->c - ('A' - 1);
		parser_getc(parser);
		goto got_key;
	}
	parser_ungetc(parser);
	r = read_word(parser);
	if(r)
		return r;
	for(U32 i = 0; i < ARRLEN(keyTranslations); i++)
		if(!strcasecmp(keyTranslations[i].word, parser->word)) {
			key = keyTranslations[i].key;
			goto got_key;
		}
	return SKIP;
got_key:
	newKeys = realloc(parser->keys, sizeof(*parser->keys) * (parser->nKeys + 1));
	if(!newKeys)
		return OUTOFMEMORY;
	parser->keys = newKeys;
	parser->keys[parser->nKeys++] = key;
	return SUCCESS;
}

/**
 * Reads a key list and stores it inside parser->keys
 *
 * Examples:
 * a b
 * d l, x x
 * home ^A, A
 */
static inline int
read_keys(struct bind_parser *parser)
{
	int r;
	int *newKeys;

	parser->nKeys = 0;
	while(!(r = read_key(parser))) {
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
	if(r != SKIP)
		return r;
	if(!parser->nKeys)
		return SKIP;
	newKeys = realloc(parser->keys, sizeof(*parser->keys) * (parser->nKeys + 1));
	if(!newKeys)
		return OUTOFMEMORY;
	parser->keys = newKeys;
	parser->keys[parser->nKeys++] = 0;
	return SUCCESS;
}

/**
 * Reads a call and stores it inside of parser->calls
 *
 * Examples:
 * >1
 * $x-1
 * `N
 * .home
 * !paste
 * ((
 * ))
 * >w4
 */
static inline int
read_call(struct bind_parser *parser)
{
	// TODO: when this list is complete, add binary search or hashing (same goes for translate_key)
	static const struct {
		const char *word;
		U32 type;
	} namedCommands[] = {
		{ "DELETE", BIND_CALL_DELETESELECTION },
		{ "COPY", BIND_CALL_COPY },
		{ "PASTE", BIND_CALL_PASTE },
		{ "UNDO", BIND_CALL_UNDO },
		{ "REDO", BIND_CALL_REDO },
		{ "COLOR", BIND_CALL_COLORWINDOW },
		{ "CLOSE", BIND_CALL_CLOSEWINDOW },
		{ "NEW", BIND_CALL_NEWWINDOW },
		{ "WRITE", BIND_CALL_WRITEFILE },
		{ "READ", BIND_CALL_READFILE },
		{ "OPEN", BIND_CALL_OPENWINDOW },
		{ "QUIT", BIND_CALL_QUIT },
	};
	struct binding_call *newCalls;

	newCalls = realloc(parser->calls, sizeof(*parser->calls) * (parser->nCalls + 1));
	if(!newCalls)
		return OUTOFMEMORY;
	parser->calls = newCalls;
	parser->calls[parser->nCalls].flags = 0;
again:
	switch(parser->c) {
	case '|':
		while(isblank(parser_getc(parser)));
		parser->calls[parser->nCalls].flags |= FBIND_CALL_OR;
		goto again;
	case '&':
		while(isblank(parser_getc(parser)));
		parser->calls[parser->nCalls].flags |= FBIND_CALL_AND;
		goto again;
	case '(':
		if(parser_getc(parser) != '(') {
			parser_pusherror(parser, ERR_INVALID_COMMAND);
			return SKIP;
		}
		parser_getc(parser);
		if(parser->nLoopStack == ARRLEN(parser->loopStack)) {
			parser_pusherror(parser, ERR_LOOP_TOO_DEEP);
			return SKIP;
		}
		parser->loopStack[parser->nLoopStack++] = parser->nCalls;
		parser->calls[parser->nCalls].type = BIND_CALL_STARTLOOP;
		break;
	case ')':
		if(parser_getc(parser) != ')') {
			parser_pusherror(parser, ERR_INVALID_COMMAND);
			return SKIP;
		}
		parser_getc(parser);
		if(!parser->nLoopStack) {
			parser_pusherror(parser, ERR_END_OF_LOOP_WITHOUT_START);
			return SKIP;
		}
		parser->calls[parser->nCalls].param = parser->nCalls - parser->loopStack[--parser->nLoopStack];
		if(!parser->calls[parser->nCalls].param) {
			parser_pusherror(parser, ERR_EMPTY_LOOP);
			return SKIP;
		}
		parser->calls[parser->nCalls].type = BIND_CALL_ENDLOOP;
		break;
	case '#':
		parser_getc(parser);
		parser->calls[parser->nCalls].type = BIND_CALL_REGISTER;
		if(read_number(parser) != SUCCESS) {
			if(read_word(parser) != SUCCESS) {
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
			if(read_word(parser) == SUCCESS) {
				if(!strcmp(parser->word, "N"))
					parser->calls[parser->nCalls].flags = FBIND_CALL_USENUMBER;
				else
					parser_pusherror(parser, ERR_INVALID_PARAM);
				break;
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
		if(read_number(parser) == SKIP) {
			if(read_word(parser) != SUCCESS) {
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
			if(read_word(parser) == SUCCESS) {
				if(!strcmp(parser->word, "N"))
					parser->calls[parser->nCalls].flags = FBIND_CALL_USENUMBER;
				else
					parser_pusherror(parser, ERR_INVALID_PARAM);
				break;
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
		if(read_string(parser) < 0)
			return OUTOFMEMORY;
		const void *const p = const_alloc(parser->str, parser->nStr);
		if(!p)
			return OUTOFMEMORY;
		parser->calls[parser->nCalls].param = const_getid(p);
		break;
	}
	case '!':
	case ':': {
		const bool setMode = parser->c == ':';
		parser_getc(parser);
		if(read_word(parser) != SUCCESS) {
			parser_pusherror(parser, ERR_EXPECTED_PARAM);
			return MUSTSKIP;
		}
		if(setMode) {
			struct param_request req;

			parser->calls[parser->nCalls].type = BIND_CALL_SETMODE;
			req.mode = parser->nModes - 1;
			req.major = parser->modes[parser->nModes - 1].nBindings;
			req.minor = parser->nCalls;
			strcpy(req.target, parser->word);
			if(parser_addparamrequest(parser, &req) < 0)
				return OUTOFMEMORY;
		} else {
			U32 i;
			for(i = 0; i < ARRLEN(namedCommands); i++)
				if(!strcasecmp(namedCommands[i].word, parser->word)) {
					parser->calls[parser->nCalls].type = namedCommands[i].type;
					break;
				}
			if(i == ARRLEN(namedCommands)) {
				parser_pusherror(parser, ERR_INVALID_COMMAND);
				return SKIP;
			}
		}
		break;
	}
	default:
		parser_pusherror(parser, ERR_INVALID_COMMAND);
		return MUSTSKIP;
	}
	parser->nCalls++;
	return SUCCESS;
}

static inline int
read_calls(struct bind_parser *parser)
{
	int r;

	parser->nCalls = 0;
	while(1) {
		while(isblank(parser->c))
			parser_getc(parser);
		if(parser->c == EOF || parser->c == '\n')
			break;
		if((r = read_call(parser)) < 0 || r == MUSTSKIP)
			return r;
		if(r == SKIP)
			break;
	}
	return parser->nCalls ? SUCCESS : SKIP;
}

int
bind_parse(FILE *fp)
{
	int errCode = 0;
	int r = 0;
	struct bind_parser parser;

	memset(&parser, 0, sizeof(parser));
	parser.fp = fp;
	parser.modes = malloc(sizeof(*parser.modes) * 3);
	parser.str = malloc(4);
	if(!parser.modes || !parser.str) {
		free(parser.modes);
		free(parser.str);
		return OUTOFMEMORY;
	}
	while(1) {
		if(r == MUSTSKIP)
			while(parser.c != '\n' && parser.c != EOF)
				parser_getc(&parser);
		if(parser_skip(&parser) != SUCCESS)
			break;
		r = read_mode(&parser);
		if(r < 0) {
			errCode = r;
			goto err;
		}
		if(r != SKIP)
			continue;
		if(!parser.nModes) {
			parser_pusherror(&parser, ERR_BIND_OUTSIDEOF_MODE);
			r = MUSTSKIP;
			continue;
		}
		r = read_keys(&parser);
		if(r < 0) {
			errCode = r;
			goto err;
		}
		if(r == MUSTSKIP) {
			parser_pusherror(&parser, ERR_INVALID_KEY);
			continue;
		}
		r = read_calls(&parser);
		if(r < 0) {
			errCode = r;
			goto err;
		}
		if(r) {
			r = MUSTSKIP;
			continue;
		}
		struct binding_mode *const mode = parser.modes + parser.nModes - 1;
		struct binding *const newBindings =
			realloc(mode->bindings, sizeof(*mode->bindings) * (mode->nBindings + 1));
		if(!newBindings) {
			errCode = OUTOFMEMORY;
			goto err;
		}
		mode->bindings = newBindings;
		struct binding *const bind = mode->bindings + mode->nBindings++;
		bind->keys = alloca(sizeof(*parser.keys) * parser.nKeys);
		bind->calls = alloca(sizeof(*parser.calls) * parser.nCalls);
		if(!bind->keys || !bind->calls) {
			errCode = OUTOFMEMORY;
			goto err;
		}
		memcpy(bind->keys, parser.keys, sizeof(*parser.keys) * parser.nKeys);
		memcpy(bind->calls, parser.calls, sizeof(*parser.calls) * parser.nCalls);
		bind->nKeys = parser.nKeys;
		bind->nCalls = parser.nCalls;
	}
	// Cleanup the parser
	// 1. Fulfill accumulated requests
	// 2. Add parsed modes to global mode list
	// 3. Free resources
	if(!parser.nErrors) {
		U32 pos[parser.nModes];
		mode_getmergepos(parser.modes, parser.nModes, pos);
		for(U32 i = 0, m; i < parser.nParamRequests; i++) {
			for(m = 0; m < parser.nModes; m++)
				if(!strcmp(parser.modes[m].name, parser.paramRequests[i].target))
					break;
			if(m == parser.nModes)
				return -17;
			parser.modes[parser.paramRequests[i].mode].
			bindings[parser.paramRequests[i].major].
			calls   [parser.paramRequests[i].minor].
			param = pos[m];
		}
		for(U32 i = 0, m; i < parser.nAppendRequests; i++) {
			for(m = 0; m < parser.nModes; m++)
				if(!strcmp(parser.modes[m].name, parser.appendRequests[i].donor))
					break;
			if(m == parser.nModes)
				return -18;
			const struct binding_mode *const donor = parser.modes + m;
			struct binding_mode *const receiver = parser.modes + parser.appendRequests[i].receiver;
			receiver->bindings = realloc(receiver->bindings,
					sizeof(*receiver->bindings) * (receiver->nBindings + donor->nBindings));
			if(!receiver->bindings)
				return -19;
			memcpy(receiver->bindings + receiver->nBindings,
					donor->bindings,
					sizeof(*donor->bindings) * donor->nBindings);
			receiver->nBindings += donor->nBindings;
		}
		mode_merge(parser.modes, parser.nModes);
	}

	// TODO: this is debug code, remove it later
	fseek(parser.fp, 0, SEEK_SET);
	int line = 1, col = 1;
	for(U32 j = 0; j < parser.nErrStack; j++) {
		while(ftell(parser.fp) != parser.errStack[j].pos) {
			int c = fgetc(parser.fp);
			if(c == EOF)
				break;
			if(c == '\n') {
				line++;
				col = 1;
			} else
				col++;
		}
		printf("error(%d:%d): %s\n", line, col, error_messages[parser.errStack[j].err]);
	}
	errCode = parser.nErrors ? 1 : 0;
err:
	free(parser.keys);
	free(parser.calls);
	for(U32 i = 0; i < parser.nModes; i++)
		free(parser.modes[i].bindings);
	free(parser.modes);
	free(parser.paramRequests);
	free(parser.appendRequests);
	return errCode;
}
