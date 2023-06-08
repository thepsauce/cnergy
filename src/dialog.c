#include "cnergy.h"

void
drawframe(const char *title, int y, int x, int ny, int nx)
{
	mvhline(y, x, ACS_HLINE, nx);
	mvhline(y + ny - 1, x, ACS_HLINE, nx);
	mvvline(y, x, ACS_VLINE, ny);
	mvvline(y, x + nx - 1, ACS_VLINE, ny);
	// draw corners
	mvaddch(y, x, ACS_ULCORNER);
	mvaddch(y, x + nx - 1, ACS_URCORNER);
	mvaddch(y + ny - 1, x, ACS_LLCORNER);
	mvaddch(y + ny - 1, x + nx - 1, ACS_LRCORNER);
	// draw title
	mvaddstr(y, x + 3, title);
	// erase content
	for(ny--, nx--; --ny;)
	for(int n = 1; n < nx; n++)
		mvaddch(y + ny, x + n, ' ');
}

int
messagebox(const char *title, const char *msg, ...)
{
	va_list l;
	const char *opt;
	const char *storedMsg = NULL;
	char buf[88];
	char options[8];
	unsigned nOptions = 0;
	int defaultOption = -1;
	int line, col;
	int lines, cols;
	int y, x;
	int maxY, maxX;

	line = LINES / 5;
	col = COLS / 5;
	lines = LINES * 3 / 5;
	cols = COLS * 3 / 5;
	drawframe(title, line, col, lines, cols);
	// draw message
	y = line + lines / 5;
	x = col + cols / 5;
	maxY = line + lines * 4 / 5;
	maxX = col + cols * 4 / 5;
	move(y, x);
	// for format
	va_start(l, msg);
	while(1) {
		char *space;
		int next_x;
		size_t n;

		if(!*msg) {
			if(!storedMsg || !*storedMsg)
				break;
			msg = storedMsg;
			storedMsg = NULL;
		}
		// intepret %
		if(*msg == '%') {
			msg++;
			switch(*msg) {
			case 0: msg--; break;
			case 's':
				msg++;
				storedMsg = msg;
				msg = va_arg(l, const char*);
				break;
			case 'c':
				msg++;
				storedMsg = msg;
				snprintf(buf, sizeof(buf), "%c", va_arg(l, char));
				msg = buf;
				break;
			case 'u':
				msg++;
				storedMsg = msg;
				snprintf(buf, sizeof(buf), "%u", va_arg(l, unsigned));
				msg = buf;
				break;
			case 'd':
				msg++;
				storedMsg = msg;
				snprintf(buf, sizeof(buf), "%d", va_arg(l, int));
				msg = buf;
				break;
			case 'z':
				msg++;
				storedMsg = msg;
				switch(*msg) {
				case 'd':
					msg++;
					snprintf(buf, sizeof(buf), "%zd", va_arg(l, ptrdiff_t));
					break;
				case 'u':
					msg++;
				/* fall through */
				default:
					snprintf(buf, sizeof(buf), "%zu", va_arg(l, size_t));
					break;
				}
				msg = buf;
				break;
			}
		}
		// break words at spaces
		space = strchr(msg, ' ');
		n = space ? (size_t) (space - msg) : strlen(msg);
		next_x = x + (int) n;
		if(next_x >= maxX && x != col + cols / 5) {
			x = col + cols / 5;
			y++;
			if(y == maxY)
				break;
			move(y, x);
		}
		n = MIN((int) n, maxX - x);
		addnstr(msg, n);
		if(space && next_x < maxX) {
			addch(' ');
			x++;
		}
		if(space)
			msg++;
		msg += n;
		x = next_x;
	}

	move(line + lines - 1, col + 3);
	while((opt = va_arg(l, const char*))) {
		const char *b = strchr(opt, '[');
		if(!b || nOptions == ARRLEN(options))
			return -1;
		if(b[1] == '*') {
			defaultOption = nOptions;
			b++;
		}
		options[nOptions++] = b[1];
		printw("%s ", opt);
	}
	va_end(l);

	const int choice = tolower(getch());
	for(unsigned i = 0; i < nOptions; i++)
		if(choice == tolower(options[i]))
			return i;
	return defaultOption;
}

void *
dialog_alloc(size_t sz)
{
	void *ptr;

	do {
		ptr = malloc(sz);
	} while(!ptr && !messagebox("Memory error",
				"Your system returned NULL when trying to allocate %zu bytes of memory, what do you want to do?",
				sz, "[T]ry again", "[C]ancel", NULL));
	return ptr;
}

void *
dialog_realloc(void *ptr, size_t newSz)
{
	do {
		ptr = realloc(ptr, newSz);
	} while(!ptr && !messagebox("Memory error",
				"Your system returned NULL when trying to allocate %zu bytes of memory, what do you want to do?",
				newSz, "[T]ry again", "[C]ancel", NULL));
	return ptr;
}
