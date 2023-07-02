#include "cnergy.h"

struct binding_mode *
parser_findbindmode(struct parser *parser, const char *name)
{
	for(unsigned i = 0; i < parser->nModes; i++)
		if(!strcmp(parser->modes[i].name, name))
			return parser->modes + i;
	return NULL;
}

int
parser_addappendrequest(struct parser *parser, struct append_request *request)
{
	struct append_request *newRequests;

	newRequests = realloc(parser->appendRequests, sizeof(*parser->appendRequests) * (parser->nAppendRequests + 1));
	if(!newRequests)
		return parser_pusherror(parser, ERR_OUTOFMEMORY);
	parser->appendRequests = newRequests;
	parser->appendRequests[parser->nAppendRequests++] = *request;
	return SUCCESS;
}

int
parser_getbindmode(struct parser *parser)
{
	/**
	 * A bind mode takes up to 6 tokens, for instance:
	 * .foo::bar:
	 * After that come the mode extensions which may look like:
	 * ext1, ext2
	 */
	struct parser_token tokens[6];
	unsigned i = 0;
	char name[NAME_MAX];
	struct binding_mode *mode;

	parser_consumespace(parser);
	parser_peektokens(parser, tokens, ARRLEN(tokens));

	if(tokens[i].type == '.') {
		i++;
		name[0] = '.';
		name[1] = 0;
	} else if(tokens[i].type == '$') {
		i++;
		name[0] = '$';
		name[1] = 0;
	} else {
		name[0] = 0;
	}
	if(tokens[i].type == 'w') {
		if(tokens[i + 1].type == ':' && tokens[i + 2].type == ':' ) {
			if(name[0] == '.')
				return parser_pusherror(parser, ERRBM_SUPPPAGE);
			strcat(name, tokens[i].value);
			strcat(name, ":");
			i += 3;
			if(tokens[i].type != 'w') {
				if(isspace(tokens[i].type) && tokens[i + 1].type == 'w')
					return parser_pusherror(parser, ERRBM_NOSPACE);
				return parser_pusherror(parser, ERRBM_WORD);
			}
		}
		strcat(name, tokens[i].value);
		i++;
	}
	if(tokens[i].type == '*') {
		i++;
		if(name[0] == '.')
			return parser_pusherror(parser, ERRBM_SUPPVISUAL);
		if(name[0] == '$')
			return parser_pusherror(parser, ERRBM_SUPPINSERT);
		strcat(name, "*");
	}
	if(tokens[i].type != ':') {
		if(isspace(tokens[i].type) && tokens[i + 1].type == ':')
			return parser_pusherror(parser, ERRBM_NOSPACE);
		return parser_pusherror(parser, ERRBM_COLON);
	}
	parser_consumetokens(parser, i + 1);
	if(parser_findbindmode(parser, name)) {
		parser_pusherror(parser, ERRBM_DUP);
	} else {
		mode = realloc(parser->modes,
				sizeof(*parser->modes) * (parser->nModes + 1));
		if(!mode)
			return parser_pusherror(parser, ERR_OUTOFMEMORY);
		parser->modes = mode;
		mode += parser->nModes;
		parser->nModes++;
		memset(mode, 0, sizeof(*mode));
		strcpy(mode->name, name);
	}
	/* get mode extensions */
	while(parser_consumeblank(parser),
			parser_peektokens(parser, tokens, 3) > 0) {
		i = 0;
		if(tokens[i].type == 'w') {
			struct append_request req;

			strcpy(req.receiver, name);
			req.donor[0] = '.';
			strcpy(req.donor + 1, tokens[0].value);
			parser_addappendrequest(parser, &req);
			i++;
			if(tokens[i].type == ' ')
				i++;
			if(tokens[i].type == ',') {
				parser_consumetokens(parser, i + 1);
				continue;
			}
			if(tokens[i].type != '\n')
				return parser_pusherror(parser, ERRBM_INVALID);
		}
		if(tokens[i].type == '\n') {
			parser_consumetokens(parser, i + 1);
			break;
		}
		return parser_pusherror(parser, ERRBM_INVALID);
	}
	printf("added mode: %s\n", name);
	return SUCCESS;
}
