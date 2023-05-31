#include "cnergy.h"

int
parser_checkbindword(struct parser *parser)
{
	return !strcmp(parser->word, "bind") ? COMMIT : FAIL;
}

int
parser_getbindmodeprefix(struct parser *parser)
{
	memset(&parser->mode, 0, sizeof(parser->mode));
	if(parser->c == '.' || parser->c == '$') {
		parser->mode.flags = parser->c == '.' ? FBIND_MODE_SUPPLEMENTARY : FBIND_MODE_TYPE;
		parser_getc(parser);
	}
	if(parser_getword(parser) != SUCCESS) {
		parser_pusherror(parser, ERR_EXPECTED_MODE_NAME);
		return FAIL;
	}
	strcpy(parser->mode.name, parser->word);
	return COMMIT;
}

int
parser_getbindmodesuffix(struct parser *parser)
{
	if(parser->c == '*') {
		parser->mode.flags |= FBIND_MODE_SELECTION;
		parser_getc(parser);
	}
	if(parser->c != ':') {
		parser_pusherror(parser, ERR_EXPECTED_COLON_MODE);
		return FAIL;
	}
	parser_getc(parser);
	return SUCCESS;
}

int
parser_getbindmodeextensions(struct parser *parser)
{
	struct append_request req;

	req.mode = NULL;
	while(parser_getwindowtype(parser), parser_getword(parser) == SUCCESS) {
		strcpy(req.donor, parser->word);
		req.windowType = parser->windowType ? : parser->files[parser->nFiles - 1].windowType;
		parser_addappendrequest(parser, &req);
		while(isblank(parser->c))
			parser_getc(parser);
		if(parser->c != ',')
			break;
		while(isblank(parser_getc(parser)));
	}
	if(parser->c != '\n' && parser->c != EOF) {
		parser_pusherror(parser, ERR_EXPECTED_LINE_BREAK_MODE);
		return FAIL;
	}
	return SUCCESS;
}

int
parser_addbindmode(struct parser *parser)
{
	struct binding_mode *nextMode;
	struct binding_mode *mode;
	U32 type;

	nextMode = malloc(sizeof(*nextMode));
	if(!nextMode)
		return OUTOFMEMORY;
	type = parser->windowType ? : parser->files[parser->nFiles - 1].windowType;
	mode = parser->firstModes[type];
	if(!mode)
		parser->firstModes[type] = nextMode;
	else {
		while(mode->next)
			mode = mode->next;
		mode->next = nextMode;
	}
	memcpy(nextMode, &parser->mode, sizeof(*nextMode));
	for(U32 i = parser->nAppendRequests; i; ) {
		i--;
		if(parser->appendRequests[i].mode)
			break;
		parser->appendRequests[i].mode = nextMode;
	}
	parser->curMode = nextMode;
	printf("added mode(%u): %s\n", type, nextMode->name);
	return FINISH;
}

