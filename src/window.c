#include "cnergy.h"

int
null_render(struct window *win)
{
	(void) win;
	return 1;
}

int
null_type(struct window *win, const char *str, U32 nStr)
{
	(void) win;
	(void) str;
	(void) nStr;
	return 1;
}

bool
null_bindcall(struct window *win, struct binding_call *bc, I32 param)
{
	(void) win;
	(void) bc;
	(void) param;
	return false;
}

// edit
int edit_render(struct window *win);
int edit_type(struct window *win, const char *str, U32 nStr);
bool edit_bindcall(struct window *win, struct binding_call *bc, I32 param);
// buffer viewer
int bufferviewer_render(struct window *win);
bool bufferviewer_bindcall(struct window *win, struct binding_call *bc, I32 param);
// file view 
int fileviewer_render(struct window *win);
int fileviewer_type(struct window *win, const char *str, U32 nStr);
bool fileviewer_bindcall(struct window *win, struct binding_call *bc, I32 param);

struct window_type window_types[] = {
	[WINDOW_EDIT] = { edit_render, edit_type, edit_bindcall },
	[WINDOW_BUFFERVIEWER] = { bufferviewer_render, null_type, bufferviewer_bindcall },
	[WINDOW_FILEVIEWER] = { fileviewer_render, fileviewer_type, fileviewer_bindcall },
};

// All windows in here are rendered
struct window **all_windows;
U32 n_windows;
// THE origin (used for layout)
struct window *first_window;
// Active window
struct window *focus_window;
int focus_y, focus_x;

struct window *
window_new(U32 type)
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
	win->type = type;
	win->bindMode = all_modes[type];
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
		focus_window = win->right ? win->right :
			win->below ? win->below :
			win->left ? win->left :
			win->above ? win->above : first_window;
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
window_dup(const struct window *win)
{
	struct window *dup;

	dup = window_new(0);
	if(!dup)
		return NULL;
	memcpy(dup, win, sizeof(*win));
	return dup;
}

void
window_copylayout(struct window *win, struct window *rep)
{
	if(first_window == win)
		first_window = rep;
	if(focus_window == win)
		focus_window = rep;
	rep->above = win->above;
	rep->below = win->below;
	rep->left = win->left;
	rep->right = win->right;
	if(win->above)
		win->above->below = rep;
	if(win->below)
		win->below->above = rep;
	if(win->left)
		win->left->right = rep;
	if(win->right)
		win->right->left = rep;
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
window_above(const struct window *win)
{
	const int yHit = win->line - 1;
	const int xHit = win == focus_window ? focus_x : win->col + win->cols / 2;
	return window_atpos(yHit, xHit);
}

struct window *
window_below(const struct window *win)
{
	const int yHit = win->line + win->lines;
	const int xHit = win == focus_window ? focus_x : win->col + win->cols / 2;
	return window_atpos(yHit, xHit);
}

struct window *
window_left(const struct window *win)
{
	const int yHit = win == focus_window ? focus_y : win->line + win->lines / 2;
	const int xHit = win->col - 1;
	return window_atpos(yHit, xHit);
}

struct window *
window_right(const struct window *win)
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
	struct window *a, *b, *l, *r;

	a = win->above;
	b = win->below;
	l = win->left;
	r = win->right;
	win->above = NULL;
	win->below = NULL;
	win->left = NULL;
	win->right = NULL;
	if(a)
		a->below = b;
	if(b)
		b->above = a;
	if(l)
		l->right = r;
	if(r)
		r->left = l;
	if(b && !a && l) {
		b->left = l;
		l->right = b;
	}
	if(r && !l && a) {
		r->above = a;
		a->below = r;
	}
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
	window_types[win->type].render(win);
	return 0;
}

