int
c_state_linecomment(struct c_state *s)
{
	if(s->data[s->index] == '\n') {
		c_popstate(s);
		return 1;
	}
	if(s->data[s->index] == '\\' && s->data[s->index + 1] == '\n')
		s->index++;
	return 0;
}

int
c_state_multicomment(struct c_state *s)
{
	if(s->data[s->index] == '*' && s->data[s->index + 1] == '/') {
		s->index++;
		c_popstate(s);
	}
	return 0;
}
