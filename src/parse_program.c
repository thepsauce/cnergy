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
		tok->type = parser->c;
		if(parser_getc(parser) == 'w') {
			parser_getc(parser);
			tok->value[nValue++] = 'w';
		}
		if(parser->c == 'x') {
			parser_getc(parser);
			tok->value[nValue++] = 'x';
		}
		tok->value[nValue] = 0;
		break;
	default:
		return 1;
	}
	return SUCCESS;
}

int
parser_writeprogram(struct parser *parser, void *program, size_t szProgram)
{
	void *const newProgram = realloc(parser->program, parser->szProgram + szProgram);
	if(!newProgram)
		return -1;
	parser->program = newProgram;
	memcpy(parser->program + parser->szProgram, program, szProgram);
	printf("%zu += %zu; %p\n", parser->szProgram, szProgram, newProgram);
	parser->szProgram += szProgram;
	return 0;
}

int
parser_addlabelrequest(struct parser *parser, struct parser_label_request *lrq)
{
	struct parser_label_request *newRequests;

	newRequests = realloc(parser->labelRequests, sizeof(*parser->labelRequests) * (parser->nLabelRequests + 1));
	if(!newRequests)
		return parser_pusherror(parser, ERR_OUTOFMEMORY);
	parser->labelRequests = newRequests;
	parser->labelRequests[parser->nLabelRequests++] = *lrq;
	return SUCCESS;
}

int
parser_addlabel(struct parser *parser, struct parser_label *lbl)
{
	struct parser_label *newLabels;

	newLabels = realloc(parser->labels, sizeof(*parser->labels) * (parser->nLabels + 1));
	if(!newLabels)
		return parser_pusherror(parser, ERR_OUTOFMEMORY);
	parser->labels = newLabels;
	parser->labels[parser->nLabels++] = *lbl;
	return SUCCESS;
}

size_t
parser_getlabeladdress(struct parser *parser, const char *name)
{
	struct parser_label_request lrq;

	for(unsigned i = 0; i < parser->nLabels; i++)
		if(!strcmp(parser->labels[i].name, name))
			return parser->labels[i].address;
	strcpy(lrq.name, name);
	lrq.address = parser->szProgram;
	parser_addlabelrequest(parser, &lrq);
	return 0;
}

static int
parser_getinstruction(struct parser *parser)
{
	static const struct {
		const char *name;
		int (*getter)(struct parser *parser);
	} instructions[] = {
		{ "LDA", parser_getld },
		{ "LDB", parser_getld },
		{ "LDC", parser_getld },
		{ "LDD", parser_getld },
		{ "PSHA", parser_getpsh },
		{ "PSHB", parser_getpsh },
		{ "PSHC", parser_getpsh },
		{ "PSHD", parser_getpsh },
		{ "POPA", parser_getpop },
		{ "POPB", parser_getpop },
		{ "POPC", parser_getpop },
		{ "POPD", parser_getpop },
		{ "INCA", parser_getinc },
		{ "INCB", parser_getinc },
		{ "INCC", parser_getinc },
		{ "INCD", parser_getinc },
		{ "DECA", parser_getdec },
		{ "DECB", parser_getdec },
		{ "DECC", parser_getdec },
		{ "DECD", parser_getdec },
		{ "JMP", parser_getjmp },
		{ "JZ", parser_getjmp },
		{ "JNZ", parser_getjmp },
		{ "CALL", parser_getcall },
		{ "EXIT", parser_getexit },
	};
	static const struct {
		char ch;
		int (*getter)(struct parser *parser);
	} shorthands[] = {
		{ '&', parser_getendjmp },
		{ '<', parser_getmotion },
		{ '>', parser_getmotion },
		{ '`', parser_getmotion },
		{ '.', parser_getmotion },
		{ '$', parser_getmotion },
		{ '+', parser_getinsert },
		{ '?', parser_getassert },
		{ ':', parser_getsetmode },
	};
	struct parser_token toks[2];
	struct parser_label lbl;

	parser_peektokens(parser, toks, ARRLEN(toks));
	switch(toks[0].type) {
	case 'w': {
		unsigned i;

		if(toks[1].type == ':') {
			strcpy(lbl.name, toks[0].value);
			lbl.address = parser->szProgram;
			parser_addlabel(parser, &lbl);
			parser_consumetokens(parser, 2);
			break;
		}
		for(i = 0; i < ARRLEN(instructions); i++)
			if(!strcasecmp(instructions[i].name, toks[0].value)) {
				instructions[i].getter(parser);
				break;
			}
		if(i == ARRLEN(instructions)) {
			parser_consumetoken(parser);
			return parser_pusherror(parser, ERRINSTR_INVALIDNAME);
		}
		break;
	}
	case ';':
		parser_consumetoken(parser);
		break;
	default: {
		unsigned i;

		for(i = 0; i < ARRLEN(shorthands); i++)
			if(shorthands[i].ch == toks[0].type ) {
				shorthands[i].getter(parser);
				break;
			}
		if(i == ARRLEN(shorthands)) {
			struct filecache *fc;
			fc = fc_lock(toks[0].file);
			printf("unsupported(kill me): %d (%c) %s %s %ld\n", toks[0].type, toks[0].type, toks[0].value, fc->name, toks[0].pos);
			fc_unlock(fc);
			sleep(1);
			return FAIL;
		}
	}
	}
	return SUCCESS;
}

int
parser_getprogram(struct parser *parser)
{
	bool multi = false;
	struct parser_token tok;
	uint8_t exitInstr[sizeof(instr_t) + sizeof(int)];

	parser->szProgram = 0;
	parser->nLabels = 0;
	parser->nLabelRequests = 0;

	parser_consumeblank(parser);
	parser_setcontext(parser, parser_getprogramtoken);
	parser_peektoken(parser, &tok);

	if(tok.type == '{') {
		parser_consumetoken(parser);
		multi = true;
	}
	while(parser_consumeblank(parser), parser_peektoken(parser, &tok) > 0) {
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
	*(int*) (exitInstr + sizeof(instr_t)) = 0;
	parser_writeprogram(parser, exitInstr, sizeof(exitInstr));
	return SUCCESS;
}
