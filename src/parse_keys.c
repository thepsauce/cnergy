#include "cnergy.h"

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
	struct parser_token toks[3];

	parser->nKeys = 0;
	while(parser_peektokens(parser, toks, ARRLEN(toks)) > 0) {
		switch(toks[0].type) {
		case 'w':
		case 'd': {
			char *const v = toks[0].value;
			const unsigned n = strlen(v);

			newKeys = realloc(parser->keys, sizeof(*parser->keys) * (parser->nKeys + n));
			if(!newKeys)
				return parser_pusherror(parser, ERRK_ALLOCATING);
			parser->keys = newKeys;
			for(unsigned i = 0; i < n; i++)
				parser->keys[i + parser->nKeys] = v[i];
			parser->nKeys += n;
			parser_consumetoken(parser);
			continue;
		}
		case '\\':
			switch(toks[0].value[0]) {
			case 'a': key = '\a'; break;
			case 'b': key = '\b'; break;
			case 'e': key = 0x1b; break;
			case 'f': key = '\f'; break;
			case 'n': key = '\n'; break;
			case 'r': key = '\r'; break;
			case 't': key = '\t'; break;
			case 'v': key = '\v'; break;
			default:
				key = toks[0].value[0];
			}
			break;
		case '<':
			if(toks[1].type == 'w' && toks[2].type == '>') {
				unsigned i;

				parser_consumetokens(parser, 3);
				for(i = 0; i < ARRLEN(keyTranslations); i++) {
					if(!strcasecmp(keyTranslations[i].word, toks[1].value)) {
						key = keyTranslations[i].key;
						break;
					}
				}
				if(i == ARRLEN(keyTranslations))
					return -1;
			} else {
				parser_consumetoken(parser);
				key = '<';
			}
			break;
		case '*':
			if(toks[1].type == '*') {
				parser_consumetokens(parser, 2);
				key = -1;
			} else {
				parser_consumetoken(parser);
				key = '*';
			}
			break;
		case ',':
			if(isspace(toks[1].type)) {
				parser_consumetokens(parser, 2);
				key = 0;
			} else {
				parser_consumetoken(parser);
				key = ',';
			}
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
