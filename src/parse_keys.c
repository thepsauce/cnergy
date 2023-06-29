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
		case ' ': case '\n': case '\r':
		case '\v': case '\f':
			tok->value[0] = '\\';
			parser_ungetc(parser);
			break;
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
		int uc;
	} keyTranslations[] = {
		{ "HOME", SPECIAL_KEY(KEY_HOME) },
		{ "END", SPECIAL_KEY(KEY_END) },
		{ "LEFT", SPECIAL_KEY(KEY_LEFT) },
		{ "RIGHT", SPECIAL_KEY(KEY_RIGHT) },
		{ "UP", SPECIAL_KEY(KEY_UP) },
		{ "DOWN", SPECIAL_KEY(KEY_DOWN) },
		{ "SPACE", ' ' },
		{ "BACKSPACE", SPECIAL_KEY(KEY_BACKSPACE) },
		{ "BACK", SPECIAL_KEY(KEY_BACKSPACE) },
		{ "DELETE", SPECIAL_KEY(KEY_DC) },
		{ "DEL", SPECIAL_KEY(KEY_DC) },
		{ "TAB", '\t' },
		{ "ENTER", '\n' },
		{ "RETURN", '\n' },
		{ "ESCAPE", 0x1b },
		{ "ESC", 0x1b },
	};
	char utf8[4];
	unsigned len;
	char *newKeys;
	struct parser_token tok;

	parser_setcontext(parser, parser_getkeystoken);
	parser->nKeys = 0;
	while(parser_peektoken(parser, &tok) > 0 && !isspace(tok.type)) {
		switch(tok.type) {
		case '\\':
		case '^':
			parser_consumetoken(parser);
			utf8[0] = tok.value[0];
			len = 1;
			break;
		case '<': {
			unsigned i;

			parser_consumetoken(parser);
			for(i = 0; i < ARRLEN(keyTranslations); i++) {
				if(!strcasecmp(keyTranslations[i].word, tok.value)) {
					len = utf8_convunicode(keyTranslations[i].uc, utf8);
					break;
				}
			}
			if(i == ARRLEN(keyTranslations))
				return -1;
			break;
		}
		case '*':
			parser_consumetoken(parser);
			parser_peektoken(parser, &tok);
			if(tok.type == '*') {
				parser_consumetoken(parser);
				utf8[0] = -1;
			} else {
				utf8[0] = '*';
			}
			len = 1;
			break;
		case ',':
			parser_consumetoken(parser);
			parser_peektoken(parser, &tok);
			len = 1;
			if(isspace(tok.type)) {
				parser_consumetoken(parser);
				utf8[0] = 0;
				break;
			}
			utf8[0] = ',';
			break;
		default:
			parser_consumetoken(parser);
			utf8[0] = tok.type;
			len = 1;
		}
		newKeys = realloc(parser->keys, parser->nKeys + len);
		if(!newKeys)
			return parser_pusherror(parser, ERRK_ALLOCATING);
		parser->keys = newKeys;
		memcpy(parser->keys + parser->nKeys, utf8, len);
		parser->nKeys += len;
	}
	newKeys = realloc(parser->keys, parser->nKeys + 1);
	if(!newKeys)
		return parser_pusherror(parser, ERRK_ALLOCATING);
	parser->keys = newKeys;
	parser->keys[parser->nKeys++] = 0;
	return SUCCESS;
}
