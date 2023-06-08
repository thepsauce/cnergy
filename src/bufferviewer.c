#include "cnergy.h"

#define MAXCOLUMN 2

static int row = 0;
static int column = 0;

int
bufferviewer_render(struct window *win)
{
	unsigned i = 0;
	const int itemselectedColor = all_settings[SET_COLOR_ITEMSELECTED];
	const int itemColor = all_settings[SET_COLOR_ITEM];
	const int statusbar1Color = win == focus_window ? all_settings[SET_COLOR_STATUSBAR1_FOCUS] : all_settings[SET_COLOR_STATUSBAR1];
	const int statusbar2Color = win == focus_window ? all_settings[SET_COLOR_STATUSBAR2_FOCUS] : all_settings[SET_COLOR_STATUSBAR2];
	const int maxCol1 = win->cols * 2 / 3;
	for(int y = win->line; i < n_buffers && y < win->line + win->lines - 1; i++, y++) {
		struct filecache *fc;
		struct buffer *const buf = all_buffers[i];
		fc = !buf->file ? NULL : fc_lock(buf->file);
		attrset(column == 0 && (int) i == row ? itemselectedColor : itemColor);
		move(y, win->col);
		if(fc)
			addnstr(fc->name, MIN(maxCol1, (int) strlen(fc->name)));
		if(!fc || (fc->flags & FC_VIRTUAL))
			addnstr("[new]", MIN(maxCol1, 5));
		move(y, win->col + maxCol1 + 1);
		attrset(column == 1 && (int) i == row ? itemselectedColor : itemColor);
		printw("%zu", buf->nData);
		if(fc)
			fc_unlock(fc);
	}
	attrset(statusbar1Color);
	mvaddstr(win->line + win->lines - 1, win->col, " Buffer viewer ");
	attrset(statusbar2Color);
	printw("%u", n_buffers);
	ersline(win->col + win->cols);
	if(win == focus_window) {
		focus_y = 0;
		focus_x = 0;
	}
	return 0;
}

bool
bufferviewer_bindcall(struct window *win, struct binding_call *bc, ptrdiff_t param, ptrdiff_t *pCached)
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
