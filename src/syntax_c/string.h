int
c_state_string(struct state *s)
{
	if(s->data[s->index] == '\\') {
		unsigned hexChars = 0;

		s->attr = A_REVERSE | COLOR_PAIR(2);
		switch(s->data[s->index + 1]) {
		case 'a':
		case 'b':
		case 'e':
		case 'f':
		case 'n':
		case 'r':
		case 'v':
		case 't':
		case '\\':
		case '\"':
		case '\'':
		case '\n':
			break;
		case 'x':
			hexChars = 2;
			break;
		case 'u':
			hexChars = 4;
			break;
		case 'U':
			hexChars = 8;
			break;
		default:
			return 0;
		}
		s->index++;
		while(hexChars) {
			if(!isxdigit(s->data[s->index + 1]))
				return 0;
			s->index++;
			hexChars--;
		}
		s->attr = COLOR_PAIR(10);
	} else {
		s->attr = COLOR_PAIR(11);
		if(s->data[s->index] == '\"' || s->data[s->index] == '\n')
			state_pop(s);
	}
	return 0;
}
