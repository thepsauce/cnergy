#ifndef INCLUDED_WINDOW_H
#define INCLUDED_WINDOW_H

// window.c
/**
 * Windows are stored in a global array and the only way to hide them is to set their area to 0.
 * Windows can be layered but only windows that are attached to the first_window (origin of the layout)
 * are considered for the layouting. This fact can be used for popup windows or similar.
 */

/** TODO: State saving technique for buffer windows
 * This technique will be used to avoid re-rendering of the buffer every time.
 * It will store a list of states and their position and when re-rendering, it will
 * start from the closest previous state that is left to the caret.
 * When the user inserts a character, .... (TODO: think about this; lookahead makes this complicated)
 */

typedef enum {
	WINDOW_ALL,
	WINDOW_EDIT,
	WINDOW_BUFFERVIEWER,
	WINDOW_FILEVIEWER,
	// TODO: add more
	WINDOW_MAX,
} window_type_t;

#define FILEVIEW_SORT_ALPHA 0
#define FILEVIEW_SORT_MODTIME 1
#define FILEVIEW_SORT_CHGTIME 2

typedef struct {
	/* Marks this window as dead, meaning it can be recycled */
	unsigned dead: 1;
	union {
		// WINDOW_EDIT
		// edit.c
		struct {

		};
		// WINDOW_FILEVIEWER
		// fileview.c
		struct {
			unsigned hidden: 1;
			unsigned sort: 2;
			unsigned sortType: 1;
			unsigned sortReverse: 1;
		};
	};
} window_flags_t;

struct state;
struct binding_mode;
struct binding_call;

struct window {
	window_flags_t flags;
	window_type_t type; // defined in cnergy.h
	// position of the window
	int line, col;
	// size of the window
	int lines, cols;
	union {
		// WINDOW_EDIT
		struct {
			// text buffer
			bufferid_t buffer;
			// scrolling values of the last render
			int vScroll, hScroll;
			// selection cursor
			size_t selection;
		};
		// WINDOW_FILEVIEWER
		struct {
			fileid_t base;
			int selected;
			int scroll;
			int maxSelected;
			// input box
			int cursor;
			int curScroll;
			char path[PATH_MAX];
		};
		// TODO: add more...
	};
};

extern struct window *all_windows;
extern windowid_t n_windows;
extern windowid_t focus_window;
extern int focus_y, focus_x;

/** Helpful macros */
#define window_get(g, wid) all_windows[wid].g
#define window_getflags(wid) window_get(flags, wid)
#define window_gettype(wid) window_get(type, wid)
/** Getter for attached window */
#define window_getabove(wid) window_get(above, wid)
#define window_getbelow(wid) window_get(below, wid)
#define window_getleft(wid) window_get(left, wid)
#define window_getright(wid) window_get(right, wid)
/** Setter for attached window */
#define window_setabove(wid, a) all_windows[wid].above = (a)
#define window_setbelow(wid, b) all_windows[wid].below = (b)
#define window_setleft(wid, l) all_windows[wid].left = (l)
#define window_setright(wid, r) all_windows[wid].right = (r)
/** Get position and size of this window */
#define window_getposition(wid, l, c, ls, cs) ({ \
	struct window *_w = &all_windows[wid]; \
	l = _w->line; \
	c = _w->col; \
	ls = _w->lines; \
	cs = _w->cols; \
	0; \
})
/** Set position and size of this window */
#define window_setposition(wid, l, c, ls, cs) ({ \
	struct window *_w = &all_windows[wid]; \
	_w->line = (l); \
	_w->col = (c); \
	_w->lines = (ls); \
	_w->cols = (cs); \
	0; \
})
#define window_setsize(wid, ls, cs) ({ \
	struct window *_w = &all_windows[wid]; \
	_w->lines = (ls); \
	_w->cols = (cs); \
	0; \
})
#define window_getarea(wid) ({ \
	struct window *_w = &all_windows[wid]; \
	_w->cols * _w->lines; \
})
/** Returns the window that is most {dir} of given window */
#define window_getmost(dir, wid) ({ \
	windowid_t _wid = (wid); \
	for(; _wid != ID_NULL; _wid = window_get(dir, _wid)); \
	_wid; \
})
#define window_getmostabove(wid) window_getmost(above, wid)
#define window_getmostbelow(wid) window_getmost(below, wid)
#define window_getmostleft(wid) window_getmost(left, wid)
#define window_getmostright(wid) window_getmost(right, wid)
/** Counts the number of windows in a given direction, always includes the window itself */
#define window_count(dir, wid) ({ \
	windowid_t _wid = (wid); \
	unsigned _c = 0; \
	for(; _wid != ID_NULL; _wid = window_get(dir, _wid)) \
		_c++; \
	_c; \
})
#define window_countabove(wid) window_count(above, wid)
#define window_countbelow(wid) window_count(below, wid)
#define window_countleft(wid) window_count(left, wid)
#define window_countright(wid) window_count(right, wid)

/** Relayouts all windows and renders them */
void window_displayall(int y, int x, int lines, int cols);
/** Create a new window and add it to the window list */
windowid_t window_new(window_type_t type);
/** Safe function that detaches, deletes the window and changes focus-/first window if needed */
int window_close(windowid_t winid);
/** Delete this window from memory */
/** Note: You must detach the window or copy the layout to another window before deleting it, otherwise it's undefined behavior */
void window_delete(windowid_t winid);
/** The destination window will take place of the source window; this also swaps the focus */
void window_copylayout(windowid_t destid, windowid_t srcid);
/** Creates a duplicate of the given window */
windowid_t window_dup(windowid_t winid);
/** Creates a duplicate of the given window */
windowid_t window_dup(windowid_t winid);
/** Moves this window to the foreground, it will be drawn last */
int window_setforeground(windowid_t winid);
/** This gets the window at given position */
windowid_t window_atpos(int y, int x);
/** These four functions have additional handling and may return non null values even though the internal struct value is null */
windowid_t window_above(windowid_t winid);
windowid_t window_below(windowid_t winid);
windowid_t window_left(windowid_t winid);
windowid_t window_right(windowid_t winid);
/** Attach a window to another one */
/** pos can be one of the following values: */
/** ATT_WINDOW_UNSPECIFIED: The function decides where the window should best go */
/** ATT_WINDOW_VERTICAL: The window should go below */
/** ATT_WINDOW_HORIZONTAL: The window should go right */
enum {
	ATT_WINDOW_UNSPECIFIED,
	ATT_WINDOW_VERTICAL,
	ATT_WINDOW_HORIZONTAL,
};
void window_attach(windowid_t toid, windowid_t winid, int pos);
/** Remove all connections to any other windows */
void window_detach(windowid_t winid);
/** Relayout this window and all right/down connected windows */
void window_layout(windowid_t winid);

/* Additional functions for specific window types */
// edit.c
windowid_t edit_new(bufferid_t bufid);

#endif
