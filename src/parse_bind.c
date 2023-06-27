#include "cnergy.h"

int
parser_getbind(struct parser *parser)
{
	int *keys;
	void *program;
	struct binding *newBindings;

	if(!parser->curMode)
		return parser_pusherror(parser, ERRBIND_OUTSIDE);
	parser_consumespace(parser);
	parser_getkeys(parser);
	parser_getprogram(parser);
	if(parser->nErrors)
		return SUCCESS; /* don't allocate anything if an error occured */
	newBindings = realloc(parser->curMode->bindings, sizeof(*parser->curMode->bindings) * (parser->curMode->nBindings + 1));
	if(!newBindings)
		return parser_pusherror(parser, ERR_OUTOFMEMORY);
	if(!(keys = mode_allockeys(parser->keys, parser->nKeys)))
		return parser_pusherror(parser, ERR_OUTOFMEMORY);
	if(!(program = mode_allocprogram(parser->program, parser->szProgram)))
		return parser_pusherror(parser, ERR_OUTOFMEMORY);
	parser->curMode->bindings = newBindings;
	parser->curMode->bindings[parser->curMode->nBindings++] = (struct binding) {
		.nKeys = parser->nKeys,
		.szProgram = parser->szProgram,
		.keys = keys,
		.program = program,
	};
	return SUCCESS;
}

