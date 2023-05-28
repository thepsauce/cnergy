#include "cnergy.h"

enum {
	CNERGY_STATE_DEFAULT,
	CNERGY_STATE_SKIP,
	CNERGY_STATE_APPENDLISTDELIM,
	CNERGY_STATE_APPENDLISTITEM,
	CNERGY_STATE_KEYLIST,
	CNERGY_STATE_KEYLISTCOMMA,
	CNERGY_STATE_CALLLIST,
	CNERGY_STATE_PARAM,
	CNERGY_STATE_PARAMSTRING,
};

int
cnergy_state_default(struct state *s)
{
	bool hasFlags = false;
	U32 index;

	s->attr = COLOR_PAIR(0);
	if(isspace(s->data[s->index]))
		return 0;
	switch(s->data[s->index]) {
	case '.':
	case '$':
		hasFlags = true;
		if(isblank(s->data[s->index + 1])) {
			s->state = CNERGY_STATE_KEYLIST;
			return 1;
		}
		if(!isalpha(s->data[s->index + 1]) && s->data[s->index + 1] != '_') {
			s->state = CNERGY_STATE_SKIP;
			s->attr = A_REVERSE | COLOR_PAIR(2);
			break;
		}
		s->index++;
	case 'a' ... 'z': case 'A' ... 'Z': case '_':
		index = s->index;
		while(isalnum(s->data[s->index + 1]) || s->data[s->index + 1] == '_')
			s->index++;
		if(s->data[s->index + 1] == '*') {
			s->index++;
			hasFlags = true;
		}
		if(s->data[s->index + 1] != ':') {
			if(hasFlags) {
				s->state = CNERGY_STATE_SKIP;
				s->attr = A_REVERSE | COLOR_PAIR(2);
			} else {
				s->index = index;
				s->state = CNERGY_STATE_KEYLIST;
				return 1;
			}
			break;
		}
		s->index++;
		s->state = CNERGY_STATE_APPENDLISTITEM;
		s->attr = COLOR_PAIR(3);
		break;
	case '#':
		if(s->data[s->index + 1] == '#') {
			while(s->index + 1 != s->nData && s->data[s->index + 1] != '\n')
				s->index++;
			s->state = CNERGY_STATE_APPENDLISTITEM;
			s->attr = COLOR_PAIR(9);
			break;
		}
	/* fall through */
	default:
		s->state = CNERGY_STATE_KEYLIST;
		return 1;
	}
	return 0;
}

int
cnergy_state_skip(struct state *s)
{
	if(s->data[s->index] == '\n')
		s->state = CNERGY_STATE_DEFAULT;
	else
		s->attr = A_REVERSE | COLOR_PAIR(2);
	return 0;
}

int
cnergy_state_appendlistdelim(struct state *s)
{
	STATE_SKIPSPACE(s);
	if(s->data[s->index] == ',') {
		s->state = CNERGY_STATE_APPENDLISTITEM;
		s->attr = COLOR_PAIR(8);
		return 0;
	}
	s->state = CNERGY_STATE_SKIP;
	return 1;
}

int
cnergy_state_appendlistitem(struct state *s)
{
	STATE_SKIPSPACE(s);
	if(!isalpha(s->data[s->index]) && s->data[s->index] != '_') {
		s->state = CNERGY_STATE_SKIP;
		return 1;
	}
	while(isalnum(s->data[s->index + 1]) || s->data[s->index + 1] == '_')
		s->index++;
	s->state = CNERGY_STATE_APPENDLISTDELIM;
	s->attr = COLOR_PAIR(0);
	return 0;
}

int
cnergy_state_keylistcomma(struct state *s)
{
	s->attr = COLOR_PAIR(6);
	s->state = CNERGY_STATE_KEYLIST;
	return 0;
}

int
cnergy_state_keylist(struct state *s)
{
	STATE_SKIPSPACE(s);
	if(isblank(s->data[s->index + 1])) {
		s->attr = COLOR_PAIR(5);
	} else if(isalpha(s->data[s->index])) {
		while(isalpha(s->data[s->index + 1]))
			s->index++;
		s->attr = COLOR_PAIR(5);
	} else if(s->data[s->index] == '^') {
		s->index++;
		s->attr = COLOR_PAIR(5);
	} else {
		s->state = CNERGY_STATE_CALLLIST;
		return 1;
	}
	if(s->data[s->index + 1] == ',')
		s->state = CNERGY_STATE_KEYLISTCOMMA;
	return 0;
}

int
cnergy_state_param(struct state *s)
{
	STATE_SKIPSPACE(s);
	while(!isspace(s->data[s->index + 1]))
		s->index++;
	s->attr = COLOR_PAIR(6);
	s->state = CNERGY_STATE_CALLLIST;
	return 0;
}

int
cnergy_state_paramstring(struct state *s)
{
	if(isspace(s->data[s->index])) {
		s->state = CNERGY_STATE_SKIP;
		return 1;
	}
	const bool quoted = s->data[s->index] == '\"';
	while(1) {
		if(s->index + 1 == s->nData ||
			(quoted && (s->data[s->index + 1] == '\"' || s->data[s->index + 1] == '\n')) ||
			(!quoted && isspace(s->data[s->index + 1])))
			break;
		if(s->data[s->index + 1] == '\\')
			s->index++;
		s->index++;
	}
	if(quoted && s->data[s->index + 1] == '\"')
		s->index++;
	s->attr = COLOR_PAIR(6);
	s->state = CNERGY_STATE_CALLLIST;
	return 0;
}

int
cnergy_state_calllist(struct state *s)
{
	STATE_SKIPSPACE(s);
	s->attr = COLOR_PAIR(0);
	switch(s->data[s->index]) {
	case '|':
	case '&':
	case '^':
		return 0;
	}
	switch(s->data[s->index]) {
	case '\n':
	case EOF:
		s->state = CNERGY_STATE_DEFAULT;
		break;
	case '(':
	case ')':
		if(s->data[s->index + 1] != s->data[s->index]) {
			s->state = CNERGY_STATE_SKIP;
			return 1;
		}
		s->index++;
		break;
	case '$':
	case '<':
	case '>':
	case '`':
	case '.':
		if(s->data[s->index + 1] == 'w' ||
				s->data[s->index + 1] == 'x')
			s->index++;
	/* fall through */
	case '#':
		s->state = CNERGY_STATE_PARAM;
		break;
	case '?':
	case '+':
		s->state = CNERGY_STATE_PARAMSTRING;
		break;
	case '!':
	case ':':
		s->state = CNERGY_STATE_PARAM;
		break;
	default:
		s->state = CNERGY_STATE_SKIP;
		return 1;
	}
	return 0;
}

int (*cnergy_states[])(struct state *s) = {
	[CNERGY_STATE_DEFAULT] = cnergy_state_default,
	[CNERGY_STATE_SKIP] = cnergy_state_skip,
	[CNERGY_STATE_APPENDLISTITEM] = cnergy_state_appendlistitem,
	[CNERGY_STATE_APPENDLISTDELIM] = cnergy_state_appendlistdelim,
	[CNERGY_STATE_KEYLIST] = cnergy_state_keylist,
	[CNERGY_STATE_KEYLISTCOMMA] = cnergy_state_keylistcomma,
	[CNERGY_STATE_CALLLIST] = cnergy_state_calllist,
	[CNERGY_STATE_PARAM] = cnergy_state_param,
	[CNERGY_STATE_PARAMSTRING] = cnergy_state_paramstring,
};
