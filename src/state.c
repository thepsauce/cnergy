#include "cnergy.h"

int
state_push(struct state *s, U32 state)
{
	s->stateStack[s->iStack++] = state;
	return 0;
}

int
state_pop(struct state *s)
{
	s->state = s->stateStack[--s->iStack];
	return 0;
}

void
state_skipspace(struct state *s)
{
	while(isblank(s->data[s->index]))
		s->index++;
}

int
state_addword(struct state *s, int attr, const char *word, U32 nWord)
{
	char *w;

	for(U32 i = 0; i < s->nWords; i++)
		if(s->words[i].attr == attr && 
				strlen(s->words[i].word) == nWord && 
				!strcmp(s->words[i].word, word))
			return 1;
	s->words = realloc(s->words, sizeof(*s->words) * (s->nWords + 1));
	if(!s->words)
		return -1;
	w = alloca(nWord + 1);
	if(!w)
		return -1;
	memcpy(w, word, nWord);
	w[nWord] = 0;
	w = const_alloc(w, nWord + 1);
	if(!w)
		return -1;
	s->words[s->nWords].attr = attr;
	s->words[s->nWords].word = w;
	s->nWords++;
	return 0;
}

int
state_removeword(struct state *s, int attr, const char *word, U32 nWord)
{
	for(U32 i = 0; i < s->nWords; i++)
		if(s->words[i].attr == attr && 
				strlen(s->words[i].word) == nWord && 
				!strcmp(s->words[i].word, word)) {
			s->nWords--;
			memmove(s->words + i, s->words + i + 1, sizeof(*s->words) * (s->nWords - i));
			return 0;
		}
	return 1;
}

int
state_addparan(struct state *s)
{
	struct state_paran *newParans;

	newParans = realloc(s->parans, sizeof(*s->parans) * (s->nParans + 1));
	if(!newParans)
		return -1;
	s->parans = newParans;
	s->parans[s->nParans++] = (struct state_paran) {
		.line = s->line, .col = s->col, .ch = s->data[s->index]
	};
	if(s->index == s->cursor) {
		s->highlightparan = s->parans[s->nParans - 1];
		return 1;
	}
	return 0;
}

int
state_addcounterparan(struct state *s, int counter) 
{
	while(s->nParans && s->parans[s->nParans - 1].ch != counter) {
		struct state_paran *newMisparans;

		newMisparans = realloc(s->misparans, sizeof(*s->misparans) * (s->nMisparans + 1));
		if(newMisparans) {
			s->misparans = newMisparans;
			s->misparans[s->nMisparans++] = s->parans[s->nParans - 1];
		}
		s->nParans--;
	}
	if(!s->nParans)
		return 2;
	const struct state_paran p = s->parans[--s->nParans];
	if(s->index == s->cursor) {
		s->highlightparan = p;
		return 1;
	}
	if(!memcmp(&s->highlightparan, &p, sizeof(struct state_paran))) {
		s->highlightparan.ch = 0;
		return 1;
	}
	return 0;
}

// Note: ONLY call this inside of c_cleanup
void
state_addchar(struct state *s, int line, int col, char ch, int attr)
{
	if(col >= s->minCol && col < s->maxCol &&
			line >= s->minLine && line < s->maxLine) {
		attrset(attr);
		mvaddch(s->win->line + line - s->minLine,
				s->win->col + col + s->minCol - s->maxCol + s->win->cols, ch);
	}
}

int
state_cleanup(struct state *s)
{
	// highlight mismatched parans
	while(s->nParans) {
		const struct state_paran p = s->parans[--s->nParans];
		state_addchar(s, p.line, p.col, p.ch, A_REVERSE | COLOR_PAIR(2));
	}
	while(s->nMisparans) {
		const struct state_paran p = s->misparans[--s->nMisparans];
		state_addchar(s, p.line, p.col, p.ch, A_REVERSE | COLOR_PAIR(2));
	}
	if(s->highlightparan.ch) {
		const struct state_paran p = s->highlightparan;
		state_addchar(s, p.line, p.col, p.ch, A_BOLD | COLOR_PAIR(7));
	}
	free(s->words);
	free(s->parans);
	free(s->misparans);
	return 0;
}

int
state_continue(struct state *s)
{
	while(s->states[s->state](s));
	return 0;
}
