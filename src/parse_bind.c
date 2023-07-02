#include "cnergy.h"

int
parser_getbind(struct parser *parser)
{
	char *keys;
	void *program;
	struct binding_mode *mode;
	struct binding *bind;

	if(!parser->nModes)
		return parser_pusherror(parser, ERRBIND_OUTSIDE);
	parser_consumespace(parser);
	parser_getkeys(parser);
	parser_getprogram(parser);
	if(parser->nErrors)
		return SUCCESS; /* don't allocate anything if an error occured */
	mode = parser->modes + parser->nModes - 1;
	bind = realloc(mode->bindings, sizeof(*mode->bindings) * (mode->nBindings + 1));
	if(!bind)
		return parser_pusherror(parser, ERR_OUTOFMEMORY);
	mode->bindings = bind;
	if(!(keys = mode_allockeys(parser->keys, parser->nKeys)))
		return parser_pusherror(parser, ERR_OUTOFMEMORY);
	if(!(program = mode_allocprogram(parser->program, parser->szProgram)))
		return parser_pusherror(parser, ERR_OUTOFMEMORY);
	bind += mode->nBindings;
	mode->nBindings++;
	*bind = (struct binding) {
		.nKeys = parser->nKeys,
		.szProgram = parser->szProgram,
		.keys = keys,
		.program = program
	};
	return SUCCESS;
}

