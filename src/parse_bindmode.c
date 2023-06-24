#include "cnergy.h"

int
parser_getbindmode(struct parser *parser)
{
	/**
	 * A bind mode takes up to 8 tokens in extreme cases, for instance:
	 * .foo::bar*:
	 * After that come the mode extensions which may look like:
	 * ext1, ext2
	 */
	struct parser_token tokens[7];
	unsigned i = 0;
	struct binding_mode mode, *prevMode, *nextMode;
	window_type_t type = WINDOW_ALL;

	memset(&mode, 0, sizeof(mode));
	parser_consumespace(parser);
	parser_peektokens(parser, tokens, ARRLEN(tokens));
	if(tokens[i].type == '.') {
		i++;
		mode.flags |= FBIND_MODE_SUPPLEMENTARY;
	} else if(tokens[i].type == '$') {
		i++;
		mode.flags |= FBIND_MODE_TYPE;
	}
	if(tokens[i].type != 'w')
		return -1;
	if(tokens[i + 1].type == ':' &&
			tokens[i + 2].type == ':' ) {
		if(parser_windowtype(parser, tokens[i].value, &type))
			parser_pusherror(parser, ERR_INVALID_WINDOWTYPE);
		i += 3;
		if(tokens[i].type != 'w') {
			if(isspace(tokens[i].type) && tokens[i + 1].type == 'w')
				return parser_pusherror(parser, ERRBM_NOSPACE);
			return parser_pusherror(parser, ERRBM_WORD);
		}
	}
	strcpy(mode.name, tokens[i].value);
	i++;
	if(tokens[i].type == '*') {
		i++;
		mode.flags |= FBIND_MODE_SELECTION;
	}
	if(tokens[i].type != ':') {
		if(isspace(tokens[i].type) && tokens[i + 1].type == ':')
			return parser_pusherror(parser, ERRBM_NOSPACE);
		return parser_pusherror(parser, ERRBM_COLON);
	}
	parser_consumetokens(parser, i + 1);
	/* get mode extensions */
	while(parser_consumeblank(parser),
			parser_peektokens(parser, tokens, 3) > 0) {
		i = 0;
		if(tokens[i].type == 'w') {
			struct append_request req;

			i++;
			if(tokens[i].type == ' ')
				i++;
			if(tokens[i].type == ',') {
				parser_consumetokens(parser, i + 1);
				continue;
			}
			if(tokens[i].type != '\n')
				return parser_pusherror(parser, ERRBM_INVALID);
			req.mode = NULL;
			strcpy(req.donor, tokens[0].value);
			req.windowType = type;
			parser_addappendrequest(parser, &req);
		}
		if(tokens[i].type == '\n') {
			parser_consumetokens(parser, i + 1);
			break;
		}
		return parser_pusherror(parser, ERRBM_INVALID);
	}

	nextMode = malloc(sizeof(*nextMode));
	if(!nextMode)
		return parser_pusherror(parser, ERR_OUTOFMEMORY);
	prevMode = parser->firstModes[type];
	if(!prevMode)
		parser->firstModes[type] = nextMode;
	else {
		while(prevMode->next)
			prevMode = prevMode->next;
		prevMode->next = nextMode;
	}
	memcpy(nextMode, &mode, sizeof(*nextMode));
	for(unsigned i = parser->nAppendRequests; i; ) {
		i--;
		if(parser->appendRequests[i].mode)
			break;
		parser->appendRequests[i].mode = nextMode;
	}
	parser->curMode = nextMode;
	printf("added mode(%u): %s\n", type, nextMode->name);
	return SUCCESS;
}
