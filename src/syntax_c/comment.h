int
c_state_linecomment(struct state *s)
{
	if(s->data[s->index] == '\n') {
		state_pop(s);
		return 1;
	}
	if(s->data[s->index] == '\\' && s->data[s->index + 1] == '\n')
		s->index++;
	return 0;
}

int
c_state_multicomment(struct state *s)
{
	if(s->data[s->index] == '*' && s->data[s->index + 1] == '/') {
		s->index++;
		state_pop(s);
	}
	return 0;
}
