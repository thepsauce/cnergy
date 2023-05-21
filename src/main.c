#include "cnergy.h"
#include <locale.h>

U32
compute_scrolling_position(U32 cursor, U32 lines, U32 prevScroll) {
    if(cursor < prevScroll)
        return cursor;
    if(cursor >= lines + prevScroll)
        return cursor - (lines - 1);
    return prevScroll;
}

// TODO: move this to a better place
U32
utf8_strlen(const char *str)
{
	U32 l = 0;
	while(*str) {
		if((*str & 0xe0) == 0xc0)
			str += 2;
		else if((*str & 0xf0) == 0xe0)
			str += 3;
		else if((*str & 0xf8) == 0xf0)
			str += 4;
		else
			str++;
		l++;
	}
	return l;
}

// TODO: This functino needs cleanup
// there's also a problem when a line is exceeded horizontally
void
render(struct window *window)
{
	const U32 nLineNumbers = 4; // TODO: dynamically set this
	U32 cursorLine, cursorCol;
	U32 line, col;
	U32 sMin = 1, sMax = 0;
	struct c_state state;
	struct buffer *const buf = window->buffers + window->iBuffer;
	const U32 saveiGap = buf->iGap; // used to restore the gap position
	cursorLine = buffer_line(buf);
	cursorCol = buffer_col(buf);
	// move gap out of the way
	unsafe_buffer_movecursor(buf, buf->nData - buf->iGap);
	// TODO: this might crash if the gap size is zero
	// hardcoded fix:
	if(!buf->nGap) {
		buf->data = realloc(buf->data, buf->nData + BUFFER_GAP_SIZE);
		buf->nGap = BUFFER_GAP_SIZE;
	}
	buf->data[buf->nData] = EOF;

	line = window->line;
	col = window->col;

    if(cursorLine < window->vScroll)
        window->vScroll = cursorLine;
    if(cursorLine >= window->lines + window->vScroll)
        window->vScroll = cursorLine - (window->lines - 1);

	if(mode_has(FBIND_MODE_SELECTION)) {
		if(window->selection > saveiGap) {
			sMin = saveiGap;
			sMax = window->selection;
		} else {
			sMin = window->selection;
			sMax = saveiGap;
		}
	}
	if(!window->vScroll) {
		attrset(COLOR(242, 236));
		mvprintw(window->line, window->col, "%*d ", nLineNumbers - 1, line + 1);
	}
	memset(&state, 0, sizeof(state));
	state.data = buf->data;
	state.nData = buf->nData;
	for(U32 i = 0; i < buf->nData;) {
		state.index = i;
		const int r = c_feed(&state);
		attrset(state.attr);
		if(state.conceal) {
			const char *const conceal = state.conceal;
			state.conceal = NULL;
			if(line != cursorLine) {
				i = state.index + 1;
				if(line >= window->vScroll && line < window->vScroll + window->lines)
					mvaddstr(line - window->vScroll, col - window->hScroll + nLineNumbers, conceal);
				col += utf8_strlen(conceal);
				continue;
			}
		}
		for(; i <= state.index; i++) {
			if(i >= sMin && i <= sMax)
				attron(A_REVERSE);
			const char ch = buf->data[i];
			if(ch == '\t') {
				while((++col) % TABSIZE);
			} else if(ch == '\n') {
				line++;
				col = 0;
				if(line >= window->vScroll && line < window->vScroll + window->lines) {
					attrset(COLOR(242, 236));
					mvprintw(line - window->vScroll, window->col, "%*d ", nLineNumbers - 1, line + 1);
					attrset(state.attr);
				}
			} else if(ch < 32) {
				if(line >= window->vScroll && line < window->vScroll + window->lines) {
					attrset(COLOR_PAIR(1));
					mvaddch(line - window->vScroll, col - window->hScroll + nLineNumbers, '^');
					mvaddch(line - window->vScroll, col - window->hScroll + nLineNumbers, ch + 64);
					attrset(state.attr);
				}
				col += 2;
			} else {
				if(line >= window->vScroll && line < window->vScroll + window->lines)
					mvaddch(line - window->vScroll, col - window->hScroll + nLineNumbers, ch);
				col++;
			}
			if(i >= sMin && i <= sMax)
				attroff(A_REVERSE);
		}
	}
	move(cursorLine - window->vScroll, cursorCol - window->hScroll + nLineNumbers);
	unsafe_buffer_movecursor(buf, saveiGap - buf->iGap);
}

int
main(int argc, char **argv)
{
	setlocale(LC_ALL, "");

	FILE *f;

	for(int i = 0; i < 3; i++) {
		f = fopen("draft.cng", "r");

		printf("RET: %d\n", bind_parse(f));

		long pos = ftell(f);
		int line = 1;
		int col = 1;
		int c;
		fseek(f, 0, SEEK_SET);
		while(--pos) {
			if(fgetc(f) == '\n') {
				line++;
				col = 1;
			} else
				col++;
		}
		printf("l: %d, c: %d\n", line, col);

		fclose(f);
		print_modes(NULL, 0);
	}

#define PRINT_BUF \
	printf("%u, %u ;; %.*s[%u]%.*s ;; %.*s\n", buf.nData, buf.iGap, \
			buf.iGap, buf.data, \
			buf.nGap, \
			buf.nData - buf.iGap, buf.data + buf.iGap + buf.nGap, \
			buf.nData + buf.nGap, buf.data);
	struct buffer buf;
	initscr();

	raw();
	noecho();
	keypad(stdscr, true);
	scrollok(stdscr, true);
	clearok(stdscr, false);
	set_tabsize(4);

	if(has_colors()) {
		start_color();
		U8 gruvbox_colors[16][3] = {
			{40, 40, 40},   // 1: Black
			{204, 36, 29},  // 2: Red
			{152, 151, 26}, // 3: Green
			{215, 153, 33}, // 4: Yellow
			{69, 133, 136}, // 5: Blue
			{177, 98, 134}, // 6: Magenta
			{104, 157, 106}, // 7: Cyan
			{204, 204, 204}, // 8: Light gray
			{55, 54, 54},   // 9: Dark gray
			{251, 73, 52},  // 10: Bright red
			{184, 187, 38}, // 11: Bright green
			{250, 189, 47}, // 12: Bright yellow
			{131, 165, 152}, // 13: Bright blue
			{211, 134, 155}, // 14: Bright magenta
			{108, 181, 131}, // 15: Bright cyan
			{254, 254, 254} // 16: White
		};
		for(U32 i = 0; i < ARRLEN(gruvbox_colors); i++)
			init_color(i,
					(U32) gruvbox_colors[i][0] * 1000 / 256,
					(U32) gruvbox_colors[i][1] * 1000 / 256,
					(U32) gruvbox_colors[i][2] * 1000 / 256);
		for(U32 i = 0; i < COLORS; i++)
		for(U32 j = 0; j < COLORS; j++)
			init_pair(i + j * COLORS + 1, i, j);
	}

	int keys[32];
	U16 nKeys = 0;
	I32 num = 0;

	struct window window = {
		.line = 0,
		.col = 0,
		.lines = 18,
		.cols = 40,
		.nBuffers = 1,
		.iBuffer = 0,
		.vScroll = 0,
	};
	window.buffers = malloc(sizeof(*window.buffers)),
	memset(window.buffers, 0, sizeof(*window.buffers));

	FILE *fp = fopen("src/main.c", "r");
	buffer_insert_file(window.buffers, fp);
	fclose(fp);
	//buffer_insert(window.buffers, "#include");

	while(1) {
		render(&window);

		const int c = getch();
		clear();
		if(c == 27 && (num || nKeys)) {
			num = 0;
			nKeys = 0;
			continue;
		}
		if(!nKeys) {
			if(!mode_has(FBIND_MODE_TYPE) && isdigit(c) && (c != '0' || num)) {
				num = SAFE_MUL(num, 10);
				num = SAFE_ADD(num, c - '0');
				move(LINES - 1, 0);
				if(num)
					printw("%d", num);
				continue;
			}
		}
		keys[nKeys] = c;
		if(c == 0x03) // ^C
			break;
		move(LINES - 1, 0);
		if(num)
			printw("%d", num);
		for(U32 i = 0; i <= nKeys; i++)
			printw("%s", keyname(keys[i]));
		if(nKeys + 1 < ARRLEN(keys))
			nKeys++;
		keys[nKeys] = 0;
		move(10, 0);
		if(exec_bind(keys, num ? num : 1, &window) != 1) {
			nKeys = 0;
			num = 0;
		}
	}
	endwin();
	return 0;
}
