#include "cnergy.h"

/* all windows in here are rendered (if they are not dead) */
struct window *all_windows;
windowid_t n_windows;
/* connections between two windows */
struct window_link {
	windowid_t w1, w2;
} *window_links;
unsigned n_window_links;
/* active window */
windowid_t focus_window;
/* position of the terminal cursor (set by the focus window) */
int focus_y, focus_x;

windowid_t
window_new(window_type_t type)
{
	struct window *newWindows;
	windowid_t winid;
	struct window *win;

	for(winid = 0; winid < n_windows; winid++)
		if(all_windows[winid].flags.dead)
			break;
	if(winid == n_windows) {
		newWindows = dialog_realloc(all_windows, sizeof(*all_windows) * (n_windows + 1));
		if(!newWindows)
			return ID_NULL;
		all_windows = newWindows;
		n_windows++;
	}
	win = all_windows + winid;
	memset(win, 0, sizeof(*win));
	win->type = type;
	return winid;
}

int
window_close(windowid_t winid)
{
	window_delete(winid);
	return 0;
}

void
window_delete(windowid_t winid)
{
	all_windows[winid].flags.dead = true;
	/* Update the number of windows if needed */
	for(windowid_t wid = winid; wid < n_windows; wid++)
		if(!all_windows[wid].flags.dead)
			/* We can not cap the n_windows because there are still windows above that are alive */
			return;
	/* Find the first window that is alive */
	while(winid > 0) {
		winid--;
		if(!all_windows[winid].flags.dead)
			break;
	}
	n_windows = winid;
}

windowid_t
window_dup(windowid_t winid)
{
	windowid_t dupid;

	dupid = window_new(0);
	if(dupid == ID_NULL)
		return ID_NULL;
	memcpy(all_windows + dupid, all_windows + winid, sizeof(*all_windows));
	return dupid;
}

windowid_t
window_atpos(int y, int x)
{
	for(windowid_t wid = n_windows; wid > 0;) {
		struct window *const w = all_windows + --wid;
		if(w->flags.dead)
			continue;
		if(y >= w->line && y < w->line + w->lines &&
				x >= w->col && x < w->col + w->cols)
			return wid;
	}
	return ID_NULL;
}

windowid_t
window_above(const windowid_t winid)
{
	struct window *const win = all_windows + winid;
	const int yHit = win->line - 1;
	const int xHit = winid == focus_window ? focus_x : win->col + win->cols / 2;
	return window_atpos(yHit, xHit);
}

windowid_t
window_below(const windowid_t winid)
{
	struct window *const win = all_windows + winid;
	const int yHit = win->line + win->lines;
	const int xHit = winid == focus_window ? focus_x : win->col + win->cols / 2;
	return window_atpos(yHit, xHit);
}

windowid_t
window_left(const windowid_t winid)
{
	struct window *const win = all_windows + winid;
	const int yHit = winid == focus_window ? focus_y : win->line + win->lines / 2;
	const int xHit = win->col - 1;
	return window_atpos(yHit, xHit);
}

windowid_t
window_right(const windowid_t winid)
{
	struct window *const win = all_windows + winid;
	const int yHit = winid == focus_window ? focus_y : win->line + win->lines / 2;
	const int xHit = win->col + win->cols;
	return window_atpos(yHit, xHit);
}

void drawframe(const char *title, int y, int x, int ny, int nx);

void
window_displayall(int nextLine, int nextCol, int nextLines, int nextCols)
{
	/* find all windows that are layout origins and render them */
	for(windowid_t i = 0; i < n_windows; i++) {
		struct window *const w = all_windows + i;
		if(w->flags.dead)
			continue;

	}
}

