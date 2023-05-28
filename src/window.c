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
window_new(void)
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

int
window_render(struct window *win)
{
	// don't need to render empty windows
	if(win->lines <= 0 || win->cols <= 0)
		return 1;
	switch(win->type) {
	case WINDOW_EDIT: edit_render(win); break;
	default:
		// this window type doesn't support rendering
		return 1;
	}
	return 0;
}
