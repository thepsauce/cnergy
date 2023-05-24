#include "cnergy.h"

static inline int
parse_word(FILE *fp, int c, char *word, U32 maxWord)
{
	U32 nWord = 0;

	while(isalnum(c) || c == '_') {
		if(nWord == maxWord - 1) {
			word[0] = 0;
			return -1;
		}
		word[nWord++] = c;
		c = fgetc(fp);
	}
	word[nWord] = 0;
	return c;
}

static inline int
parse_number(FILE *fp, int c, int *pNum)
{
	int num = 0;
	int sign = 1;

	if(c == '+') {
		sign = 1;
		c = fgetc(fp);
	} else if(c == '-') {
		sign = -1;
		c = fgetc(fp);
	} else if(!isdigit(c))
		return -1;
	if(!isdigit(c))
		num = 1;
	else
		do {
			num *= 10;
			num += c - '0';
		} while(isdigit(c = fgetc(fp)));
	*pNum = sign * num;
	return c;
}

static inline int
parse_string(FILE *fp, int c, char **pStr, U32 *pnStr)
{
	char *str = NULL;
	U32 nStr = 0;
	const bool quoted = c == '\"';

	if(quoted)
		c = fgetc(fp);
	while(1) {
		if(c == EOF ||
			(quoted && (c == '\"' || c == '\n')) ||
			(!quoted && isspace(c)))
			break;
		str = realloc(str, nStr + 1);
		if(!str)
			return -1;
		if(c == '\\') {
			c = fgetc(fp);
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
		c = fgetc(fp);
	}
	if(quoted && c == '\"')
		c = fgetc(fp);
	str = realloc(str, nStr + 1);
	if(!str)
		return -1;
	str[nStr++] = 0;
	*pStr = str;
	*pnStr = nStr;
	return c;
}

static inline int
translate_key(const char *word)
{
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
	if(!word[1])
		return word[0];
	if(word[0] == '^')
		return word[1] - 'A' + 1;
	for(U32 i = 0; i < ARRLEN(keyTranslations); i++)
		if(!strcasecmp(keyTranslations[i].word, word))
			return keyTranslations[i].key;
	return -1;
}

static inline int
translate_command(const char *word, U16 *pType)
{
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
	for(U32 i = 0; i < ARRLEN(namedCommands); i++)
		if(!strcasecmp(namedCommands[i].word, word)) {
			*pType = namedCommands[i].type;
			return 0;
		}
	return -1;
}

// TODO: The exit codes of this function are pretty random, needs fixing
int
bind_parse(FILE *fp)
{
	char word[64];
	int num;
	char *str;
	U32 nStr;
	char *strings = NULL;
	size_t szStrings = 0;
	int c;
	struct binding_mode mode;
	struct binding_mode *modes;
	U32 nModes = 0;
	size_t szModeBindings;
	// the requests are necessary to allow usage before declaration
	// append requests are added when a mode should be extended by another
	struct append_request {
		U32 receiver;
		char donor[64];
	} *appendRequests = NULL;
	U32 nAppendRequests = 0;
	int *keys = NULL;
	U32 nKeys = 0;
	// param requests are added when a mode is used that is not yet defined
	struct param_request {
		U32 mode;
		U32 major, minor;
		char target[64];
	} *paramRequests = NULL;
	U32 nParamRequests = 0;
	struct binding_call *calls = NULL;
	U32 nCalls = 0;
	U32 *pos;

	modes = malloc(sizeof(*modes) * 3);
	if(!modes)
		return -1;
	while(1) {
		c = fgetc(fp);
		if(c == '#') {
			c = fgetc(fp);
			if(c != '#')
				ungetc(c, fp);
			else
				while((c = fgetc(fp)) != '\n' && c != EOF);
		}
		if(c == EOF)
			break;
		if(isspace(c))
			continue;
		memset(&mode, 0, sizeof(mode));
		if(c == '.' || c == '$') {
			mode.flags |= c == '.' ? FBIND_MODE_SUPPLEMENTARY : FBIND_MODE_TYPE;
			word[0] = c;
			c = fgetc(fp);
			if(isblank(c) || c == ',') {
				word[1] = 0;
				goto parse_key;
			}
		}
		c = parse_word(fp, c, word, sizeof(word));
		if(word[0]) {
			if(c == '*') {
				mode.flags |= FBIND_MODE_SELECTION;
				c = fgetc(fp);
			}
			if(c == ':') { // now we 100% know it's a mode change 
				strcpy(mode.name, word);
				while(isblank(c = fgetc(fp)));
				while(1) {
					c = parse_word(fp, c, word, sizeof(word));
					if(!word[0])
						break;
					appendRequests = realloc(appendRequests, sizeof(*appendRequests) * (nAppendRequests + 1));
					if(!appendRequests)
						return -88;
					appendRequests[nAppendRequests].receiver = nModes;
					strcpy(appendRequests[nAppendRequests].donor, word);
					nAppendRequests++;
					while(isblank(c))
						c = fgetc(fp);
					if(c != ',')
						break;
					while(isblank(c = fgetc(fp)));
				}
				if(c != '\n' && c != EOF)
					return -2;
				modes = realloc(modes, sizeof(*modes) * (nModes + 1));
				if(!modes)
					return -89;
				modes[nModes++] = mode;
				szModeBindings = 0;
				continue;
			}
		}
		if(mode.flags || !nModes)
			return -3;
	parse_key:
		nKeys = 0;
		while(1) {
			if(!word[0]) {
				word[0] = c;
				word[1] = 0;
				if(c == '^') {
					c = fgetc(fp);
					if(c != '^') {
						if((c < 'A' || c > 'Z') && c != '^')
							return -187;
						word[1] = c;
						c = fgetc(fp);
					}
				} else {
					c = fgetc(fp);
					if(!isblank(c) && c != ',') {
						ungetc(c, fp);
						c = word[0];
						break;
					}
				}
			}
			const int key = translate_key(word);
			if(key < 0)
				return -5;
			keys = realloc(keys, sizeof(*keys) * (nKeys + 1));
			if(!keys)
				return -6;
			keys[nKeys++] = key;
			if(c == ',') {
				keys = realloc(keys, sizeof(*keys) * (nKeys + 1));
				if(!keys)
					return -7;
				keys[nKeys++] = 0;
				c = fgetc(fp);
			}
			while(isblank(c))
				c = fgetc(fp);
			c = parse_word(fp, c, word, sizeof(word));
		}	
		if(!nKeys || !keys[nKeys - 1])
			return -21;
		keys = realloc(keys, sizeof(*keys) * (nKeys + 1));
		if(!keys)
			return -22;
		keys[nKeys++] = 0;
		// parse commands
		nCalls = 0;
		while(1) {
			while(isblank(c))
				c = fgetc(fp);
			if(c == EOF || c == '\n')
				break;
			if(c == '#') {
				ungetc('#', fp);
				break;
			}
			calls = realloc(calls, sizeof(*calls) * (nCalls + 1));
			if(!calls)
				return -9;
			calls[nCalls].flags = 0;
			switch(c) {
			case '&':
				if(!nCalls)
					return -23;
				calls[nCalls - 1].flags |= FBIND_CALL_AND;
				c = fgetc(fp);
				continue;
			case '$':
			case '`':
			case '.':
			case '<':
			case '>': {
				bool del = false;
				bool window = false;
				const int sign = c == '<' || c == '`' ? - 1 : 1;
				const bool horz = c == '<' || c == '>';
				const bool vert = c == '.' || c == '`';

				c = fgetc(fp);
				if(c == 'x') {
					del = true;
					c = fgetc(fp);
				}
				if(c == 'w') {
					window = true;
					c = fgetc(fp);
				}
				if(isalpha(c)) {
					c = parse_word(fp, c, word, sizeof(word));
					if(!strcasecmp(word, "home"))
						num = INT_MIN;
					else if(!strcasecmp(word, "end"))
						num = INT_MAX;
					else if(!strcasecmp(word, "N")) {
						num = 1;
						calls[nCalls].flags = FBIND_CALL_USENUMBER;
					} else
						return -122;
				} else {
					c = parse_number(fp, c, &num);
					c = parse_word(fp, c, word, sizeof(word));
					if(!strcmp(word, "N"))
						calls[nCalls].flags = FBIND_CALL_USENUMBER;
				}
				if(window) {
					if(del)
						return -130;
					calls[nCalls].type = vert ? BIND_CALL_MOVEWINDOW_BELOW : BIND_CALL_MOVEWINDOW_RIGHT;
				} else if(del) {
					calls[nCalls].type = vert ? BIND_CALL_DELETELINE : BIND_CALL_DELETE;
				} else {
					calls[nCalls].type = vert ? BIND_CALL_MOVEVERT : horz ? BIND_CALL_MOVEHORZ : BIND_CALL_MOVECURSOR;
				}
				calls[nCalls].param = sign * num;
				break;
			}
			case '+': {
				c = fgetc(fp);
				if(isspace(c))
					return -123;
				c = parse_string(fp, c, &str, &nStr);
				const void *const p = const_alloc(str, nStr);
				if(!p)
					return -124;
				calls[nCalls].type = BIND_CALL_INSERT;
				calls[nCalls].param = const_getid(p);
				free(str);
				break;
			}
			case '!':
			case ':': {
				const bool setMode = c == ':';
				c = fgetc(fp);
				c = parse_word(fp, c, word, sizeof(word));
				if(!word[0])
					return -10;
				if(setMode) {
					calls[nCalls].type = BIND_CALL_SETMODE;
					calls[nCalls].param = 999;
					paramRequests = realloc(paramRequests, sizeof(*paramRequests) * (nParamRequests + 1));
					if(!paramRequests)
						return -32;
					paramRequests[nParamRequests].mode = nModes - 1;
					paramRequests[nParamRequests].major = modes[nModes - 1].nBindings;
					paramRequests[nParamRequests].minor = nCalls;
					strcpy(paramRequests[nParamRequests].target, word);
					nParamRequests++;
				} else {
					if(translate_command(word, &calls[nCalls].type) < 0)
						return -12;
				}
				break;
			}
			default:
				return -13;
			}
			nCalls++;
		}
		if(!nCalls)
			return -14;
		modes[nModes - 1].bindings = realloc(modes[nModes - 1].bindings, sizeof(*modes[nModes - 1].bindings) * (modes[nModes - 1].nBindings + 1));
		if(!modes[nModes - 1].bindings)
			return -16;
		struct binding *const bind = modes[nModes - 1].bindings + modes[nModes - 1].nBindings++;
		bind->keys = alloca(sizeof(*keys) * nKeys);
		if(!bind->keys)
			return -33;
		bind->calls = alloca(sizeof(*calls) * nCalls);
		if(!bind->calls)
			return -34;
		memcpy(bind->keys, keys, sizeof(*keys) * nKeys);
		memcpy(bind->calls, calls, sizeof(*calls) * nCalls);
		bind->nKeys = nKeys;
		bind->nCalls = nCalls;
	}
	pos = alloca(sizeof(*pos) * nModes);
	mode_getmergepos(modes, nModes, pos);
	for(U32 i = 0, m; i < nParamRequests; i++) {
		for(m = 0; m < nModes; m++)
			if(!strcmp(modes[m].name, paramRequests[i].target))
				break;
		if(m == nModes)
			return -17;
		modes   [paramRequests[i].mode].
		bindings[paramRequests[i].major].
		calls   [paramRequests[i].minor].
		param = pos[m];
	}
	for(U32 i = 0, m; i < nAppendRequests; i++) {
		for(m = 0; m < nModes; m++)
			if(!strcmp(modes[m].name, appendRequests[i].donor))
				break;
		if(m == nModes)
			return -18;
		const struct binding_mode *const donor = modes + m;
		struct binding_mode *const receiver = modes + appendRequests[i].receiver;
		receiver->bindings = realloc(receiver->bindings,
				sizeof(*receiver->bindings) * (receiver->nBindings + donor->nBindings));
		if(!receiver->bindings)
			return -19;
		memcpy(receiver->bindings + receiver->nBindings,
				donor->bindings,
				sizeof(*donor->bindings) * donor->nBindings);
		receiver->nBindings += donor->nBindings;
	}
	mode_merge(modes, nModes);

	free(keys);
	free(calls);
	for(U32 i = 0; i < nModes; i++)
		free(modes[i].bindings);
	free(modes);
	free(paramRequests);
	free(appendRequests);
	return 0;
}
