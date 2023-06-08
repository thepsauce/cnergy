static const struct {
	const char *name;
	unsigned state;
} C_preprocTransitions[] = {
	{ "define"		, C_STATE_PREPROC_DEFINE },
	{ "elif"		, C_STATE_PREPROC_COMMON },
	{ "else"		, C_STATE_PREPROC_COMMON },
	{ "endif"		, C_STATE_PREPROC_COMMON },
	{ "error"		, C_STATE_PREPROC_COMMON },
	{ "ident"		, C_STATE_PREPROC_COMMON },
	{ "if"			, C_STATE_PREPROC_COMMON },
	{ "ifdef"		, C_STATE_PREPROC_COMMON },
	{ "ifndef"		, C_STATE_PREPROC_COMMON },
	{ "include"		, C_STATE_PREPROC_INCLUDE },
	{ "include_next", C_STATE_PREPROC_INCLUDE },
	{ "line"		, C_STATE_PREPROC_COMMON },
	{ "pragma"		, C_STATE_PREPROC_COMMON },
	{ "sccs"		, C_STATE_PREPROC_COMMON },
	{ "undef"		, C_STATE_PREPROC_UNDEF },
	{ "warning"		, C_STATE_PREPROC_COMMON },
	{ "warn"		, C_STATE_PREPROC_COMMON },
};

int
c_state_preproc_common(struct state *s)
{
	STATE_SKIPSPACE(s);
	switch(s->data[s->index]) {
	case '\\':
		if(s->data[s->index + 1] == '\n')
			s->index++;
		s->attr = COLOR_PAIR(5);
		break;
	case '\n':
		s->state = C_STATE_DEFAULT;
		break;
	default:
		s->attr = COLOR_PAIR(5);
		return c_state_common(s);
	}
	return 0;
}

int
c_state_preproc(struct state *s)
{
	STATE_SKIPSPACE(s);
	switch(s->data[s->index]) {
	case 'a' ... 'z': case 'A' ... 'Z': case '_': {
		const size_t startWord = s->index;
		while(isalnum(s->data[++s->index]) || s->data[s->index] == '_');
		const size_t nWord = s->index - startWord;
		s->index--;
		for(size_t t = 0; t < ARRLEN(C_preprocTransitions); t++)
			if(!strncmp(C_preprocTransitions[t].name, s->data + startWord, nWord) &&
					!C_preprocTransitions[t].name[nWord]) {
				s->attr = COLOR_PAIR(5);
				s->state = C_preprocTransitions[t].state;
				return 0;
			}
		s->attr = A_REVERSE | COLOR_PAIR(2);
		s->state = C_STATE_PREPROC_COMMON;
		break;
	}
	default:
		return c_state_preproc_common(s);
	}
	return 0;
}

int
c_state_preproc_define(struct state *s)
{
	STATE_SKIPSPACE(s);
	switch(s->data[s->index]) {
	case 'a' ... 'z': case 'A' ... 'Z': case '_': {
		const size_t startWord = s->index;
		while(isalnum(s->data[++s->index]) || s->data[s->index] == '_');
		const size_t nWord = s->index - startWord;
		s->index--;
		sortedlist_add(&s->words, s->data + startWord, nWord, (void*) (intptr_t) COLOR_PAIR(5));
		s->attr = COLOR_PAIR(5);
		s->state = C_STATE_PREPROC_COMMON;
		break;
	}
	default:
		s->state = C_STATE_PREPROC_COMMON;
		return 1;
	}
	return 0;
}

int
c_state_preproc_include(struct state *s)
{
	STATE_SKIPSPACE(s);
	switch(s->data[s->index]) {
	case '<':
	case '\"': {
		const char termin = s->data[s->index] == '<' ? '>' : '\"';
		char ch;
		while(1) {
			s->index++;
			ch = s->data[s->index];
			if(ch == EOF) {
				s->index--;
				break;
			}
			if(ch == '\n' || ch == termin)
				break;
			if(ch == '\\') {
				s->index++;
				if(s->index + 1 == s->nData)
					break;
			}
		}
		s->attr = ch == termin ? COLOR_PAIR(11) : COLOR_PAIR(5);
		s->state = C_STATE_PREPROC_COMMON;
		break;
	}
	default:
		s->state = C_STATE_PREPROC_COMMON;
		return 1;
	}
	return 0;
}

int
c_state_preproc_undef(struct state *s)
{
	STATE_SKIPSPACE(s);
	switch(s->data[s->index]) {
	case 'a' ... 'z': case 'A' ... 'Z': case '_': {
		const size_t startWord = s->index;
		while(isalnum(s->data[++s->index]) || s->data[s->index] == '_');
		const size_t nWord = s->index - startWord;
		s->index--;
		sortedlist_remove(&s->words, s->data + startWord, nWord);
		s->attr = COLOR_PAIR(5);
		s->state = C_STATE_PREPROC_COMMON;
		break;
	}
	default:
		s->state = C_STATE_PREPROC_COMMON;
		return 1;
	}
	return 0;
}
