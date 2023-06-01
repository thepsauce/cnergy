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
	// no flags yet
	FWINDOW_______,
} window_flags_t;

struct state;

struct window {
	window_flags_t flags;
	window_type_t type;
	// the binding mode this window is in
	struct binding_mode *bindMode;
	// position of the window
	int line, col;
	// size of the window
	int lines, cols;
	// scrolling values of the last render
	int vScroll, hScroll;
	// selection cursor
	size_t selection;
	// that window is above/below this window
	struct window *above, *below;
	// that window is left/right of this window
	struct window *left, *right;
	union {
		struct {
			// text buffer
			struct buffer *buffer;
			// syntax states
			int (**states)(struct state *s);
		};
		// TODO: add more...
	};
};

extern struct window_type {
	int (*render)(struct window *win);
	int (*type)(struct window *win, const char *str, size_t nStr);
	bool (*bindcall)(struct window *win, struct binding_call *bc, ssize_t param);
} window_types[];
extern struct window **all_windows;
extern unsigned n_windows;
extern struct window *first_window;
extern struct window *focus_window;
extern int focus_y, focus_x;

/** Create a new window and add it to the window list */
struct window *window_new(window_type_t type);
/** Safe function that detaches, deletes the window and changes focus-/first window if needed */
int window_close(struct window *win);
/** Delete this window from memory */
/** Note: You must detach the window or copy the layout to another window before deleting it, otherwise it's undefined behavior */
void window_delete(struct window *win);
/** The second window will take the place of the first one; this also swaps the focus */
void window_copylayout(struct window *win, struct window *rep);
/** Creates a duplicate of the given window */
struct window *window_dup(const struct window *win);
/** Creates a duplicate of the given window */
struct window *window_dup(const struct window *win);
/** This gets the window at given position */
struct window *window_atpos(int y, int x);
/** These four functions have additional handling and may return non null values even though the internal struct value is null */
struct window *window_above(const struct window *win);
struct window *window_below(const struct window *win);
struct window *window_left(const struct window *win);
struct window *window_right(const struct window *win);
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
void window_attach(struct window *to, struct window *win, int pos);
/** Remove all connections to any other windows */
void window_detach(struct window *win);
void window_layout(struct window *win);
/** Return values of 1 mean that the window wasn't rendered */
int window_render(struct window *win);

/* Additional functions for specific window types */
// edit.c
int (**edit_statesfromfiletype(const char *file))(struct state *s);
struct window *edit_new(struct buffer *buf, int (**states)(struct state *s));

#endif
