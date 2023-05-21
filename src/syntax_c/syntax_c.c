#include "cnergy.h"

int
c_pushstate(struct c_state *s, U32 state)
{
	s->stateStack[s->iStack++] = state;
	return 0;
}

int
c_popstate(struct c_state *s)
{
	s->state = s->stateStack[--s->iStack];
	return 0;
}

static const struct {
	int pair;
	// note: be sure to increase this number when a list exceeds 32 words; the word list is null terminated
	const char *words[32];
} C_keywords[] = {
	{ 12, {
		"__auto_type",
		"char",
		"double",
		"float",
		"int",
		"long",
		"short",
		"signed",
		"sizeof",
		"typedef",
		"typeof",
		"unsigned",
		"void",
		"FILE",
		NULL,
	} },
	{ 4, {
		"auto",
		"const",
		"enum",
		"extern",
		"register",
		"static",
		"struct",
		"union",
		"volatile",
		NULL,
	} },
	{ 10, {
		"break",
		"case",
		"continue",
		"default",
		"do",
		"else",
		"for",
		"goto",
		"if",
		"return",
		"switch",
		"while",
		NULL,
	} },
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

int
c_state_common(struct c_state *s)
{
	switch(s->data[s->index]) {
	case 'a' ... 'z': case 'A' ... 'Z': case '_': {
		const U32 startWord = s->index;
		while(isalnum(s->data[++s->index]) || s->data[s->index] == '_');
		const U32 nWord = s->index - startWord;
		s->index--;
		for(U32 k = 0; k < ARRLEN(C_keywords); k++)
		for(const char *const *ws = C_keywords[k].words, *w; w = *ws; ws++)
			if(nWord == strlen(w) && !memcmp(w, s->data + startWord, nWord)) {
				s->attr = COLOR_PAIR(C_keywords[k].pair);
				break;
			}
		break;
	}
	case '.':
		if(!isdigit(s->data[s->index + 1]))
			goto case_char;
		c_pushstate(s, s->state);
		s->state = C_STATE_NUMBER_FLOAT;
		s->attr = COLOR_PAIR(14);
		break;
	case '0':
		c_pushstate(s, s->state);
		s->state = C_STATE_NUMBER_ZERO;
		s->attr = COLOR_PAIR(14);
		break;
	case '1' ... '9':
		c_pushstate(s, s->state);
		s->state = C_STATE_NUMBER_DECIMAL;
		s->attr = COLOR_PAIR(14);
		break;
	case '\"':
		c_pushstate(s, s->state);
		s->state = C_STATE_STRING;
		s->attr = COLOR_PAIR(11);
		break;
	case '/':
		if(s->data[s->index + 1] == '/') {
			s->index++;
			c_pushstate(s, s->state);
			s->state = C_STATE_LINECOMMENT;
			s->attr = COLOR_PAIR(5);
			break;
		} else if(s->data[s->index + 1] == '*') {
			s->index++;
			c_pushstate(s, s->state);
			s->state = C_STATE_MULTICOMMENT;
			s->attr = COLOR_PAIR(5);
			break;
		}
	default:
	case_char:
		for(U32 c = 0; c < ARRLEN(C_chars); c++)
			if(C_chars[c].c == s->data[s->index] &&
					C_chars[c].c2 == s->data[s->index + 1]) {
				s->attr = COLOR_PAIR(13);
				s->conceal = C_chars[c].to;
				s->index++;
				return 0;
			}
		for(U32 c = 0; c < ARRLEN(C_chars); c++)
			if(C_chars[c].c == s->data[s->index] && !C_chars[c].c2) {
				s->attr = COLOR_PAIR(13);
				break;
			}
	}
	return 0;
}

#include "string.h"
#include "number.h"
#include "preproc.h"
#include "comment.h"

int
c_state_default(struct c_state *s)
{
	switch(s->data[s->index]) {
	case '#':
		s->state = C_STATE_PREPROC;
		s->attr = COLOR_PAIR(5);
		break;
	default: {
		const char c = s->data[s->index];
		s->attr = 0;
		c_state_common(s);
		// if there was word but no keyword, look ahead for a function call
		if(!s->attr && (isalpha(c) || c == '_')) {
			U32 i = s->index;
			while(isspace(s->data[i + 1]))
				i++;
			if(s->data[i + 1] == '(')
				s->attr = A_BOLD;
		}
	}
	}
	return 0;
}

int (*states[])(struct c_state *s) = {
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
	[C_STATE_PREPROC_INCLUDE] = c_state_preproc_include,

	[C_STATE_LINECOMMENT] = c_state_linecomment,
	[C_STATE_MULTICOMMENT] = c_state_multicomment,
};

int
c_feed(struct c_state *s)
{
	while(states[s->state](s));
	return 0;
}

