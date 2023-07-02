#include "cnergy.h"

int
parser_getc(struct parser *parser)
{
	if(!parser->nStreams)
		return EOF;
	if(parser->c == EOF) {
		parser->nStreams--;
		fclose(parser->streams[parser->nStreams].fp);
		if(!parser->nStreams)
			return EOF;
	}
	parser->prev_c = parser->c;
	return parser->c = fgetc(parser->streams[parser->nStreams - 1].fp);
}

int
parser_ungetc(struct parser *parser)
{
	ungetc(parser->c, parser->streams[parser->nStreams - 1].fp);
	return parser->c = parser->prev_c;
}

long
parser_tell(struct parser *parser)
{
	return ftell(parser->streams[parser->nStreams - 1].fp);
}

static int
parser_gettoken(struct parser *parser)
{
	struct parser_token *const tok = parser->tokens + parser->nTokens++;
	while(1) {
		if(!parser->nStreams) {
			parser->nTokens--;
			return 1;
		}
		if(parser->c != EOF)
			break;
		parser_getc(parser);
	}
	tok->pos = parser_tell(parser) - 1;
	tok->file = parser->streams[parser->nStreams - 1].file;
	if((*parser->tokengetter)(parser, tok) == SUCCESS)
		return SUCCESS;
	switch(parser->c) {
	case ' ': case '\t':
		while(isblank(parser_getc(parser)));
		if(parser->c == '\n' || parser->c == '\r') {
	/* fall through */
	case '\n': case '\r':
			tok->type = '\n';
		againcomment:
			while(isspace(parser->c))
				parser_getc(parser);
			if(parser->c == '#') {
	/* fall through */
	case '#':
				tok->type = '\n';
				while(parser->c != '\n' && parser->c != '\r' && parser->c != EOF)
					parser_getc(parser);
				parser_getc(parser);
				goto againcomment;
			}
		} else {
			tok->type = ' ';
		}
		break;
	case '\\':
		tok->type = '\\';
		if((tok->value[0] = parser_getc(parser)) != EOF)
			parser_getc(parser);
		break;
	default:
		tok->type = parser->c;
		parser_getc(parser);
		break;
	}
	return SUCCESS;
}

static int
parser_getglobaltoken(struct parser *parser, struct parser_token *tok)
{
	unsigned nValue = 0;

	switch(parser->c) {
	case 'a' ... 'z':
	case 'A' ... 'Z':
	case '_':
		tok->value[nValue++] = parser->c;
		while(isalnum(parser_getc(parser)) || parser->c == '_')
			tok->value[nValue++] = parser->c;
		tok->value[nValue] = 0;
		tok->type = 'w';
		break;
	case '\"':
		while(parser_getc(parser) != '\"' && parser->c != '\n' && parser->c != '\r') {
			tok->value[nValue++] = parser->c;
			if(parser->c == '\\')
				tok->value[nValue++] = parser_getc(parser);
			if(parser->c == EOF)
				break;
		}
		if(parser->c != EOF)
			parser_getc(parser);
		tok->value[nValue] = 0;
		tok->type = '\"';
		break;
	default:
		return 1;
	}
	return 0;
}

unsigned
parser_peektoken(struct parser *parser, struct parser_token *tok)
{
	return parser_peektokens(parser, tok, 1);
}

unsigned
parser_peektokens(struct parser *parser, struct parser_token *toks, unsigned nToks)
{
	assert(nToks <= ARRLEN(parser->tokens) && "misuse of parser_peektokens (peeking at too many tokens at once)");
	while(nToks > parser->nTokens)
		if(parser_gettoken(parser))
			break;
	parser->nPeekedTokens = MIN(nToks, parser->nTokens);
	memcpy(toks, parser->tokens, sizeof(*toks) * parser->nPeekedTokens);
	memset(toks + parser->nPeekedTokens, 0, sizeof(*toks) * (nToks - parser->nPeekedTokens));
	return parser->nPeekedTokens;
}

void
parser_setcontext(struct parser *parser, int (*tokengetter)(struct parser *parser, struct parser_token *tok))
{
	if(parser->tokengetter == tokengetter)
		return;
	if(parser->nTokens) {
		parser->nTokens = 0;
		parser->nPeekedTokens = 0;
		fseek(parser->streams[parser->nStreams - 1].fp, parser->tokens[0].pos, SEEK_SET);
		parser_getc(parser);
	}
	parser->tokengetter = tokengetter;
}

void
parser_consumespace(struct parser *parser)
{
	if(parser->nTokens) {
		if(isspace(parser->tokens[0].type)) {
			parser->nTokens--;
			parser->nPeekedTokens--;
			memmove(parser->tokens, parser->tokens + 1, sizeof(*parser->tokens) * parser->nTokens);
		}
	} else {
	again:
		while(isspace(parser->c))
			parser_getc(parser);
		/* space also includes comments */
		if(parser->c == '#') {
			while(parser->c != '\n' && parser->c != EOF)
				parser_getc(parser);
			parser_getc(parser);
			goto again;
		}
	}
}

void
parser_consumeblank(struct parser *parser)
{
	if(parser->nTokens) {
		if(parser->tokens[0].type == ' ') {
			parser->nTokens--;
			parser->nPeekedTokens--;
			memmove(parser->tokens, parser->tokens + 1, sizeof(*parser->tokens) * parser->nTokens);
		}
	} else {
		while(isblank(parser->c))
			parser_getc(parser);
	}
}

void
parser_consumetoken(struct parser *parser)
{
	parser_consumetokens(parser, 1);
}

void
parser_consumetokens(struct parser *parser, unsigned nToks)
{
	assert(parser->nPeekedTokens >= nToks && "misuse of parser_consumetokens (not enough peeked)");
	parser->nTokens -= nToks;
	parser->nPeekedTokens -= nToks;
	memmove(parser->tokens, parser->tokens + nToks, sizeof(*parser->tokens) * parser->nTokens);
}

parser_error_t
parser_pusherror(struct parser *parser, parser_error_t err)
{
	if(parser->nErrStack == ARRLEN(parser->errStack)) {
		memmove(parser->errStack, parser->errStack + 1, sizeof(parser->errStack) - sizeof(*parser->errStack));
		parser->nErrStack--;
	}
	parser->errStack[parser->nErrStack].err = err;
	parser->errStack[parser->nErrStack].token = parser->tokens[0];
	parser->nErrStack++;
	if(err > WARN_MAX)
		parser->nErrors++;
	else
		parser->nWarnings++;
	return err;
}

int
parseint(const char *str, ptrdiff_t *pInt)
{
	ptrdiff_t sgn = 1;
	ptrdiff_t i;

	if(*str == '+') {
		str++;
	} else if(*str == '-') {
		sgn = -1;
		str++;
	} else if(!isdigit(*str))
		return -1;
	if(!isdigit(*str)) {
		*pInt = sgn;
		return 0;
	}
	i = 0;
	while(isdigit(*str)) {
		i *= 10;
		i += *str - '0';
		str++;
	}
	*pInt = SAFE_MUL(i, sgn);
	return 0;
}

int
parser_run(struct parser *parser, fileid_t file)
{
	FILE *fp;
	struct parser_token tok;

	fp = fc_open(file, "rb");
	if(!fp)
		return -1;
	parser->streams[0] = (struct parser_file) {
		.fp = fp,
		.file = file,
	};
	parser->nStreams = 1;
	parser->c = fgetc(parser->streams[0].fp);
	while(1) {
		parser_consumespace(parser);
		parser_setcontext(parser, parser_getglobaltoken);
		if(parser_peektoken(parser, &tok) == 0)
			return SUCCESS;
		if(tok.type == 'w') {
			if(!strcmp(tok.value, "bind")) {
				parser_consumetoken(parser);
				parser_getbindmode(parser);
				continue;
			}
			if(!strcmp(tok.value, "include")) {
				parser_consumetoken(parser);
				parser_consumespace(parser);
				if(parser_peektoken(parser, &tok) == 0 ||
						tok.type != '\"')
					return parser_pusherror(parser, ERRINCLUDE_STRING);
				parser_consumetoken(parser);
				if(parser->nStreams == ARRLEN(parser->streams))
					return parser_pusherror(parser, ERRFILE_TOOMANY);
				fp = fc_open(fc_cache(fc_getparent(file), tok.value), "rb");
				if(!fp)
					return parser_pusherror(parser, ERRFILE_IO);
				while(parser_peektoken(parser, &tok) > 0 && tok.type != '\n')
					parser_consumetoken(parser);
				parser_consumetoken(parser);
				parser_peektoken(parser, &tok);
				if(parser->nStreams && parser->nTokens) {
					parser->nTokens = 0;
					parser->nPeekedTokens = 0;
					fseek(parser->streams[parser->nStreams - 1].fp,
							parser->tokens[0].pos, SEEK_SET);
				}
				parser->streams[parser->nStreams++] = (struct parser_file) {
					.fp = fp,
					.file = file,
				};
				parser_getc(parser);
				continue;
			}
		}
		parser_getbind(parser);
	}
}

int
parser_cleanup(struct parser *parser)
{
	for(unsigned i = 0; i < parser->nStreams; i++)
		fclose(parser->streams[i].fp);
	for(unsigned i = 0; i < parser->nModes; i++)
		free(parser->modes[i].bindings);
	free(parser->modes);
	free(parser->labels);
	free(parser->labelRequests);
	free(parser->keys);
	free(parser->program);
	free(parser->appendRequests);
	return 0;
}
