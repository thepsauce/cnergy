#ifndef INCLUDED_EDITOR_H
#define INCLUDED_EDITOR_H

#include "base.h"
#include "window.h"
#include "bind.h"
#include "parse.h"

#include <curses.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// TODO: COLOR is a temporary macro, it should get removed soon
#define COLOR(bg, fg) COLOR_PAIR(1 + ((bg) | ((fg) * 16)))
#define ersline(max) ({ \
	__auto_type _m = (max); \
	printw("%*s", MAX((int) _m - getcurx(stdscr), 0), ""); \
})

/* Sorted list */
// sortedlist.c
/**
 * This list is sorted at all times and can use that to
 * allow for faster access times using binary search at the price
 * of slightly slower insertion times.
 */

struct sortedlist {
	struct sortedlist_entry {
		char *word;
		// the parameter can be anything you want
		void *param;
	} *entries;
	size_t nEntries;
};

/** Adds and entry to the sorted list, it will not add the element if it already exists (no duplicates) */
int sortedlist_add(struct sortedlist *s, const char *word, size_t nWord, void *param);
int sortedlist_remove(struct sortedlist *s, const char *word, size_t nWord);
bool sortedlist_exists(struct sortedlist *s, const char *word, size_t nWord, void **pParam);

/* File info */
// filesystem_unix.c

typedef enum {
	FI_TYPE_NULL,
	FI_TYPE_DIR,
	FI_TYPE_REG,
	FI_TYPE_EXEC,
	FI_TYPE_OTHER,
} file_type_t;

struct file_info {
	file_type_t type;
	// inode change
	time_t chgTime;
	// last modification time
	time_t modTime;
	// last access time
	time_t accTime;
};

int getfileinfo(struct file_info *fi, const char *path);

/* UTF8 */
// utf8.c

// Returns the width this would visually take up
// Note: Newlines are interpreted as ^J
int utf8_widthnstr(const char *str, size_t nStr);
int utf8_widthstr(const char *str);
// Returns the number of columns this character takes up
// Note: nStr is not allowed to be 0, it's undefined behavior
int utf8_width(const char *utf8, size_t nStr, int tabRef);
// Returns the length of bytes of a single character
unsigned utf8_len(const char *str, size_t nStr);
// Checks if the given character is valid utf8
// Note: nStr is not allowed to be 0, might cause segfault
bool utf8_valid(const char *utf8, size_t nStr);
/** Convert byte distance to char distance */
ssize_t utf8_cnvdist(const char *str, size_t nStr, size_t index, ssize_t distance);

/* Dialog */
// dialog.c
/**
 * Dialogs temporarily block all user input to show a message and wait for a user's reaction
 */

const char *choosefile(void);
int messagebox(const char *title, const char *msg, ...);
// These functions show a dialog to the user if the allocation failed
// The user may then retry allocating or cancel
void *dialog_alloc(size_t sz);
void *dialog_realloc(void *ptr, size_t newSz);

/* Clipboard */
// clip_x11.c
// clip_none.c

// X11 support YES
// MacOS support NO

// This function copies the given text to the clipboard
// Returns 0 if the function succeeded
int clipboard_copy(const char *text, size_t nText);
// This function gets the clipboard text and stores it inside text,
// the received pointer must be freed by the caller
// Returns 0 if the function succeeded
int clipboard_paste(char **text);

/* Gap buffer */
// buffer.c
/**
 * The gap buffer is a dynamic array that has a gap which is part of the data, this makes for very efficient
 * insertion and especially deletion operations but slightly slows down caret movement
 */

enum {
	EVENT_INSERT,
	EVENT_DELETE,
	EVENT_REPLACE,
};

// TODO: maybe make a global event queue that can be used from anywhere and is also stored in a file (restore session on crash)
struct event {
	unsigned flags;
	unsigned type;
	size_t iGap;
	unsigned vct, prevVct;
	char *ins, *del;
	size_t nIns, nDel;
};

// Default size of the buffer gap
#define BUFFER_GAP_SIZE 64

struct buffer {
	char *data;
	size_t nData;
	size_t iGap;
	size_t nGap;
	unsigned vct;
	char *file;
	time_t fileTime;
	unsigned saveEvent;
	struct event *events;
	unsigned nEvents;
	unsigned iEvent;
};

extern struct buffer **all_buffers;
extern unsigned n_buffers;

// Create a new buffer based on the given file, it may be null, then an empty buffer is created
// This also adds it to the internal buffer list
struct buffer *buffer_new(const char *file);
// Free all resources associated to this buffer
// Make sure to delete all windows associated to this buffer before calling this function
void buffer_free(struct buffer *buf);
// Saves the buffer to its file; if it has none, the user will be asked to choose a file
int buffer_save(struct buffer *buf);
// Returns the moved amount
ssize_t unsafe_buffer_movecursor(struct buffer *buf, ssize_t distance);
ssize_t buffer_movecursor(struct buffer *buf, ssize_t distance);
// Moves the cursor horizontally
// Returns the moved amount
ssize_t buffer_movehorz(struct buffer *buf, ssize_t distance);
// Move the cursor up or down by the specified number of lines
// Returns the moved amount
ssize_t buffer_movevert(struct buffer *buf, ssize_t distance);
// Returns the number of characters inserted
size_t buffer_insert(struct buffer *buf, const char *str, size_t nStr);
// Returns the number of characters inserted
size_t buffer_insert_file(struct buffer *buf, FILE *fp);
// Returns the amount that was deleted
ssize_t buffer_delete(struct buffer *buf, ssize_t amount);
// Returns the amount of lines that were deleted
ssize_t buffer_deleteline(struct buffer *buf, ssize_t amount);
// Returns the amount of events undone
int buffer_undo(struct buffer *buf);
// Returns the amount of events redone
int buffer_redo(struct buffer *buf);
// Returns line index at cursor
int buffer_line(const struct buffer *buf);
// Returns column index at cursor
int buffer_col(const struct buffer *buf);
// Returns the length of the line
size_t buffer_getline(const struct buffer *buf, int line, char *dest, size_t maxDest);

/* Syntax highlighting */
// state.c
// syntax_c/
// syntax_cnergy/
/** TODO:
 * All this is just a temporary solution to the quite complex problem of fast, efficient and rich syntax highlighting.
 * The syntax will in the future not be defined in c files but rather in .cng files where the user can also add custom syntax
 */

struct state {
	// the window we draw inside of
	struct window *win;
	// the stack can be used to repurpose states (states within states)
	unsigned stateStack[32];
	unsigned iStack;
	// current active state
	unsigned state;
	// words to highlight, the word data is placed into the const_alloc space
	// this list is alphabetically sorted
	// the words array should be freed when finished
	struct sortedlist words;
	// paranthesis matching
	struct state_paran {
		int line, col;
		int ch;
	} *parans, *misparans;
	unsigned nParans, nMisparans;
	// set if the caret is on an opening paran
	struct state_paran highlightparan;
	// conceal can be set to non null to replace characters visually with other characters
	// conceal is only used when the caret is not on the same line as the concealment
	// conceal is not allowed to replace new lines, nor contain new lines (undefined behavior)
	const char *conceal;
	// attr is used when a state returns to print the character between the start and end of the state
	int attr;
	// this is the raw buffer data, index specifies the current index within the data
	char *data;
	size_t nData;
	size_t index;
	// cursor index for cursor assertions
	size_t cursor;
	// visual position of the cursor (relative to the scrolled window origin) (can be used to highlight something later and not instantly)
	int line, col;
	int minLine, maxLine, minCol, maxCol;
};

// Skip spaces and tabs
// Note: Put this at the beginning of a state function
#define STATE_SKIPSPACE(s) if(isblank(s->data[s->index])) return 0
// Push a state to the state stack
int state_push(struct state *s, unsigned state);
// Pop the state from the state stack into the current state
int state_pop(struct state *s);
// Add the current char as an opening paranthesis
int state_addparan(struct state *s);
// Add the current char as a closing paranthesis
// This function will find matching paranthesis
int state_addcounterparan(struct state *s, int counter);
// Acquire the next syntax element
int state_continue(struct state *s);
// Draw a char to the window
// Note: ONLY call this inside of c_cleanup
void state_addchar(struct state *s, int line, int col, char ch, int attr);
// Free all resources associated to this state
int state_cleanup(struct state *s);

// C syntax
extern int (*c_states[])(struct state *s);
// cnergy syntax
extern int (*cnergy_states[])(struct state *s);

#endif
