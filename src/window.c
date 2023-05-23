#include "cnergy.h"

struct window *first_window;
struct window *focus_window;
int focus_y, focus_x;

void
window_layout(struct window *win)
{
	struct window *w;
	int i;
	U32 nRight = 1, nBelow = 1;
	int linesPer, colsPer, linesRemainder, colsRemainder;
	int nextCol, nextLine;
	const int line = win->line;
	const int col = win->col;
	const int lines = win->lines;
	const int cols = win->cols;

	if(!win->left)
		for(w = win->right; w; w = w->right)
			nRight++;
	if(!win->above)
		for(w = win->below; w; w = w->below)
			nBelow++;
	linesPer = win->lines / nBelow;
	colsPer = win->cols / nRight;
	linesRemainder = win->lines % nBelow;
	colsRemainder = win->cols % nRight;

	win->cols = colsPer;
	win->lines = linesPer;
	nextCol = col + colsPer;
	nextLine = line + linesPer;
	// if there is a window left, it will handle the right windows, we don't want to handle them twice
	if(!win->left)
		for(w = win->right; w; w = w->right) {
			w->line = line;
			w->col = nextCol;
			w->lines = lines;
			w->cols = colsPer;
			if(colsRemainder) {
				colsRemainder--;
				w->cols++;
			}
			nextCol += w->cols;
			window_layout(w);
		}
	if(!win->above)
		for(i = 1, w = win->below; w; w = w->below, i++) {
			w->line = nextLine;;
			w->col = col;
			w->lines = linesPer;
			w->cols = cols;
			if(linesRemainder) {
				linesRemainder--;
				w->lines++;
			}
			nextLine += w->lines;
			window_layout(w);
		}
}

void
window_render(struct window *win)
{
	struct buffer *buf;
	U32 curLine, curCol;
	U32 saveiGap;
	U32 nLineNumbers;
	U32 minLine, maxLine, minCol, maxCol;
	U32 minSel, maxSel;

	U32 line, col;
	struct c_state state;

	// get buffer and cursor position
	buf = win->buffers + win->iBuffer;
	curLine = buffer_line(buf);
	curCol = buffer_col(buf);
	// move gap out of the way
	saveiGap = buf->iGap;
	unsafe_buffer_movecursor(buf, buf->nData - buf->iGap);
	// make sure there is an EOF at the end of the data
	if(!buf->nGap) {
		buf->data = realloc(buf->data, buf->nData + BUFFER_GAP_SIZE);
		buf->nGap = BUFFER_GAP_SIZE;
	}
	buf->data[buf->nData] = EOF;

	// get size of left pad
	nLineNumbers = 2;
	for(U32 i = buffer_line(buf); i; i /= 10)
		nLineNumbers++;
	nLineNumbers = MAX(nLineNumbers, 4);

	// scroll the text if the caret is out of view
    if(curLine < win->vScroll)
        win->vScroll = curLine;
    if(curLine >= win->lines - 1 + win->vScroll)
        win->vScroll = curLine - (win->lines - 2);

	// note that we scroll a little extra horizontally
    if(curCol - 1 <= win->hScroll)
        win->hScroll = MAX((int) curCol - win->cols / 4, 0);
    if(curCol >= win->cols - nLineNumbers + win->hScroll)
        win->hScroll = curCol - (win->cols - nLineNumbers - 2) + win->cols / 4;

	// get bounds of text
	minLine = win->vScroll;
	maxLine = win->vScroll + win->lines - 1;
	minCol = win->hScroll;
	maxCol = win->hScroll - nLineNumbers + win->cols;

	// get bounds of selection
	if(mode_has(FBIND_MODE_SELECTION)) {
		if(win->selection > saveiGap) {
			minSel = saveiGap;
			maxSel = win->selection;
		} else {
			minSel = win->selection;
			maxSel = saveiGap;
		}
	} else {
		// no selection
		minSel = 1;
		maxSel = 0;
	}
	
	// setup loop
	line = 0;
	col = 0;
	move(win->line, win->col);
	// draw first line if needed
	if(!win->vScroll) {
		attrset(COLOR(3, 8));
		mvprintw(win->line, win->col, "%*d ", nLineNumbers - 1, 1);
	}
	memset(&state, 0, sizeof(state));
	state.data = buf->data;
	state.nData = buf->nData;

	// draw all data
	for(U32 i = 0; i < buf->nData; state.index = i) {
		c_feed(&state);
		if(state.conceal) {
			const char *conceal;
				
			conceal = state.conceal;
			state.conceal = NULL;
			if(line != curLine) {
				i = state.index + 1;
				if(line >= minLine && line < maxLine) {
					attrset(state.attr);
					while(*conceal) {
						const U32 len = utf8_len(*conceal);
						if(col >= minCol && col < maxCol)
							addnstr(conceal, len);
						conceal += len;
						col++;
					}
				}
				continue;
			}
		}
		// check if the state interpreted the utf8 encoded character correctly (if not we handle it here)
		if((buf->data[i] & 0x80) && state.index == i) {
			attrset(state.attr);
			const U32 len = utf8_len(buf->data[i]);
			if(col >= minCol && col < maxCol)
				addnstr(buf->data + i, len);
			i += len;
			col += utf8_width(buf->data + i, col);
			continue;
		}
		for(; i <= state.index; i++) {
			const char ch = buf->data[i];
			attrset(state.attr);
			if(i >= minSel && i <= maxSel)
				attron(A_REVERSE);
			if(ch == '\t') {
				do {
					if(line >= minLine && line < maxLine && col >= minCol && col < maxCol)
						addch(' ');
				} while((++col) % TABSIZE);
			} else if(ch == '\n') {
				line++;
				attrset(COLOR(3, 8));
				if(line >= minLine && line < maxLine)
					mvprintw(win->line + line - minLine, win->col, "%*d ", nLineNumbers - 1, line + 1);
				col = 0;
			} else if(iscntrl(ch)) {
				if(line >= minLine && line < maxLine) {
					if(col >= minCol && col < maxCol)
						addch('^');
					if(col + 1 >= minCol && col + 1 < maxCol)
						addch(ch != 0x7f ? ch + 'A' + 1 : '<');
				}
				col += 2;
			} else {
				if(line >= minLine && line < maxLine && col >= minCol && col < maxCol)
					addch(ch);
				col++;
			}
		}
	}
	// draw little waves until we hit the status bar
	attrset(COLOR(6, 0));
	for(line++; line + 1 < maxLine; line++)
		mvprintw(line + win->line - minLine, win->col, "%*s~ ", nLineNumbers - 2, "");
	// draw status bar
	attrset(COLOR(6, 8));
	mvaddstr(win->line + win->lines - 1, win->col, "Buffer ");
	attrset(COLOR(3, 8));
	printw("%u, %u", curLine, curCol);
	printw("%*s", MAX(win->col + win->cols - getcurx(stdscr), 0), "");
	// set the global end caret position
	if(win == focus_window) {
		focus_y = win->line + curLine - minLine;
		focus_x = win->col + curCol + nLineNumbers - minCol;
	}
	// move back the gap before we leave
	unsafe_buffer_movecursor(buf, saveiGap - buf->iGap);
	if(win->right)
		window_render(win->right);
	if(win->below)
		window_render(win->below);
}
