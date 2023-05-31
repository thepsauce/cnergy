#include "cnergy.h"

static const char *error_messages[] = {
	[ERR_INVALID_SYNTAX] = "invalid syntax",
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
	[ERR_WINDOW_DEL] = "cannot use x with w",
	[ERR_LOOP_TOO_DEEP] = "loop is too deep",
	[ERR_END_OF_LOOP_WITHOUT_START] = "reached an end of a loop but there is no start",
	[ERR_EMPTY_LOOP] = "loop cannot be empty",
	[ERR_XOR_LOOP] = "xor is not allowed before the start or end of a loop",
	[ERR_MODE_NOT_EXIST] = "mode does not exist",
	[ERR_EXPECTED_MODE_NAME] = "mode does not exist",
	[ERR_EXPECTED_WORD_AT] = "expected word after @",
	[ERR_INVALID_WINDOW_TYPE] = "invalid window type after @",
};

const char *
parser_strerror(U32 err)
{
	return error_messages[err];
}

int (*constructs[][16])(struct parser *parser) = {
	{ parser_getword, parser_checkbindword, parser_skipspace, parser_getwindowtype, parser_skipspace,
		parser_getbindmodeprefix, parser_getbindmodesuffix, parser_skipspace,
		parser_getbindmodeextensions, parser_addbindmode },
	{ parser_getword, parser_checkincludeword, parser_skipspace,
		parser_getwindowtype, parser_skipspace, parser_getstring, parser_addinclude },
	{ parser_getkeylist, parser_getcalllist, parser_addbind },

};

int
parser_cleanup(struct parser *parser)
{
	free(parser->str);
	for(U32 i = 0; i < WINDOW_MAX; i++) {
		for(struct binding_mode *m = parser->firstModes[i]; m; m = m->next) {
			free(m->bindings);
		}
		free(parser->firstModes[i]);
	}
	free(parser->appendRequests);
	return 0;
}

int
parser_open(struct parser *parser, const char *path, U32 windowType)
{
	FILE *fp;

	if(parser->nFiles == ARRLEN(parser->files))
		return -1;
	fp = fopen(path, "r");
	if(!fp)
		return -1;
	parser->files[parser->nFiles].fp = fp;
	parser->files[parser->nFiles].windowType = windowType;
	parser->files[parser->nFiles].path = const_alloc(path, strlen(path) + 1); // don't care if const_alloc failes
	parser->nFiles++;
	return 0;
}

int
parser_getc(struct parser *parser)
{
	if(parser->c == EOF) {
		parser->nFiles--;
		fclose(parser->files[parser->nFiles].fp);
		if(!parser->nFiles)
			return EOF;
	}
	parser->prev_c = parser->c;
	return parser->c = fgetc(parser->files[parser->nFiles - 1].fp);
}

void
parser_ungetc(struct parser *parser)
{
	ungetc(parser->c, parser->files[parser->nFiles - 1].fp);
	parser->c = parser->prev_c;
}

void
parser_pusherror(struct parser *parser, U32 err)
{
	if(parser->nErrStack == ARRLEN(parser->errStack)) {
		memmove(parser->errStack, parser->errStack + 1, sizeof(parser->errStack) - sizeof(*parser->errStack));
		parser->nErrStack--;
	}
	parser->errStack[parser->nErrStack].err = err;
	parser->errStack[parser->nErrStack].file = parser->files[parser->nFiles - 1].path;
	parser->errStack[parser->nErrStack].pos = ftell(parser->files[parser->nFiles - 1].fp) - 1;
	parser->nErrStack++;
	if(err > WARN_MAX)
		parser->nErrors++;
	else
		parser->nWarnings++;
}

int
parser_addappendrequest(struct parser *parser, struct append_request *request)
{
	struct append_request *newRequests;

	newRequests = realloc(parser->appendRequests, sizeof(*parser->appendRequests) * (parser->nAppendRequests + 1));
	if(!newRequests)
		return OUTOFMEMORY;
	parser->appendRequests = newRequests;
	parser->appendRequests[parser->nAppendRequests++] = *request;
	return SUCCESS;
}

int
parser_getword(struct parser *parser)
{
	int c;
	U32 nWord = 0;

	c = parser->c;
	if(!isalpha(c) && c != '_')
		return FAIL;
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

int
parser_getnumber(struct parser *parser)
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
		return FAIL;
	if(!isdigit(c))
		num = 1;
	else
		do {
			num = SAFE_MUL(num, 10);
			num = SAFE_ADD(num, c - '0');
		} while(isdigit(c = parser_getc(parser)));
	parser->num = SAFE_MUL(sign, num);
	parser->c = c;
	return SUCCESS;
}

int
parser_getstring(struct parser *parser)
{
	int c;
	char *str;
	U32 nStr = 0;

	c = parser->c;
	if(!isprint(c))
		return FAIL;
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

int
parser_getwindowtype(struct parser *parser)
{
	static const char *windowTypes[] = {
		[WINDOW_ALL] = "all",
		[WINDOW_EDIT] = "edit",
		[WINDOW_BUFFERVIEWER] = "bufferviewer",
		[WINDOW_FILEVIEWER] = "fileviewer",
	};
	if(parser->c != '@')
		return SUCCESS;
	parser_getc(parser);
	if(parser_getword(parser) != SUCCESS) {
		parser_pusherror(parser, ERR_EXPECTED_WORD_AT);
		return FAIL;
	}
	for(U32 i = 0; i < ARRLEN(windowTypes); i++)
		if(!strcmp(windowTypes[i], parser->word)) {
			parser->windowType = i;
			return SUCCESS;
		}
	parser_pusherror(parser, ERR_INVALID_WINDOW_TYPE);
	return FAIL;
}

int
parser_skipspace(struct parser *parser)
{
	while(isblank(parser->c))
		parser_getc(parser);
	return SUCCESS;
}

int
parser_next(struct parser *parser)
{
	long pos;
	int c, prev_c;

	do {
		if(!parser->nFiles)
			return 1;
		c = parser_getc(parser);
		if(c == '#') {
			c = parser_getc(parser);
			if(c != '#') {
				parser_ungetc(parser);
				break;
			}
			while((c = parser_getc(parser)) != '\n' && c != EOF);
		}
	} while(isspace(c) || c == EOF);
	pos = ftell(parser->files[parser->nFiles - 1].fp);
	c = parser->c;
	prev_c = parser->prev_c;
	for(U32 i = 0; i < ARRLEN(constructs); i++) {
		int r;
		bool com = false;
		int (**con)(struct parser *parser);

		con = constructs[i];
		do {
			r = (**con)(parser);
			if(r == COMMIT) {
				r = SUCCESS;
				com = true;
			}
			con++;
		} while(r == SUCCESS);
		if(r == FINISH)
			return 0;
		if(r < 0)
			return -1;
		if(com) {
			while(parser->c != '\n' && parser->c != EOF)
				parser_getc(parser);
			return 0;
		}
		fseek(parser->files[parser->nFiles - 1].fp, pos, SEEK_SET);
		parser->prev_c = prev_c;
		parser->c = c;
	}
	parser_pusherror(parser, ERR_INVALID_SYNTAX);
	while(parser->c != '\n' && parser->c != EOF)
		parser_getc(parser);
	return 0;
}
