#include "cnergy.h"

static void
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
	const char *storedMsg;
	char buf[88];
	char options[8];
	int nOption = 0;
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
		long int n;

		while(!*msg) {
			if(!storedMsg)
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
				storedMsg = msg;
				msg = va_arg(l, const char*);
				break;
			case 'u':
				storedMsg = msg;
				snprintf(buf, sizeof(buf), "%u", va_arg(l, U32));
				msg = buf;
				break;
			case 'd':
				storedMsg = msg;
				snprintf(buf, sizeof(buf), "%d", va_arg(l, I32));
				msg = buf;
				break;
			}
		}
		// break words at spaces
		space = strchr(msg, ' ');
		n = space ? space - msg : (long int) strlen(msg);
		if(x + n >= maxX && x != col + cols / 5) {
			x = col + cols / 5;
			y++;
			if(y == maxY)
				break;
			move(y, x);
		}
		n = MIN(n, maxX - x);
		addnstr(msg, n);
		if(space && x + n < maxX) {
			addch(' ');
			x++;
		}
		if(space)
			msg++;
		msg += n;
		x += n;
	}

	move(line + lines - 1, col + 3);
	while((opt = va_arg(l, const char*))) {
		const char *b = strchr(opt, '[');
		if(!b || nOption == ARRLEN(options))
			return -1;
		if(b[1] == '*') {
			defaultOption = nOption;
			b++;
		}
		options[nOption++] = b[1];
		printw("%s ", opt);
	}
	va_end(l);

	const int choice = tolower(getch());
	for(int i = 0; i < nOption; i++)
		if(choice == tolower(options[i]))
			return i;
	return defaultOption;
}

void *
safe_alloc(U32 sz)
{
	void *ptr;

	do {
		ptr = malloc(sz);
	} while(!ptr && !messagebox("Memory error",
				"Your system returned NULL when trying to allocate %u bytes of memory, what do you want to do?",
				sz, "[T]ry again", "[C]ancel", NULL));
	return ptr;
}

void *
safe_realloc(void *ptr, U32 newSz)
{
	do {
		ptr = realloc(ptr, newSz);
	} while(!ptr && !messagebox("Memory error",
				"Your system returned NULL when trying to allocate %u bytes of memory, what do you want to do?",
				newSz, "[T]ry again", "[C]ancel", NULL));
	return ptr;
}



