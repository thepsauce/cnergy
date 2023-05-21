#include "cnergy.h"

struct window *first_window;
struct window *focus_window;

// TODO: move utf8_strlen to a better place
static U32
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

void
window_layout(struct window *win)
{
	struct window *w;
	int i;
	U32 nRight = 1, nBelow = 1;
	const int line = win->line;
	const int col = win->col;
	const int lines = win->lines;
	const int cols = win->cols;
	for(w = win->right; w; w = w->right)
		nRight++;
	for(w = win->below; w; w = w->below)
		nBelow++;
	const int linesPer = win->lines / nBelow;
	const int colsPer = win->cols / nRight;
	win->cols = colsPer;
	win->lines = linesPer;
	// TODO: Make last window the maximum minus the taken size
	for(i = 1, w = win->right; w; w = w->right, i++) {
		w->line = line;
		w->col = col + colsPer * i;
		w->lines = lines;
		w->cols = colsPer;
		struct window *const right = w->right;
		w->right = NULL;
		window_layout(w);
		w->right = right;
	}
	for(i = 1, w = win->below; w; w = w->below, i++) {
		w->line = line + linesPer * i;
		w->col = col;
		w->lines = linesPer;
		w->cols = cols;
		struct window *const below = w->below;
		w->below = NULL;
		window_layout(w);
		w->below = below;
	}
}

// TODO: This functino needs cleanup
// there's also a problem when a line is exceeded horizontally
void
window_render(struct window *win)
{
	const U32 nLineNumbers = 4; // TODO: dynamically set this
	U32 cursorLine, cursorCol;
	U32 line, col;
	U32 sMin = 1, sMax = 0;
	struct c_state state;
	struct buffer *const buf = win->buffers + win->iBuffer;
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

	const U32 cols = win->cols - nLineNumbers;
	const U32 lines = win->lines - 1;
    if(cursorLine < win->vScroll)
        win->vScroll = cursorLine;
    if(cursorLine >= lines + win->vScroll)
        win->vScroll = cursorLine - (lines - 1);

	if(mode_has(FBIND_MODE_SELECTION)) {
		if(win->selection > saveiGap) {
			sMin = saveiGap;
			sMax = win->selection;
		} else {
			sMin = win->selection;
			sMax = saveiGap;
		}
	}
	memset(&state, 0, sizeof(state));
	state.data = buf->data;
	state.nData = buf->nData;
	line = 0;
	col = 0;
	if(!win->vScroll) {
		attrset(COLOR(242, 236));
		mvprintw(win->line, win->col, "%*d ", nLineNumbers - 1, 1);
	}
	for(U32 i = 0; i < buf->nData;) {
		state.index = i;
		const int r = c_feed(&state);
		attrset(state.attr);
		if(state.conceal) {
			const char *const conceal = state.conceal;
			state.conceal = NULL;
			if(line != cursorLine) {
				i = state.index + 1;
				if(line >= win->vScroll && line < win->vScroll + lines)
					mvaddstr(line + win->line - win->vScroll, col + win->col - win->hScroll + nLineNumbers, conceal);
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
				if(line >= win->vScroll && line < win->vScroll + lines) {
					attrset(COLOR(242, 236));
					mvprintw(line + win->line - win->vScroll, win->col, "%*d ", nLineNumbers - 1, line + 1);
					attrset(state.attr);
				}
			} else if(ch < 32) {
				if(line >= win->vScroll && line < win->vScroll + lines) {
					attrset(COLOR_PAIR(1));
					mvaddch(line + win->line - win->vScroll, col + win->col - win->hScroll + nLineNumbers, '^');
					mvaddch(line + win->line - win->vScroll, col + win->col - win->hScroll + nLineNumbers, ch + 64);
					attrset(state.attr);
				}
				col += 2;
			} else {
				if(line >= win->vScroll && line < win->vScroll + lines)
					mvaddch(line + win->line - win->vScroll, col + win->col - win->hScroll + nLineNumbers, ch);
				col++;
			}
			if(i >= sMin && i <= sMax)
				attroff(A_REVERSE);
		}
	}
	attrset(COLOR(242, 236));
	if(!line)
		line++;
	for(; line < win->vScroll + lines; line++)
		mvprintw(line + win->line - win->vScroll, win->col, "  ~ ", nLineNumbers - 1, line + 1);
	// TODO: render status bar
	mvprintw(line + win->line - win->vScroll, win->col, "status: %u, %u, %u, %u, %u, %u %c%c", win->vScroll, win->hScroll, win->line, win->col, win->lines, win->cols, win->right ? 'R' : 0, win->below ? 'B' : 0);
	if(win == focus_window)
		move(cursorLine - win->vScroll, cursorCol - win->hScroll + nLineNumbers);
	// TODO: fix cursor rendering for multiple windows (conflict)
	unsafe_buffer_movecursor(buf, saveiGap - buf->iGap);

	if(win->right)
		window_render(win->right);
	if(win->below)
		window_render(win->below);
}
