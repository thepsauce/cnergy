#include "cnergy.h"

int
parser_getprogramtoken(struct parser *parser, struct parser_token *tok)
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
	case '+': case '-':
		tok->value[nValue++] = parser->c;
		if(!isdigit(parser_getc(parser))) {
			tok->value[nValue++] = '1';
			tok->value[nValue] = 0;
			tok->type = 'd';
			break;
		}
	/* fall through */
	case '0' ... '9':
		tok->value[nValue++] = parser->c;
		while(isdigit(parser_getc(parser)))
			tok->value[nValue++] = parser->c;
		tok->value[nValue] = 0;
		tok->type = 'd';
		break;
	case '$':
	case '<':
	case '>':
	case '.':
	case '`':
		if(parser_getc(parser) == 'w') {
			parser_getc(parser);
			tok->value[nValue++] = 'w';
		}
		if(parser_getc(parser) == 'x') {
			parser_getc(parser);
			tok->value[nValue++] = 'x';
		}
		tok->value[nValue] = 0;
		tok->type = parser->c;
		break;
	default:
		return 1;
	}
	return SUCCESS;
}

static int
parser_writeprogram(struct parser *parser, void *program, size_t szProgram)
{
	void *const newProgram = realloc(parser->program, parser->szProgram + szProgram);
	if(!newProgram)
		return -1;
	parser->program = newProgram;
	memcpy(parser->program + parser->szProgram, program, szProgram);
	parser->szProgram += szProgram;
	return 0;
}

static int
parser_writedata(struct parser *parser, void *data, size_t szData)
{
	void *const newData = realloc(parser->data, parser->szData + szData);
	if(!newData)
		return -1;
	parser->data = newData;
	memcpy(parser->data + parser->szData, data, szData);
	parser->szData += szData;
	return 0;
}

static int
parser_getinstruction(struct parser *parser)
{
	struct parser_token toks[4];
	unsigned i;
	uint8_t prog[128];
	void *p;

	p = prog;
	// TODO: implement all instructions (1000 lines of code to follow ..... :( )
	parser_peektokens(parser, toks, ARRLEN(toks));
	switch(toks[0].type) {
	case 'w':
		parser_consumetoken(parser);
		break;
	case '$':
	case '`':
	case '.':
	case '<':
	case '>': {
		bool del = false;
		bool window = false;
		char *w;
		ptrdiff_t mul;
		const bool horz = toks[0].type == '<' || toks[0].type == '>';
		const bool vert = toks[0].type == '.' || toks[0].type == '`';
		mul = toks[0].type == '<' || toks[0].type == '`' ? -1 : 1;
		w = toks[0].value;
		if(*w == 'w') {
			w++;
			window = true;
		}
		if(*w == 'x') {
			w++;
			del = true;
		}
		parser_consumetoken(parser);
		i = 1;
		if(toks[i].type == 'd') {
			ptrdiff_t i;

			(void) parseint(toks[i].value, &i);
			mul = SAFE_MUL(mul, i);
			i++;
		} else if(toks[i].type == 'w') {
			if(!strcmp(toks[i].value, "home")) {
				mul = SAFE_MUL(mul, PTRDIFF_MIN);
				i++;
			} else if(!strcmp(toks[i].value, "end")) {
				mul = SAFE_MUL(mul, PTRDIFF_MAX);
				i++;
			}
		}
		if(toks[i].type == 'w') {
			if(!strcmp(toks[i].value, "A")) {
				/* do nothing */
			} else if(!strcmp(toks[i].value, "B")) {
				*(instr_t*) p = INSTR_LDAB;
				p += sizeof(instr_t);
			} else if(!strcmp(toks[i].value, "C")) {
				*(instr_t*) p = INSTR_LDAC;
				p += sizeof(instr_t);
			} else if(!strcmp(toks[i].value, "D")) {
				*(instr_t*) p = INSTR_LDAD;
				p += sizeof(instr_t);
			} else {
				parser_pusherror(parser, ERRINSTR_INVALIDWORD);
				return FAIL;
			}
		}
		if(mul != 1) {
			*(instr_t*) p = INSTR_MULA;
			p += sizeof(instr_t);
			*(ptrdiff_t*) p = mul;
			p += sizeof(ptrdiff_t);
		}
		*(instr_t*) p = INSTR_CALL;
		p += sizeof(instr_t);
		if(window && del)
			return parser_pusherror(parser, ERRINSTR_WINDOWDEL);
		if(del)
			*(call_t*) p = vert ? CALL_MOVEWINDOW_BELOW : CALL_MOVEWINDOW_RIGHT;
		else if(window)
			*(call_t*) p = vert ? CALL_DELETELINE : CALL_DELETE;
		else
			*(call_t*) p = vert ? CALL_MOVEVERT : horz ? CALL_MOVEHORZ : CALL_MOVECURSOR;
		p += sizeof(call_t);
		parser_writeprogram(parser, prog, p - (void*) prog);
		break;
	}
	case '+': case '?': {
		parser_consumetoken(parser);
		if(toks[1].type == '\"') {
			parser_consumetoken(parser);
			char *const str = mode_allocstring(toks[1].value, strlen(toks[1].value));
			if(!str)
				return parser_pusherror(parser, ERR_OUTOFMEMORY);
			*(instr_t*) p = INSTR_LDA;
			p += sizeof(instr_t);
			*(size_t*) p = (size_t) str;
			p += sizeof(size_t);
			*(call_t*) p = toks[0].type == '+' ? CALL_INSERT : CALL_ASSERT;
			p += sizeof(call_t);
		} else {
			parser_consumetoken(parser);
			// TODO:
		}
		break;
	}
	case ':': {
		parser_consumetoken(parser);
		if(toks[1].type != 'w')
			return parser_pusherror(parser, ERRINSTR_COLONWORD);
		parser_consumetoken(parser);
		char *const str = mode_allocstring(toks[1].value, strlen(toks[1].value));
		if(!str)
			return parser_pusherror(parser, ERR_OUTOFMEMORY);
		*(instr_t*) p = INSTR_LDA;
		p += sizeof(instr_t);
		*(size_t*) p = (size_t) str;
		p += sizeof(size_t);
		*(call_t*) p = CALL_SETMODE;
		p += sizeof(call_t);
		break;
	}
	default:
		return FAIL;
	}
	return SUCCESS;
}

int
parser_getprogram(struct parser *parser)
{
	bool multi = false;
	struct parser_token tok;
	uint8_t exitInstr[sizeof(instr_t) + sizeof(ptrdiff_t)];

	parser->szProgram = 0;
	parser->szData = 0;
	parser->nLabels = 0;
	parser->nLabelRequests = 0;

	parser_consumeblank(parser);
	parser_setcontext(parser, parser_getprogramtoken);
	parser_peektoken(parser, &tok);

	if(tok.type == '{') {
		parser_consumetoken(parser);
		parser_peektoken(parser, &tok);
		multi = true;
	}
	while(parser_peektoken(parser, &tok) > 0) {
		if(tok.type == '}') {
			if(!multi) {
				const int r = parser_pusherror(parser, ERRPROG_MISSING_OPENING);
				parser_consumetoken(parser);
				return r;
			}
			parser_consumetoken(parser);
			break;
		}
		if(tok.type == '\n') {
			parser_consumetoken(parser);
			if(!multi)
				break;
			continue;
		}
		parser_getinstruction(parser);
	}
	/* handle label requests */
	for(size_t i = 0; i < parser->nLabelRequests; i++)
		for(size_t j = 0; j < parser->nLabels; j++)
			if(parser->labelRequests[i].name[0] == '\0')
				*(ptrdiff_t*) (parser->program + parser->labelRequests[i].address) =
					parser->szProgram - parser->labelRequests[i].address;
			else if(!strcmp(parser->labelRequests[i].name, parser->labels[i].name))
					*(ptrdiff_t*) (parser->program + parser->labelRequests[i].address) =
						parser->labels[i].address - parser->labelRequests[i].address;
			else
				parser_pusherror(parser, ERRPROG_MISSING_LABEL);
	/* add exit to the end of the program */
	*(instr_t*) exitInstr = INSTR_EXIT;
	*(ptrdiff_t*) (exitInstr + sizeof(instr_t)) = 0;
	parser_writeprogram(parser, exitInstr, sizeof(exitInstr));
	return SUCCESS;
}
