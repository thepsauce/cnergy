#include "cnergy.h"

#define MAXCOLUMN 2

static int row = 0;
static int column = 0;

static inline int
bufferviewer_render(windowid_t winid)
{
	bufferid_t bid = 0;
	struct window *const win = all_windows + winid;
	const int itemselectedColor = all_settings[SET_COLOR_ITEMSELECTED];
	const int itemColor = all_settings[SET_COLOR_ITEM];
	const int statusbar1Color = winid == focus_window ? all_settings[SET_COLOR_STATUSBAR1_FOCUS] : all_settings[SET_COLOR_STATUSBAR1];
	const int statusbar2Color = winid == focus_window ? all_settings[SET_COLOR_STATUSBAR2_FOCUS] : all_settings[SET_COLOR_STATUSBAR2];
	const int maxCol1 = win->cols * 2 / 3;
	for(int y = win->line; bid < n_buffers && y < win->line + win->lines - 1; bid++, y++) {
		struct filecache *fc;
		struct buffer *const buf = all_buffers + bid;
		fc = !buf->file ? NULL : fc_lock(buf->file);
		attrset(column == 0 && (int) bid == row ? itemselectedColor : itemColor);
		move(y, win->col);
		if(fc)
			addnstr(fc->name, MIN(maxCol1, (int) strlen(fc->name)));
		if(!fc || (fc->flags & FC_VIRTUAL))
			addnstr("[new]", MIN(maxCol1, 5));
		move(y, win->col + maxCol1 + 1);
		attrset(column == 1 && (int) bid == row ? itemselectedColor : itemColor);
		printw("%zu", buf->nData);
		if(fc)
			fc_unlock(fc);
	}
	attrset(statusbar1Color);
	mvaddstr(win->line + win->lines - 1, win->col, " Buffer viewer ");
	attrset(statusbar2Color);
	printw("%u", n_buffers);
	ersline(win->col + win->cols);
	if(winid == focus_window) {
		focus_y = 0;
		focus_x = 0;
	}
	return 0;
}

bool
bufferviewer_event(windowid_t winid, struct event *ev)
{
	(void) winid;
	switch(ev->type) {
	case EVENT_MOVEVERT:
		if(!row && ev->amount < 0)
			return false;
		if(row + 1 >= (int) n_buffers && ev->amount> 0)
			return false;
		row = SAFE_ADD(row, ev->amount);
		row = MIN(row, (int) n_buffers - 1);
		row = MAX(row, 0);
		break;
	case EVENT_MOVEHORZ:
		if(!column && ev->amount < 0)
			return false;
		if(column + 1 >= MAXCOLUMN && ev->amount > 0)
			return false;
		column = SAFE_ADD(column, ev->amount);
		column = MIN(column, MAXCOLUMN - 1);
		column = MAX(column, 0);
		break;
	case EVENT_MOVECURSOR: {
		int cursor;
		const int maxCursor = n_buffers * MAXCOLUMN;
		cursor = column + row * MAXCOLUMN;
		if(!cursor && ev->amount < 0)
			return false;
		if(cursor >= maxCursor && ev->amount > 0)
			return false;
		cursor = SAFE_ADD(cursor, ev->amount);
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
