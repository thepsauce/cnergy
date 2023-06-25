#include "cnergy.h"

static int
parser_getkeystoken(struct parser *parser, struct parser_token *tok)
{
	unsigned nValue = 0;

	switch(parser->c) {
	case '\\':
		switch(parser_getc(parser)) {
		case 'a': tok->value[0] = '\a'; break;
		case 'b': tok->value[0] = '\b'; break;
		case 'e': tok->value[0] = 0x1b; break;
		case 'f': tok->value[0] = '\f'; break;
		case 'n': tok->value[0] = '\n'; break;
		case 'r': tok->value[0] = '\r'; break;
		case 't': tok->value[0] = '\t'; break;
		case 'v': tok->value[0] = '\v'; break;
		default:
			tok->value[0] = parser->c;
		}
		tok->type = '\\';
		break;
	case '<':
		if(isalpha(parser_getc(parser))) {
			do {
				tok->value[nValue++] = parser->c;
			} while(isalpha(parser_getc(parser)));
			tok->value[nValue] = 0;
			if(parser->c != '>')
				return parser_pusherror(parser, ERRK_MISSING_CLOSING_ANGLE_BRACKET);
			parser_getc(parser);
		}
		tok->type = '<';
		break;
	case '^':
		if(isgraph(parser_getc(parser))) {
			tok->value[0] = parser->c - ('A' - 1);
			parser_getc(parser);
		} else {
			tok->value[0] = '^';
		}
		tok->type = '^';
		break;
	default:
		return 1;
	}
	return SUCCESS;
}

int
parser_getkeys(struct parser *parser)
{
	static const struct {
		const char *word;
		int key;
	} keyTranslations[] = {
		{ "HOME", KEY_HOME },
		{ "END", KEY_END },
		{ "LEFT", KEY_LEFT },
		{ "RIGHT", KEY_RIGHT },
		{ "UP", KEY_UP},
		{ "DOWN", KEY_DOWN },
		{ "SPACE", ' ' },
		{ "BACKSPACE", KEY_BACKSPACE },
		{ "BACK", KEY_BACKSPACE },
		{ "DELETE", KEY_DC },
		{ "DEL", KEY_DC },
		{ "TAB", '\t' },
		{ "ENTER", '\n' },
		{ "RETURN", '\n' },
		{ "ESCAPE", 0x1b },
		{ "ESC", 0x1b },
	};
	int key;
	int *newKeys;
	struct parser_token toks[2];

	parser_setcontext(parser, parser_getkeystoken);
	parser->nKeys = 0;
	while(parser_peektokens(parser, toks, ARRLEN(toks)) > 0) {
		switch(toks[0].type) {
		case '\\':
		case '^':
			parser_consumetoken(parser);
			key = toks[0].value[0];
			break;
		case '<': {
			unsigned i;

			parser_consumetoken(parser);
			for(i = 0; i < ARRLEN(keyTranslations); i++) {
				if(!strcasecmp(keyTranslations[i].word, toks[0].value)) {
					key = keyTranslations[i].key;
					break;
				}
			}
			if(i == ARRLEN(keyTranslations))
				return -1;
			break;
		}
		case '*':
			parser_consumetoken(parser);
			if(toks[1].type == '*') {
				parser_consumetoken(parser);
				key = -1;
			} else {
				key = '*';
			}
			break;
		case ',':
			parser_consumetoken(parser);
			if(isspace(toks[1].type)) {
				parser_consumetoken(parser);
				key = 0;
				break;
			}
			key = ',';
			break;
		case ' ':
		case '\n':
			goto end;
		default:
			parser_consumetoken(parser);
			key = toks[0].type;
		}
		newKeys = realloc(parser->keys, sizeof(*parser->keys) * (parser->nKeys + 1));
		if(!newKeys)
			return parser_pusherror(parser, ERRK_ALLOCATING);
		parser->keys = newKeys;
		parser->keys[parser->nKeys++] = key;
	}
end:
	newKeys = realloc(parser->keys, sizeof(*parser->keys) * (parser->nKeys + 1));
	if(!newKeys)
		return parser_pusherror(parser, ERRK_ALLOCATING);
	parser->keys = newKeys;
	parser->keys[parser->nKeys++] = 0;
	return SUCCESS;
}
