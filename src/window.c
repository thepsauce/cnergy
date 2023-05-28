#include "cnergy.h"

// All windows in here are rendered
struct window **all_windows;
U32 n_windows;
// THE origin (used for layout)
struct window *first_window;
// Active window
struct window *focus_window;
int focus_y, focus_x;

struct window *
window_new(struct buffer *buf)
{
	struct window **newWindows;
	struct window *win;

	newWindows = safe_realloc(all_windows, sizeof(*all_windows) * (n_windows + 1));
	if(!newWindows)
		return NULL;
	win = safe_alloc(sizeof(*win));
	if(!win)
		return NULL;
	memset(win, 0, sizeof(*win));
	win->type = WINDOW_BUFFER;
	win->buffer = buf;
	all_windows = newWindows;
	all_windows[n_windows++] = win;
	return win;
}

int
window_close(struct window *win)
{
	if(win == first_window)
		first_window = win->below ? win->below : win->right;
	if(win == focus_window)
		focus_window = win->above ? win->above :
			win->below ? win->below :
			win->right ? win->right :
			win->left ? win->left : first_window;
	window_detach(win);
	window_delete(win);
	return 0;
}

void
window_delete(struct window *win)
{
	for(U32 i = 0; i < n_windows; i++)
		if(all_windows[i] == win) {
			n_windows--;
			memmove(all_windows + i, all_windows + i + 1, sizeof(*all_windows) * (n_windows - i));
			break;
		}
	free(win);
}

struct window *
window_atpos(int y, int x)
{
	for(U32 i = 0; i < n_windows; i++) {
		struct window *const w = all_windows[i];
		if(y >= w->line && y < w->line + w->lines &&
				x >= w->col && x < w->col + w->cols)
			return w;
	}
	return NULL;
}

struct window *
window_above(struct window *win)
{
	const int yHit = win->line - 1;
	const int xHit = win == focus_window ? focus_x : win->col + win->cols / 2;
	return window_atpos(yHit, xHit);
}

struct window *
window_below(struct window *win)
{
	const int yHit = win->line + win->lines;
	const int xHit = win == focus_window ? focus_x : win->col + win->cols / 2;
	return window_atpos(yHit, xHit);
}

struct window *
window_left(struct window *win)
{
	const int yHit = win == focus_window ? focus_y : win->line + win->lines / 2;
	const int xHit = win->col - 1;
	return window_atpos(yHit, xHit);
}

struct window *
window_right(struct window *win)
{
	const int yHit = win == focus_window ? focus_y : win->line + win->lines / 2;
	const int xHit = win->col + win->cols;
	return window_atpos(yHit, xHit);
}

void
window_attach(struct window *to, struct window *win, int pos)
{
	U32 nVert;
	U32 nHorz;
	struct window *w;

	switch(pos) {
	case ATT_WINDOW_VERTICAL: goto vertical_attach;
	case ATT_WINDOW_HORIZONTAL: goto horizontal_attach;
	}
	// find shortest list
	nVert = 0;
	for(w = to; w->above; w = w->above);
	for(; w->below; w = w->below)
		nVert++;

	nHorz = 0;
	for(w = to; w->left; w = w->left);
	for(; w->right; w = w->right)
		nHorz++;
	if(nHorz < nVert)
		goto horizontal_attach;
vertical_attach:
	if(to->below)
		to->below->above = win;
	win->below = to->below;
	win->above = to;
	to->below = win;
	return;
horizontal_attach:
	if(to->right)
		to->right->left = win;
	win->right = to->right;
	win->left = to;
	to->right = win;
}

void
window_detach(struct window *win)
{
	if(win->above)
		win->above->below = win->below;
	if(win->below)
		win->below->above = win->above;
	if(win->left)
		win->left->right = win->right;
	if(win->right)
		win->right->left = win->left;
	win->above = NULL;
	win->below = NULL;
	win->left = NULL;
	win->right = NULL;
}

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
	// if there is a window left, it will not handle the right windows, we don't want to handle them twice
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
			w->line = nextLine;
			w->col = col;
			// note that we use linesPer and not lines here
			w->lines = linesPer;
			w->cols = colsPer;
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
	struct state state;
	U32 minSel, maxSel;

	// don't need to draw empty windows
	if(win->lines <= 0 || win->cols <= 0)
		return;
	// get buffer and cursor position
	buf = win->buffer;
	curLine = buffer_line(buf);
	curCol = buffer_col(buf);
	// move gap out of the way
	saveiGap = buf->iGap;
	unsafe_buffer_movecursor(buf, buf->nData - buf->iGap);
	// make sure there is an EOF at the end of the data (this is for convenience)
	if(!buf->nGap) {
		char *const newData = safe_realloc(buf->data, buf->nData + BUFFER_GAP_SIZE);
		if(!newData)
			return;
		buf->data = newData;
		buf->nGap = BUFFER_GAP_SIZE;
	}
	buf->data[buf->nData] = EOF;

	// prepare state
	memset(&state, 0, sizeof(state));

	// scroll the text if the caret is out of view
	if(curLine < win->vScroll)
		win->vScroll = curLine;
	if(curLine >= win->lines - 1 + win->vScroll)
		win->vScroll = curLine - (win->lines - 2);

	state.minLine = win->vScroll;
	state.maxLine = win->vScroll + win->lines - 1;

	// get width of line numbers
	nLineNumbers = 1; // initial padding to the right side
	if(window_left(win))
		nLineNumbers++; // add some padding
	for(U32 i = state.maxLine; i; i /= 10)
		nLineNumbers++;

	// note that we scroll a little extra horizontally
	// TODO: fix bug where scrolling will sometimes cause the cursor to be misaligned with a multi width character
	if(curCol <= win->hScroll)
		win->hScroll = MAX((int) curCol - win->cols / 4, 0);
	if(curCol >= win->cols - nLineNumbers + win->hScroll)
		win->hScroll = curCol - (win->cols - nLineNumbers - 2) + win->cols / 4;

	state.minCol = win->hScroll;
	state.maxCol = win->hScroll - nLineNumbers + win->cols;

	// get bounds of selection
	if(win == focus_window && mode_has(FBIND_MODE_SELECTION)) {
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
	move(win->line, win->col);
	// draw first line if needed
	if(!win->vScroll) {
		attrset(win == focus_window ? COLOR(11, 8) : COLOR(3, 8));
		printw("%*d ", nLineNumbers - 1, 1);
	}
	state.win = win;
	state.data = buf->data;
	state.nData = buf->nData;
	state.cursor = saveiGap;
	state.line = 0;
	state.col = 0;

	// draw all data
	for(U32 i = 0; i < buf->nData; state.index = i) {
		if(win->states)
			state_continue(&state);
		if(state.conceal) {
			const char *conceal;

			conceal = state.conceal;
			state.conceal = NULL;
			if(state.line != curLine) {
				i = state.index + 1;
				if(state.line >= state.minLine && state.line < state.maxLine) {
					attrset(state.attr);
					while(*conceal) {
						const U32 len = utf8_len(conceal, strlen(conceal));
						if(state.col >= state.minCol && state.col < state.maxCol)
							addnstr(conceal, len);
						conceal += len;
						state.col++;
					}
				}
				continue;
			}
		}
		// check if the state interpreted the utf8 encoded character correctly (if not we handle it here)
		if((buf->data[i] & 0x80) && state.index == i) {
			attrset(state.attr);
			const U32 len = utf8_len(buf->data + i, buf->nData - i);
			if(state.col >= state.minCol && state.col < state.maxCol) {
				if(i >= minSel && i <= maxSel)
					attron(A_REVERSE);
				if(!utf8_valid(buf->data + i, buf->nData - i)) {
					attron(COLOR(4, 0));
					if(state.col >= state.minCol && state.col < state.maxCol)
						addch('^');
					if(state.col + 1 >= state.minCol && state.col + 1 < state.maxCol)
						addch('?');
				} else {
					addnstr(buf->data + i, len);
				}
			}
			state.col += utf8_width(buf->data + i, buf->nData - i, state.col);
			i += len;
			continue;
		}
		for(; i <= state.index; i++) {
			const char ch = buf->data[i];
			attrset(state.attr);
			if(i >= minSel && i <= maxSel)
				attron(A_REVERSE);
			if(ch == '\t') {
				do {
					if(state.line >= state.minLine && state.line < state.maxLine &&
							state.col >= state.minCol && state.col < state.maxCol)
						addch(' ');
				} while((++state.col) % TABSIZE);
			} else if(ch == '\n') {
				if(state.line >= state.minLine && state.line < state.maxLine) {
					// draw extra space for selection
					if(i >= minSel && i <= maxSel) {
						if(state.col >= state.minCol && state.col < state.maxCol) {
							attrset(A_REVERSE);
							addch(' ');
						}
						state.col++;
					}
					// erase rest of line
					attrset(0);
					state.col = MAX(state.col, state.minCol);
					if(state.col < state.maxCol)
						printw("%*s", state.maxCol - state.col, "");
				}
				state.line++;
				if(state.line >= state.minLine && state.line < state.maxLine) {
					// draw line prefix
					attrset(win == focus_window ? COLOR(11, 8) : COLOR(3, 8));
					mvprintw(win->line + state.line - state.minLine, win->col, "%*d ", nLineNumbers - 1, state.line + 1);
				}
				state.col = 0;
			} else if(iscntrl(ch)) {
				if(state.line >= state.minLine && state.line < state.maxLine) {
					attron(COLOR(4, 0));
					if(state.col >= state.minCol && state.col < state.maxCol)
						addch('^');
					if(state.col + 1 >= state.minCol && state.col + 1 < state.maxCol)
						addch(ch == 0x7f ? '?' : ch + 'A' - 1);
				}
				state.col += 2;
			} else {
				if(state.line >= state.minLine && state.line < state.maxLine && state.col >= state.minCol && state.col < state.maxCol)
					addch(ch);
				state.col++;
			}
		}
	}
	// erase rest of line and draw little waves until we hit the status bar
	attrset(COLOR(6, 0));
	while(state.line < state.maxLine) {
		state.col = MAX(state.col, state.minCol);
		if(state.col < state.maxCol)
			printw("%*s", state.maxCol - state.col, "");
		state.line++;
		if(state.line >= state.maxLine)
			break;
		mvprintw(state.line + win->line - state.minLine, win->col, "%*s~ ", nLineNumbers - 2, "");
		state.col = 0;
	}
	state_cleanup(&state);
	// draw status bar (TODO: what if the status bar doesn't fit?)
	attrset(win == focus_window ? COLOR(14, 8) : COLOR(6, 8));
	mvaddstr(win->line + win->lines - 1, win->col, " Buffer");
	attrset(win == focus_window ? COLOR(11, 8) : COLOR(3, 8));
	if(buf->saveEvent != buf->iEvent)
		addch('*');
	if(buf->file)
		printw(" %s", buf->file);
	printw(" %u, %u", curLine, curCol);
	printw("%*s", MAX(win->col + win->cols - getcurx(stdscr), 0), "");
	// set the global end caret position
	if(win == focus_window) {
		focus_y = win->line + curLine - state.minLine;
		focus_x = win->col + curCol + nLineNumbers - state.minCol;
	}
	// move back the gap before we leave
	unsafe_buffer_movecursor(buf, saveiGap - buf->iGap);
}
