#include "cnergy.h"

int
parser_getld(struct parser *parser)
{
	struct parser_token toks[5];
	uint8_t prog[128];
	void *p;
	char sz = 0;
	int i = 2;

	// TODO: implement ld
	parser_peektokens(parser, toks, ARRLEN(toks));
	if(toks[1].type != ' ')
		return parser_pusherror(parser, ERRINSTR_INVALID);
	if(toks[2].type == 'w') {
		if(!strcmp(toks[2].value, "byte"))
			sz = 'b';
		else if(!strcmp(toks[2].value, "int"))
			sz = 'i';
		else if(!strcmp(toks[2].value, "pointer"))
			sz = 'p';
		if(sz) {
			if(toks[3].type != ' ')
				return parser_pusherror(parser, ERRINSTR_INVALID);
			i = 4;
		}
	}
	if(toks[i].type == 'd') {

	}
}

int
parser_getpsh(struct parser *parser)
{
	struct parser_token tok;
	instr_t instr;

	parser_peektoken(parser, &tok);
	parser_consumetoken(parser);
	switch(tok.value[3]) {
	case 'A': case 'a': instr = INSTR_PSHA; break;
	case 'B': case 'b': instr = INSTR_PSHB; break;
	case 'C': case 'c': instr = INSTR_PSHC; break;
	case 'D': case 'd': instr = INSTR_PSHD; break;
	}
	return parser_writeprogram(parser, &instr, sizeof(instr_t));
}

int
parser_getpop(struct parser *parser)
{
	struct parser_token tok;
	instr_t instr;

	parser_peektoken(parser, &tok);
	parser_consumetoken(parser);
	switch(tok.value[3]) {
	case 'A': case 'a': instr = INSTR_POPA; break;
	case 'B': case 'b': instr = INSTR_POPB; break;
	case 'C': case 'c': instr = INSTR_POPC; break;
	case 'D': case 'd': instr = INSTR_POPD; break;
	}
	return parser_writeprogram(parser, &instr, sizeof(instr));
}

int
parser_getinc(struct parser *parser)
{
	struct parser_token tok;
	instr_t instr;

	parser_peektoken(parser, &tok);
	parser_consumetoken(parser);
	switch(tok.value[3]) {
	case 'A': case 'a': instr = INSTR_INCA; break;
	case 'B': case 'b': instr = INSTR_INCB; break;
	case 'C': case 'c': instr = INSTR_INCC; break;
	case 'D': case 'd': instr = INSTR_INCD; break;
	}
	return parser_writeprogram(parser, &instr, sizeof(instr));
}

int
parser_getdec(struct parser *parser)
{
	struct parser_token tok;
	instr_t instr;

	parser_peektoken(parser, &tok);
	parser_consumetoken(parser);
	switch(tok.value[3]) {
	case 'A': case 'a': instr = INSTR_DECA; break;
	case 'B': case 'b': instr = INSTR_DECB; break;
	case 'C': case 'c': instr = INSTR_DECC; break;
	case 'D': case 'd': instr = INSTR_DECD; break;
	}
	return parser_writeprogram(parser, &instr, sizeof(instr));
}

int
parser_getjmp(struct parser *parser)
{
	struct parser_token toks[3];
	uint8_t prog[sizeof(instr_t) + sizeof(ptrdiff_t)];

	parser_peektokens(parser, toks, ARRLEN(toks));
	if(toks[1].type != ' ' || toks[2].type != 'w') {
		parser_consumetoken(parser);
		return parser_pusherror(parser, ERRINSTR_JMPWORD);
	}
	parser_consumetokens(parser, 3);
	*(instr_t*) prog = toks[0].value[1] == 'm' ? INSTR_JMP :
		toks[0].value[1] == 'z' ? INSTR_JZ : INSTR_JNZ;
	*(ptrdiff_t*) (prog + sizeof(instr_t)) =
		parser_getlabeladdress(parser, toks[2].value);
	return parser_writeprogram(parser, prog, sizeof(prog));
}

int
parser_getcall(struct parser *parser)
{
	struct parser_token toks[3];
	uint8_t prog[sizeof(instr_t) + sizeof(call_t)];

	parser_peektokens(parser, toks, ARRLEN(toks));
	if(toks[1].type != ' ' || toks[2].type != 'w') {
		parser_consumetoken(parser);
		return parser_pusherror(parser, ERRINSTR_JMPWORD);
	}
	parser_consumetokens(parser, 3);
	*(instr_t*) prog = INSTR_CALL;
	for(call_t i = 1; i < CALL_MAX; i++)
		if(callNames[i] && !strcasecmp(callNames[i], toks[2].value)) {
			*(call_t*) (prog + sizeof(instr_t)) = i;
			break;
		}
	return parser_writeprogram(parser, prog, sizeof(prog));
}

int
parser_getexit(struct parser *parser)
{
	struct parser_token toks[3];
	uint8_t prog[sizeof(instr_t) + sizeof(int)];
	ptrdiff_t code;

	parser_peektokens(parser, toks, ARRLEN(toks));
	if(toks[1].type != ' ' || toks[2].type != 'd') {
		parser_consumetoken(parser);
		return parser_pusherror(parser, ERRINSTR_EXITNUMBER);
	}
	parser_consumetokens(parser, 3);
	*(instr_t*) prog = INSTR_EXIT;
	(void) parseint(toks[2].value, &code);
	*(int*) (prog + sizeof(instr_t)) = code;
	return parser_writeprogram(parser, prog, sizeof(prog));
}
