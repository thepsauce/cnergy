#include "editor.h"

int
c_pushstate(struct c_state *s, U32 state)
{
	s->stateStack[s->state++] = state;
	return 0;
}

int
c_popstate(struct c_state *s)
{
	s->state = s->stateStack[--s->state];
	return 0;
}

static const char *C_keywords[] = {
	"auto",
	"__auto_type",
	"break",
	"case",
	"char",
	"const",
	"continue",
	"default",
	"do",
	"double",
	"else",
	"enum",
	"extern",
	"float",
	"for",
	"goto",
	"if",
	"int",
	"long",
	"register",
	"return",
	"short",
	"signed",
	"sizeof",
	"static",
	"struct",
	"switch",
	"typedef",
	"typeof",
	"union",
	"unsigned",
	"void",
	"volatile",
	"while",

	"FILE",
};

static const struct {
	char c, c2, to[4];
} C_chars[] = {
	{ ',',   0, "" },
	{ '.',   0, "" },
	{ ';',   0, "" },
	{ ':',   0, "" },
	{ '+',   0, "" },
	{ '-', '>', { 0xe2, 0x86, 0x92, 0 } },
	{ '*',   0, "" },
	{ '/',   0, "" },
	{ '%',   0, "" },
	{ '<', '=', { 0xe2, 0x89, 0xa4, 0 } },
	{ '>', '=', { 0xe2, 0x89, 0xa5, 0 } },
	{ '<', '<', { 0xc2, 0xab, 0 } },
	{ '>', '>', { 0xc2, 0xbb, 0 } },
	{ '=',   0, "" },
	{ '?',   0, "" },
	{ '!', '=', { 0xe2, 0x89, 0xa0, 0 } },
	{ '!', '!', { 0xe2, 0x80, 0xbc, 0 } },
	{ '~',   0, "" },
	{ '&',   0, "" },
	{ '|',   0, "" },
};

#include "src/syntax_c/string.h"
#include "src/syntax_c/number.h"
#include "src/syntax_c/preproc.h"

int
c_state_default(struct c_state *s, struct c_info *i)
{
	s->attr = COLOR_PAIR(0);
	switch(i->data[i->index]) {
	case 'a' ... 'z': case 'A' ... 'Z': case '_': {
		const U32 start = i->index;
		while(i->index != i->nData && isalnum(i->data[i->index]))
			i->index++;
		for(U32 k = 0; k < ARRLEN(C_keywords); k++)
			if(i->index - start == strlen(C_keywords[k]) &&
					!memcmp(C_keywords[k], i->data + start,
						i->index - start)) {
				s->attr = COLOR_PAIR(4);
				break;
			}
		i->index--;
		break;
	}
	case '0':
		c_pushstate(s, s->state);
		s->state = C_STATE_NUMBER_ZERO;
		s->attr = COLOR_PAIR(7);
		break;
	case '1' ... '9':
		c_pushstate(s, s->state);
		s->state = C_STATE_NUMBER_DECIMAL;
		s->attr = COLOR_PAIR(7);
		break;
	case '\"':
		c_pushstate(s, s->state);
		s->state = C_STATE_STRING;
		s->attr = COLOR_PAIR(11);
		break;
	case '#':
		s->state = C_STATE_PREPROC;
		s->attr = COLOR_PAIR(5);
		break;
	default:
		if(i->index + 1 != i->nData)
			for(U32 c = 0; c < ARRLEN(C_chars); c++)
				if(C_chars[c].c == i->data[i->index] &&
						C_chars[c].c2 == i->data[i->index + 1]) {
					s->attr = COLOR_PAIR(10);
					s->conceal = C_chars[c].to;
					i->index++;
					return 0;
				}
		for(U32 c = 0; c < ARRLEN(C_chars); c++)
			if(C_chars[c].c == i->data[i->index] && !C_chars[c].c2) {
				s->attr = COLOR_PAIR(10);
				break;
			}
	}
	return 0;
}

// 0 - all good
// 1 - push index (start long match)
// 2 - pop index (stop long match (colorize))
// 3 - clear index
int (*states[])(struct c_state *s, struct c_info *i) = {
	[C_STATE_DEFAULT] = c_state_default,

	[C_STATE_STRING] = c_state_string,
	[C_STATE_STRING_ESCAPE] = c_state_string_escape,
	[C_STATE_STRING_X1 ... C_STATE_STRING_X8] = c_state_string_x,
	
	[C_STATE_NUMBER_ZERO] = c_state_number_zero,
	[C_STATE_NUMBER_DECIMAL] = c_state_number_decimal,
	[C_STATE_NUMBER_HEX] = c_state_number_hex,
	[C_STATE_NUMBER_OCTAL] = c_state_number_octal,
	[C_STATE_NUMBER_BINARY] = c_state_number_binary,
	[C_STATE_NUMBER_EXP] = c_state_number_exp,
	[C_STATE_NUMBER_NEGEXP] = c_state_number_negexp,
	[C_STATE_NUMBER_FLOAT] = c_state_number_float,
	[C_STATE_NUMBER_ERRSUFFIX] = c_state_number_errsuffix,
	[C_STATE_NUMBER_LSUFFIX] = c_state_number_lsuffix,
	[C_STATE_NUMBER_USUFFIX] = c_state_number_usuffix,

	[C_STATE_PREPROC] = c_state_preproc,
	[C_STATE_PREPROC_COMMON] = c_state_preproc_common,
	[C_STATE_PREPROC_ESCAPE] = c_state_preproc_escape,
	[C_STATE_PREPROC_INCLUDE] = c_state_preproc_include,
};

int
c_feed(struct c_state *s, struct c_info *i)
{
	while(states[s->state](s, i));
	return 0;
}

