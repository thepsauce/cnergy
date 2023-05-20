int
c_state_string_x(struct c_state *s, struct c_info *i)
{
	if(!isxdigit(i->data[i->index])) {
		s->state = C_STATE_STRING;
		return 1;
	}
	s->attr = COLOR_PAIR(10);
	if(s->state == C_STATE_STRING_X1)
		s->state = C_STATE_STRING;
	else
		s->state--;
	return 0;
}

int
c_state_string_escape(struct c_state *s, struct c_info *i)
{
	switch(i->data[i->index]) {
	case 'a':
	case 'b':
	case 'e':
	case 'f':
	case 'n':
	case 'r':
	case 'v':
	case '\\':
	case '\"':
	case '\'':
	case '\n':
		s->state = C_STATE_STRING;
		s->attr = COLOR_PAIR(10);
		break;
	case 'x':
		s->state = C_STATE_STRING_X2;
		s->attr = COLOR_PAIR(10);
		break;
	case 'u':
		s->state = C_STATE_STRING_X4;
		s->attr = COLOR_PAIR(10);
		break;
	case 'U':
		s->state = C_STATE_STRING_X8;
		s->attr = COLOR_PAIR(10);
		break;
	default:
		s->state = C_STATE_STRING;
		s->attr = A_REVERSE | COLOR_PAIR(2);
	}
	return 0;
}

int
c_state_string(struct c_state *s, struct c_info *i)
{
	if(i->data[i->index] == '\\') {
		s->state = C_STATE_STRING_ESCAPE;
		s->attr = COLOR_PAIR(10);
	} else {
		s->attr = COLOR_PAIR(11);
		if(i->data[i->index] == '\"' || i->data[i->index] == '\n')
			c_popstate(s);
	}
	return 0;
}

