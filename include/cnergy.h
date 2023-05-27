#ifndef INCLUDED_EDITOR_H
#define INCLUDED_EDITOR_H

#include <assert.h>
#include <ctype.h>
#include <curses.h>
#include <dirent.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* General purpose */
#define MAX(a, b) ({ \
	__auto_type _a = (a); \
	__auto_type _b = (b); \
	_a > _b ? _a : _b; \
})
#define MIN(a, b) ({ \
	__auto_type _a = (a); \
	__auto_type _b = (b); \
	_a < _b ? _a : _b; \
})
#define ABS(a) ({ \
	__auto_type _a = (a); \
	_a < 0 ? -_a : _a; \
})
#define SAFE_ADD(a, b) ({ \
	__auto_type _a = (a); \
	__auto_type _b = (b); \
	typeof(_a) _c; \
	if(__builtin_add_overflow(_a, _b, &_c)) \
		_c = _a > 0 && _b > 0 ? INT_MAX : INT_MIN; \
	_c; \
})
#define SAFE_MUL(a, b) ({ \
	__auto_type _a = (a); \
	__auto_type _b = (b); \
	typeof(_a) _c; \
	if(__builtin_mul_overflow(_a, _b, &_c)) \
		_c = (a > 0 && b > 0) || (a < 0 && b < 0) ? INT_MAX : INT_MIN; \
	_c; \
})

#define ARRLEN(a) (sizeof(a)/sizeof*(a))
typedef int8_t I8;
typedef int16_t I16;
typedef int32_t I32;
typedef int64_t I64;
typedef uint8_t U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;

#define COLOR(bg, fg) COLOR_PAIR(1 + ((bg) | ((fg) * 16)))
#define ersline(max) ({ \
	__auto_type _m = (max); \
	printw("%*s", MAX((int) _m - getcurx(stdscr), 0), ""); \
})

#define _MACOS 1
#define _X11 2
#define _UNIX 3
#define _WINDOWS 4

void *const_alloc(const void *data, U32 sz);
U32 const_getid(const void *data);
void *const_getdata(U32 id);

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
	struct event *events;
	U32 nEvents;
	U32 iEvent;
	U32 nRefs;
};

extern struct buffer **buffers;
extern U32 nBuffers;

// Create a new buffer based on the given file, it may be null, then an empty buffer is created
// This also adds it to the internal buffer list
struct buffer *buffer_new(FILE *fp);
// Decrement the reference counter of this buffer
// It gets removed from memory if it hits 0
void buffer_free(struct buffer *buf);
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

/* Window */

// TODO: State saving technique for buffer windows
// This technique will be used to avoid re-rendering of the buffer every time.
// It will store a list of states and their position and when re-rendering, it will
// start from the closest previous state that is left to the caret.
// When the user inserts a character, .... (TODO: think about this; lookahead makes this complicated)

enum {
	WINDOW_BUFFER,
	WINDOW_FILEVIEW,
	WINDOW_COLORS,
};

enum {
	// no flags yet
	FWINDOW_______,
};

struct state;

struct window {
	U32 flags;
	U32 type;
	// file that is open inside of this window
	// this value can be null
	char *file;
	// time in milliseconds
	time_t fileTime;
	// this is the event on which the file was last saved
	U32 saveEvent;
	// position of the window
	int line, col;
	// size of the window
	int lines, cols;
	// a window can have multiple buffers but must have at least one
	U32 nBuffers;
	U32 iBuffer;
	struct buffer **buffers;
	// scrolling values of the last render
	U32 vScroll, hScroll;
	// selection cursor
	U32 selection;
	// that window is above/below this window
	struct window *above, *below;
	// that window is left/right of this window
	struct window *left, *right;
	// syntax states
	int (**states)(struct state *s);
};

extern struct window **all_windows;
extern U32 n_windows;
extern struct window *first_window;
extern struct window *focus_window;
extern int focus_y, focus_x;

enum {
	ATT_WINDOW_UNSPECIFIED,
	ATT_WINDOW_VERTICAL,
	ATT_WINDOW_HORIZONTAL,
};

// Create a new window and add it to the window list
struct window *window_new(struct buffer *buf);
// Safe function that detaches, deletes the window and changes focus-/first window if needed
void window_close(struct window *win);
// Delete this window from memory
// Note: You must detach a window before deleting it, otherwise it's undefined behavior
void window_delete(struct window *win);
// These four functions have additional handling and may return non null values even though the internal struct value is null
struct window *window_above(struct window *win);
struct window *window_below(struct window *win);
struct window *window_left(struct window *win);
struct window *window_right(struct window *win);
// Attach a window to another one
// pos can be one of the following values:
// ATT_WINDOW_UNSPECIFIED: The function decides where the window should best go
// ATT_WINDOW_VERTICAL: The window should go below
// ATT_WINDOW_HORIZONTAL: The window should go right 
void window_attach(struct window *to, struct window *win, int pos);
// Remove all connections to any other windows
void window_detach(struct window *win);
void window_layout(struct window *win);
void window_render(struct window *win);

/* Key bindings */

enum {
	BIND_CALL_NULL,
	BIND_CALL_STARTLOOP,
	BIND_CALL_ENDLOOP,
	BIND_CALL_REGISTER,
	BIND_CALL_ASSERT,
	BIND_CALL_MOVECURSOR,
	BIND_CALL_MOVEHORZ,
	BIND_CALL_MOVEVERT,
	BIND_CALL_INSERT,
	BIND_CALL_DELETE,
	BIND_CALL_DELETELINE,
	BIND_CALL_DELETESELECTION,
	BIND_CALL_SETMODE,
	BIND_CALL_COPY,
	BIND_CALL_PASTE,
	BIND_CALL_UNDO,
	BIND_CALL_REDO,
	BIND_CALL_COLORWINDOW,
	BIND_CALL_OPENWINDOW,
	BIND_CALL_CLOSEWINDOW,
	BIND_CALL_NEWWINDOW,
	BIND_CALL_MOVEWINDOW_RIGHT,
	BIND_CALL_MOVEWINDOW_BELOW,
	BIND_CALL_WRITEFILE,
	BIND_CALL_READFILE,
	BIND_CALL_QUIT
};

enum {
	FBIND_CALL_USENUMBER = 1 << 0,
	FBIND_CALL_AND = 1 << 1,
	FBIND_CALL_OR = 1 << 2,
	FBIND_CALL_XOR = 1 << 4,
};

struct binding_call {
	U16 flags;
	U16 type;
	I32 param;
};

struct binding {
	U32 nKeys;
	U32 nCalls;
	int *keys;
	struct binding_call *calls;
};

enum {
	FBIND_MODE_SUPPLEMENTARY = 1 << 0,
	FBIND_MODE_SELECTION = 1 << 1,
	FBIND_MODE_TYPE = 1 << 2,
};

struct binding_mode {
	char name[64];
	U32 flags;
	U32 nBindings;
	struct binding *bindings;
};

U32 mode_has(U32 flags);
const char *mode_name(void);
int mode_getmergepos(struct binding_mode *modes, U32 nModes, U32 *pos);
int mode_merge(struct binding_mode *modes, U32 nModes);

int bind_parse(FILE *fp);
int exec_bind(const int *keys, I32 amount);

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
	struct {
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
	// visual position of the cursor (can be used to highlight something later and not instantly)
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
void state_addchar(struct state *s, int line, int col, char ch, int attr);
// Free all resources associated to this state
int state_cleanup(struct state *s);

// C syntax
extern int (*c_states[])(struct state *s);
// cnergy syntax
extern int (*cnergy_states[])(struct state *s);

#endif
