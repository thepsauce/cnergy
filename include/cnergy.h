#ifndef INCLUDED_EDITOR_H
#define INCLUDED_EDITOR_H

#include "base.h"
enum {
	WINDOW_ALL,
	WINDOW_EDIT,
	WINDOW_BUFFERVIEWER,
	WINDOW_FILEVIEWER,
	// TODO: add more
	WINDOW_MAX,
};
#include "bind.h"
#include "window.h"
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

// Note: Implemented in src/dialog.c
// These functions show a dialog to the user if the allocation failed
// The user may then retry allocating
void *safe_alloc(U32 sz);
void *safe_realloc(void *ptr, U32 newSz);

/* File info */

#define FI_TYPE_DIR 1
#define FI_TYPE_REG 2
#define FI_TYPE_EXEC 3
#define FI_TYPE_OTHER 4

struct file_info {
	U32 type;
	// inode change
	time_t chgTime;
	// last modification time
	time_t modTime;
	// last access time
	time_t accTime;
};

int getfileinfo(struct file_info *fi, const char *path);

/* UTF8 */

// Returns the width this would visually take up
// Note: Newlines are interpreted as ^J
U32 utf8_widthnstr(const char *str, U32 nStr);
U32 utf8_widthstr(const char *str);
// Returns the number of columns this character takes up
// Note: nStr is not allowed to be 0, it's undefined behavior
int utf8_width(const char *utf8, U32 nStr, U32 tabRef);
// Returns the length of bytes of a single character
int utf8_len(const char *str, U32 nStr);
// Checks if the given character is valid utf8
// Note: nStr is not allowed to be 0, might cause segfault
bool utf8_valid(const char *utf8, U32 nStr);

/* Dialog */
const char *choosefile(void);
int messagebox(const char *title, const char *msg, ...);
int getfiletime(const char *file, time_t *pTime);

/* Clipboard */

// X11 support YES
// Windows support NO
// MacOS NO

// This function copies the given text to the clipboard
// Returns 0 if the function succeeded
int clipboard_copy(const char *text, U32 nText);
// This function gets the clipboard text and stores it inside text,
// the received pointer must be freed by the caller
// Returns 0 if the function succeeded
int clipboard_paste(char **text);

/* Gap buffer */

enum {
	EVENT_INSERT,
	EVENT_DELETE,
	EVENT_REPLACE,
};

struct event {
	U16 flags;
	U16 type;
	U32 iGap;
	U32 vct, prevVct;
	char *ins, *del;
	U32 nIns, nDel;
};

// Default size of the buffer gap
#define BUFFER_GAP_SIZE 64

struct buffer {
	char *data;
	U32 nData;
	U32 iGap;
	U32 nGap;
	U32 vct;
	char *file;
	time_t fileTime;
	U32 saveEvent;
	struct event *events;
	U32 nEvents;
	U32 iEvent;
};

extern struct buffer **all_buffers;
extern U32 n_buffers;

// Create a new buffer based on the given file, it may be null, then an empty buffer is created
// This also adds it to the internal buffer list
struct buffer *buffer_new(const char *file);
// Free all resources associated to this buffer
// Make sure to delete all windows associated to this buffer before calling this function
void buffer_free(struct buffer *buf);
// Saves the buffer to its file; if it has none, the user will be asked to choose a file
int buffer_save(struct buffer *buf);
// Returns the moved amount
I32 unsafe_buffer_movecursor(struct buffer *buf, I32 distance);
I32 buffer_movecursor(struct buffer *buf, I32 distance);
// Moves the cursor horizontally
// Returns the moved amount
I32 buffer_movehorz(struct buffer *buf, I32 distance);
// Move the cursor up or down by the specified number of lines
// Returns the moved amount
I32 buffer_movevert(struct buffer *buf, I32 distance);
// Returns the number of characters inserted
U32 buffer_insert(struct buffer *buf, const char *str, U32 nStr);
// Returns the number of characters inserted
U32 buffer_insert_file(struct buffer *buf, FILE *fp);
// Returns the amount that was deleted
int buffer_delete(struct buffer *buf, I32 amount);
// Returns the amount of lines that were deleted
I32 buffer_deleteline(struct buffer *buf, I32 amount);
// Returns the amount of events undone
int buffer_undo(struct buffer *buf);
// Returns the amount of events redone
int buffer_redo(struct buffer *buf);
// Returns line index at cursor
U32 buffer_line(const struct buffer *buf);
// Returns column index at cursor
U32 buffer_col(const struct buffer *buf);
// Returns the length of the line
U32 buffer_getline(const struct buffer *buf, U32 line, char *dest, U32 maxDest);

/* Syntax highlighting */

struct state {
	// the window we draw inside of
	struct window *win;
	// the stack can be used to repurpose states (states within states)
	U32 stateStack[32];
	U32 iStack;
	// current active state
	U32 state;
	// words to highlight, the word data is placed into the const_alloc space
	// this list is alphabetically sorted
	// the words array should be freed when finished
	struct state_word {
		int attr;
		char *word;
	} *words;
	U32 nWords;
	// paranthesis matching
	struct state_paran {
		int line, col;
		int ch;
	} *parans, *misparans;
	U32 nParans, nMisparans;
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
	U32 nData;
	U32 index;
	// cursor index for cursor assertions
	U32 cursor;
	// visual position of the cursor (relative to the scrolled window origin) (can be used to highlight something later and not instantly)
	U32 line, col;
	U32 minLine, maxLine, minCol, maxCol;
};

// Skip spaces and tabs
// Note: Put this at the beginning of a state function
#define STATE_SKIPSPACE(s) if(isblank(s->data[s->index])) return 0
// Push a state to the state stack
int state_push(struct state *s, U32 state);
// Pop the state from the state stack into the current state
int state_pop(struct state *s);
// Add a word to the word list
int state_addword(struct state *s, int attr, const char *word, U32 nWord);
// Remove a word from the word list
int state_removeword(struct state *s, const char *word, U32 nWord);
// Find a word in the word list
// Returns 0 if the element was found and -1 if it failed
int state_findword(struct state *s, const char *word, U32 nWord);
// Add the current char as an opening paranthesis
int state_addparan(struct state *s);
// Add the current char as a closing paranthesis
// This function will find matching paranthesis
int state_addcounterparan(struct state *s, int counter);
// Acquire the next syntax element
int state_continue(struct state *s);
// Draw a char to the window
// Note: ONLY call this inside of c_cleanup
void state_addchar(struct state *s, U32 line, U32 col, char ch, int attr);
// Free all resources associated to this state
int state_cleanup(struct state *s);

// C syntax
extern int (*c_states[])(struct state *s);
// cnergy syntax
extern int (*cnergy_states[])(struct state *s);

#endif
