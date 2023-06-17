#include "cnergy.h"

int
null_render(windowid_t winid)
{
	(void) winid;
	return 1;
}

int
null_type(windowid_t winid, const char *str, size_t nStr)
{
	(void) winid;
	(void) str;
	(void) nStr;
	return 1;
}

bool
null_bindcall(windowid_t winid, struct binding_call *bc, ptrdiff_t param, ptrdiff_t *pCached)
{
	(void) winid;
	(void) bc;
	(void) param;
	(void) pCached;
	return false;
}

// edit
int edit_render(windowid_t winid);
int edit_type(windowid_t winid, const char *str, size_t nStr);
bool edit_bindcall(windowid_t winid, struct binding_call *bc, ptrdiff_t param, ptrdiff_t *pCached);
// buffer viewer
int bufferviewer_render(windowid_t winid);
bool bufferviewer_bindcall(windowid_t winid, struct binding_call *bc, ptrdiff_t param, ptrdiff_t *pCached);
// file view
int fileviewer_render(windowid_t winid);
int fileviewer_type(windowid_t winid, const char *str, size_t nStr);
bool fileviewer_bindcall(windowid_t winid, struct binding_call *bc, ptrdiff_t param, ptrdiff_t *pCached);

struct window_type window_types[] = {
	[WINDOW_EDIT] = { edit_render, edit_type, edit_bindcall },
	[WINDOW_BUFFERVIEWER] = { bufferviewer_render, null_type, bufferviewer_bindcall },
	[WINDOW_FILEVIEWER] = { fileviewer_render, fileviewer_type, fileviewer_bindcall },
};

/* all windows in here are rendered */
struct window *all_windows;
unsigned n_windows;
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
	win->above = ID_NULL;
	win->below = ID_NULL;
	win->left = ID_NULL;
	win->right = ID_NULL;
	win->type = type;
	win->bindMode = all_modes[type];
	return winid;
}

int
window_close(windowid_t winid)
{
	if(winid == focus_window)
		focus_window = window_getbelow(winid) != ID_NULL ? window_getbelow(winid) :
			window_getright(winid) != ID_NULL ? window_getright(winid) :
			window_getleft(winid) != ID_NULL ? window_getleft(winid) :
			window_getabove(winid) != ID_NULL ? window_getabove(winid) : ID_NULL;
	window_detach(winid);
	window_delete(winid);
	return 0;
}

void
window_delete(windowid_t winid)
{
	all_windows[winid].flags.dead = true;
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

void
window_copylayout(windowid_t destid, windowid_t srcid)
{
	if(focus_window == srcid)
		focus_window = destid;
	window_setabove(destid, window_getabove(srcid));
	window_setbelow(destid, window_getbelow(srcid));
	window_setleft(destid, window_getleft(srcid));
	window_setright(destid, window_getright(srcid));
	if(window_getabove(srcid) != ID_NULL)
		window_setbelow(window_getabove(srcid), destid);
	if(window_getbelow(srcid) != ID_NULL)
		window_setabove(window_getbelow(srcid), destid);
	if(window_getleft(srcid) != ID_NULL)
		window_setright(window_getleft(srcid), destid);
	if(window_getright(srcid) != ID_NULL)
		window_setleft(window_getright(srcid), destid);
}

windowid_t 
window_atpos(int y, int x)
{
	for(windowid_t i = n_windows; i > 0;) {
		struct window *const w = all_windows + --i;
		if(w->flags.dead)
			continue;
		if(y >= w->line && y < w->line + w->lines &&
				x >= w->col && x < w->col + w->cols)
			return i;
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

void
window_attach(windowid_t toid, windowid_t winid, int pos)
{
	switch(pos) {
	case ATT_WINDOW_VERTICAL: goto vertical_attach;
	case ATT_WINDOW_HORIZONTAL: goto horizontal_attach;
	}
	/* Find shortest list */
	const unsigned
		nVert = window_countbelow(window_getmostabove(winid)),
		nHorz = window_countright(window_getmostleft(winid));
	if(nVert > nHorz)
		goto horizontal_attach;
vertical_attach:
	if(window_getbelow(toid) != ID_NULL)
		window_setabove(window_getbelow(toid), winid);
	window_setbelow(winid, window_getbelow(toid));
	window_setabove(winid, toid);
	window_setleft(winid, ID_NULL);
	window_setright(winid, ID_NULL);
	window_setbelow(toid, winid);
	return;
horizontal_attach:
	if(window_getright(toid) != ID_NULL)
		window_setleft(window_getright(toid), winid);
	window_setabove(winid, ID_NULL);
	window_setbelow(winid, ID_NULL);
	window_setright(winid, window_getright(toid));
	window_setleft(winid, toid);
	window_setright(toid, winid);
}

void
window_detach(windowid_t winid)
{
	windowid_t a, b, l, r;

	a = window_getabove(winid);
	b = window_getbelow(winid);
	l = window_getleft(winid);
	r = window_getright(winid);
	window_setabove(winid, ID_NULL);
	window_setbelow(winid, ID_NULL);
	window_setleft(winid, ID_NULL);
	window_setright(winid, ID_NULL);
	if(a != ID_NULL)
		window_setbelow(a, b);
	if(b != ID_NULL)
		window_setabove(b, a);
	if(l != ID_NULL)
		window_setright(l, r);
	if(r != ID_NULL)
		window_setleft(r, l);
	if(a == ID_NULL && l == ID_NULL) {
		if(b != ID_NULL && r != ID_NULL) {
			b = window_getmostright(b);
			window_setright(b, r);
			window_setleft(r, b);
			/*// Second option:
			r = window_getmostbelow(r);
			window_setbelow(r, b);
			window_setabove(b, r);*/
		}
	} else if(a != ID_NULL && r != ID_NULL) {
		window_setabove(r, a);
		window_setbelow(r, b);
		window_setbelow(a, r);
	} else if(l != ID_NULL && b != ID_NULL) {
		window_setleft(b, l);
		window_setright(b, r);
		window_setright(l, b);
	}
}

void
window_layout(windowid_t winid)
{
	windowid_t wid;
	unsigned nBelow = 1, nRight = 1;
	int linesPer, colsPer, linesRemainder, colsRemainder;
	int nextLine, nextCol;
	int line, col, lines, cols;

	window_getposition(winid, line, col, lines, cols);
	nBelow = window_getabove(winid) == ID_NULL ? window_countbelow(winid) : 1;
	nRight = window_getleft(winid) == ID_NULL ? window_countright(winid) : 1;
	linesPer = lines / nBelow;
	colsPer = cols / nRight;
	linesRemainder = lines % nBelow;
	colsRemainder = cols % nRight;

	window_setsize(winid, linesPer, colsPer);
	nextLine = line + linesPer;
	nextCol = col + colsPer;
	/* if there is a window left, it will not handle the right windows, we don't want to handle them twice */
	if(window_getleft(winid) == ID_NULL)
		for(wid = winid; (wid = window_getright(wid)) != ID_NULL; ) {
			int cols;

			cols = colsPer;
			if(colsRemainder) {
				colsRemainder--;
				cols++;
			}
			window_setposition(wid, line, nextCol, lines, cols);
			nextCol += cols;
			window_layout(wid);
		}
	/* if there is a window above, it will not handle the below windows, we don't want to handle them twice */
	if(window_getabove(winid) == ID_NULL)
		for(wid = winid; (wid = window_getbelow(wid)) != ID_NULL; ) {
			int lines;

			lines = linesPer;
			if(linesRemainder) {
				linesRemainder--;
				lines++;
			}
			/* note that we use linesPer and not lines here */
			window_setposition(wid, nextLine, col, lines, colsPer);
			nextLine += lines;
			window_layout(wid);
		}
}

int
window_render(windowid_t winid)
{
	/* don't need to render empty windows */
	if(window_getarea(winid) == 0)
		return 1;
	window_types[window_gettype(winid)].render(winid);
	if(window_getbelow(winid) != ID_NULL)
		window_render(window_getbelow(winid));
	if(window_getright(winid) != ID_NULL)
		window_render(window_getright(winid));
	return 0;
}

