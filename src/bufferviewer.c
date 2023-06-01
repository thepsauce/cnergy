#include "cnergy.h"

// TODO: make the buffer viewer which will show the user all open (those inside of all_buffers)

#define MAXCOLUMN 2

static int row = 0;
static int column = 0;

int
bufferviewer_render(struct window *win)
{
	unsigned i = 0;
	const int maxCol1 = win->cols * 2 / 3;
	attrset(COLOR_PAIR(3));
	for(int y = win->line; i < n_buffers && y < win->line + win->lines - 1; i++, y++) {
		struct buffer *const buf = all_buffers[i];
		attrset(COLOR(3, column == 0 && (int) i == row ? 8 : 0));
		move(y, win->col);
		if(buf->file)
			addnstr(buf->file, MIN(maxCol1, (int) strlen(buf->file)));
		else
			addnstr("[new]", MIN(maxCol1, 5));
		move(y, win->col + maxCol1 + 1);
		attrset(COLOR(3, column == 1 && (int) i == row ? 8 : 0));
		printw("%zu", buf->nData);
	}
	attrset(win == focus_window ? COLOR(14, 8) : COLOR(6, 8));
	mvaddstr(win->line + win->lines - 1, win->col, " Buffer viewer ");
	attrset(win == focus_window ? COLOR(11, 8) : COLOR(3, 8));
	printw("%u", n_buffers);
	ersline(win->col + win->cols);
	if(win == focus_window) {
		focus_y = 0;
		focus_x = 0;
	}
	return 0;
}

bool
bufferviewer_bindcall(struct window *win, struct binding_call *bc, ssize_t param)
{
	switch(bc->type) {
	case BIND_CALL_MOVEVERT:
		if(!row && param < 0)
			return false;
		if(row + 1 >= (int) n_buffers && param > 0)
			return false;
		row = SAFE_ADD(row, param);
		row = MIN(row, (int) n_buffers - 1);
		row = MAX(row, 0);
		break;
	case BIND_CALL_MOVEHORZ:
		if(!column && param < 0)
			return false;
		if(column + 1 >= MAXCOLUMN && param > 0)
			return false;
		column = SAFE_ADD(column, param);
		column = MIN(column, MAXCOLUMN - 1);
		column = MAX(column, 0);
		break;
	case BIND_CALL_MOVECURSOR: {
		int cursor;
		const int maxCursor = n_buffers * MAXCOLUMN;
		cursor = column + row * MAXCOLUMN;
		if(!cursor && param < 0)
			return false;
		if(cursor >= maxCursor && param > 0)
			return false;
		cursor = SAFE_ADD(cursor, param);
		cursor = MIN(cursor, maxCursor - 1);
		cursor = MAX(cursor, 0);
		row = cursor / MAXCOLUMN;
		column = cursor % MAXCOLUMN;
		break;
	}
	default:
		return false;
	}
	return true;
}
