#include "cnergy.h"

static int
parser_getinstruction(struct parser *parser)
{
	struct parser_token toks[5];

	// TODO: implement all instructions (1000 lines of code to follow ..... :( )
	if(parser_peektokens(parser, toks, ARRLEN(toks)) > 0 && toks[0].type != '}')
		parser_consumetoken(parser);
	return SUCCESS;
}

int
parser_getprogram(struct parser *parser)
{
	bool multi = false;
	struct parser_token tok;

	parser_consumeblank(parser);
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
		}
		parser_getinstruction(parser);
	}
	
	return SUCCESS;
}
