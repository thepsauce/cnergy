#include "cnergy.h"

int
parser_getendjmp(struct parser *parser)
{
	struct parser_token tok;
	struct parser_label_request lrq;
	uint8_t prog[sizeof(instr_t) + sizeof(ptrdiff_t)];
	void *p;

	parser_peektoken(parser, &tok);
	parser_consumetoken(parser);
	lrq.name[0] = 0;
	lrq.address = parser->szProgram;
	parser_addlabelrequest(parser, &lrq);
	p = prog;
	*(instr_t*) p = INSTR_JNZ;
	p += sizeof(instr_t);
	*(ptrdiff_t*) p = 0;
	p += sizeof(ptrdiff_t);
	parser_writeprogram(parser, prog, sizeof(prog));
	return SUCCESS;
}

int
parser_getmotion(struct parser *parser)
{
	struct parser_token toks[4];
	struct parser_label_request lrq;
	uint8_t prog[64];
	void *p;
	unsigned iTok;
	bool del = false;
	bool wander = false;
	char *w;
	ptrdiff_t mul;
	bool aregloaded = false;

	parser_peektokens(parser, toks, ARRLEN(toks));
	iTok = 0;
	const bool horz = toks[iTok].type == '<' || toks[iTok].type == '>';
	const bool vert = toks[iTok].type == '.' || toks[iTok].type == '`';
	mul = toks[iTok].type == '<' || toks[iTok].type == '`' ? -1 : 1;
	w = toks[iTok].value;
	if(*w == 'w') {
		w++;
		wander = true;
	}
	if(*w == 'x') {
		w++;
		del = true;
	}
	iTok++;
	if(toks[iTok].type == '+')
		iTok++;
	else if(toks[iTok].type == '-') {
		mul = SAFE_MUL(mul, -1);
		iTok++;
	}
	if(toks[iTok].type == 'd') {
		ptrdiff_t val;

		(void) parseint(toks[iTok].value, &val);
		mul = SAFE_MUL(mul, val);
		iTok++;
	} else if(toks[iTok].type == 'w') {
		if(!strcmp(toks[iTok].value, "home")) {
			mul = SAFE_MUL(mul, PTRDIFF_MIN);
			iTok++;
		} else if(!strcmp(toks[iTok].value, "end")) {
			mul = SAFE_MUL(mul, PTRDIFF_MAX);
			iTok++;
		}
	}
	p = prog;
	if(toks[iTok].type == 'w') {
		if(!strcmp(toks[iTok].value, "A")) {
			/* do nothing */
			aregloaded = true;
		} else if(!strcmp(toks[iTok].value, "B")) {
			*(instr_t*) p = INSTR_LDAB;
			p += sizeof(instr_t);
			aregloaded = true;
		} else if(!strcmp(toks[iTok].value, "C")) {
			*(instr_t*) p = INSTR_LDAC;
			p += sizeof(instr_t);
			aregloaded = true;
		} else if(!strcmp(toks[iTok].value, "D")) {
			*(instr_t*) p = INSTR_LDAD;
			p += sizeof(instr_t);
			aregloaded = true;
		} else {
			parser_consumetokens(parser, iTok + 1);
			return parser_pusherror(parser, ERRINSTR_INVALIDWORD);
		}
		iTok++;
	}
	if(!aregloaded) {
		*(instr_t*) p = INSTR_LDA;
		p += sizeof(instr_t);
		*(ptrdiff_t*) p = mul;
		p += sizeof(ptrdiff_t);
	} else if(mul != 1) {
		*(instr_t*) p = INSTR_MULA;
		p += sizeof(instr_t);
		*(ptrdiff_t*) p = mul;
		p += sizeof(ptrdiff_t);
	}
	*(instr_t*) p = INSTR_CALL;
	p += sizeof(instr_t);
	if(wander && del) {
		parser_consumetokens(parser, iTok);
		return parser_pusherror(parser, ERRINSTR_MOVEDEL);
	}
	if(del)
		*(call_t*) p = vert ? CALL_DELETELINE : CALL_DELETE;
	else if(wander)
		*(call_t*) p = vert ? CALL_MOVEBELOWPAGE : CALL_MOVERIGHTPAGE;
	else
		*(call_t*) p = vert ? CALL_MOVEVERT : horz ? CALL_MOVEHORZ : CALL_MOVECURSOR;
	p += sizeof(call_t);
	parser_consumetokens(parser, iTok);
	parser_writeprogram(parser, prog, p - (void*) prog);
	return SUCCESS;
}

int
parser_getinsert(struct parser *parser)
{
	struct parser_token toks[2];
	uint8_t prog[sizeof(instr_t) + sizeof(ptrdiff_t) +
		sizeof(instr_t) + sizeof(call_t)];
	void *p;

	parser_peektokens(parser, toks, ARRLEN(toks));
	parser_consumetoken(parser);
	if(toks[1].type != '\"')
		return parser_pusherror(parser, ERRINSTR_STRING);
	parser_consumetoken(parser);
	char *const str = mode_allocstring(toks[1].value, strlen(toks[1].value));
	if(!str)
		return parser_pusherror(parser, ERR_OUTOFMEMORY);
	p = prog;
	*(instr_t*) p = INSTR_LDA;
	p += sizeof(instr_t);
	*(size_t*) p = (size_t) str;
	p += sizeof(size_t);
	*(instr_t*) p = INSTR_CALL;
	p += sizeof(instr_t);
	*(call_t*) p = CALL_INSERT;
	p += sizeof(call_t);
	parser_writeprogram(parser, prog, sizeof(prog));
	return SUCCESS;
}

int
parser_getassert(struct parser *parser)
{
	struct parser_token toks[2];
	uint8_t prog[sizeof(instr_t) + sizeof(ptrdiff_t) +
		sizeof(instr_t) + sizeof(call_t)];
	void *p;

	parser_peektokens(parser, toks, ARRLEN(toks));
	parser_consumetoken(parser);
	if(toks[1].type != '\"')
		return parser_pusherror(parser, ERRINSTR_STRING);
	parser_consumetoken(parser);
	char *const str = mode_allocstring(toks[1].value, strlen(toks[1].value));
	if(!str)
		return parser_pusherror(parser, ERR_OUTOFMEMORY);
	p = prog;
	*(instr_t*) p = INSTR_LDA;
	p += sizeof(instr_t);
	*(size_t*) p = (size_t) str;
	p += sizeof(size_t);
	*(instr_t*) p = INSTR_CALL;
	p += sizeof(instr_t);
	*(call_t*) p = CALL_ASSERTSTRING;
	p += sizeof(call_t);
	parser_writeprogram(parser, prog, sizeof(prog));
	return SUCCESS;
}
