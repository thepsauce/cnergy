static const struct {
	const char *name;
	U32 state;
} C_preprocTransitions[] = {
	{ "define"      , C_STATE_PREPROC_COMMON },
	{ "elif"        , C_STATE_PREPROC_COMMON },
	{ "else"        , C_STATE_PREPROC_COMMON },
	{ "endif"       , C_STATE_PREPROC_COMMON },
	{ "error"       , C_STATE_PREPROC_COMMON },
	{ "ident"       , C_STATE_PREPROC_COMMON },
	{ "if"          , C_STATE_PREPROC_COMMON },
	{ "ifdef"       , C_STATE_PREPROC_COMMON },
	{ "ifndef"      , C_STATE_PREPROC_COMMON },
	{ "import"      , C_STATE_PREPROC_INCLUDE },
	{ "include"     , C_STATE_PREPROC_INCLUDE },
	{ "include_next", C_STATE_PREPROC_INCLUDE },
	{ "line"        , C_STATE_PREPROC_COMMON },
	{ "pragma"      , C_STATE_PREPROC_COMMON },
	{ "sccs"        , C_STATE_PREPROC_COMMON },
	{ "undef"       , C_STATE_PREPROC_COMMON },
	{ "warning"     , C_STATE_PREPROC_COMMON },
};

int
c_state_preproc_common(struct c_state *s, struct c_info *i)
{
	switch(i->data[i->index]) {
	case ' ': case '\t':
		s->attr = COLOR_PAIR(5);
		break;
	case '\\':
		c_pushstate(s, s->state);
		s->state = C_STATE_PREPROC_ESCAPE;
		s->attr = COLOR_PAIR(5);
		break;
	case '\n':
		s->state = C_STATE_DEFAULT;
		break;
	default:
		s->attr = A_REVERSE | COLOR_PAIR(2);
		break;
	}
	return 0;
}

int
c_state_preproc_escape(struct c_state *s, struct c_info *i)
{
	switch(i->data[i->index]) {
	case ' ': case '\t':
	default:
		s->attr = A_REVERSE | COLOR_PAIR(2);
		break;
	case '\n':
		c_popstate(s);
		break;
	}
	return 0;
}

int
c_state_preproc(struct c_state *s, struct c_info *i)
{
	switch(i->data[i->index]) {
	case 'a' ... 'z': case 'A' ... 'Z': case '_': {
		const U32 start = i->index;
		while(i->index != i->nData && isalnum(i->data[i->index]))
			i->index++;
		for(U32 t = 0; t < ARRLEN(C_preprocTransitions); t++)
			if(i->index - start == strlen(C_preprocTransitions[t].name) &&
					!memcmp(C_preprocTransitions[t].name, i->data + start,
						i->index - start)) {
				s->attr = COLOR_PAIR(5);
				s->state = C_preprocTransitions[t].state;
				i->index--;
				return 0;
			}
		s->attr = A_REVERSE | COLOR_PAIR(2);
		s->state = C_STATE_PREPROC_COMMON;
		i->index--;
		break;
	}
	default:
		return c_state_preproc_common(s, i);
	}
	return 0;
}

int
c_state_preproc_include(struct c_state *s, struct c_info *i)
{
	switch(i->data[i->index]) {
	case '<':
	case '\"': {
		const char termin = i->data[i->index] == '<' ? '>' : '\"';
		while(i->index + 1 != i->nData) {
			i->index++;
			if(i->data[i->index] == '\n' || i->data[i->index] == termin)
				break;
			if(i->data[i->index] == '\\') {
				i->index++;
				if(i->index + 1 == i->nData)
					break;
			}
		}
		s->attr = i->data[i->index] == termin ? COLOR_PAIR(11) : COLOR_PAIR(5);
		s->state = C_STATE_PREPROC_COMMON;
		break;
	}
	default:
		return c_state_preproc_common(s, i);
	}
	return 0;
}

int
c_state_preproc_word(struct c_state *s, struct c_info *i)
{
	switch(i->data[i->index]) {
	case 'a' ... 'z': case 'A' ... 'Z': case '_': {
		U32 n = 0;
		do {
			n++;
		} while(i->index + 1 != i->nData && isalnum(i->data[i->index]));
		s->attr =  COLOR_PAIR(5);
		break;
	}
	}
	return 0;
}

