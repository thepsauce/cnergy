int
c_state_string(struct c_state *s)
{
	if(s->data[s->index] == '\\') {
		U32 hexChars = 0;

		s->attr = COLOR_PAIR(10);
		s->index++;
		switch(s->data[s->index]) {
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
			s->attr = A_REVERSE | COLOR_PAIR(2);
		}
		while(hexChars) {
			if(!isxdigit(s->data[s->index + 1])) {
				s->attr = A_REVERSE | COLOR_PAIR(2);
				break;
			}
			s->index++;
			hexChars--;
		}
	} else {
		s->attr = COLOR_PAIR(11);
		if(s->data[s->index] == '\"' || s->data[s->index] == '\n')
			c_popstate(s);
	}
	return 0;
}

