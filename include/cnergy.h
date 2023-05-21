#ifndef INCLUDED_EDITOR_H
#define INCLUDED_EDITOR_H

#include <ctype.h>
#include <curses.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

#define _MACOS 1
#define _X11 2
#define _UNIX 3
#define _WINDOWS 4

void *const_alloc(const void *data, U32 sz);
int const_free(void *data);
U32 const_getid(const void *data);
void *const_getdata(U32 id);

#define COLOR(bg, fg) COLOR_PAIR(1 + ((bg) | ((fg) * 16)))

// Clipboard
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
};

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
U32 buffer_insert(struct buffer *buf, const char *str);
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
	WINDOW_COLORS,
};

enum {
	FWINDOW_PRIORITIZE_BELOW = 1 << 0,
};

struct window {
	U32 flags;
	U32 type;
	// position of the window
	int line, col;
	// size of the window
	int lines, cols;
	union {
		struct {
			// a window can have multiple buffers but must have at least one
			U32 nBuffers;
			U32 iBuffer;
			struct buffer *buffers;
			// scrolling values of the last render
			U32 vScroll, hScroll;
			// selection cursor
			U32 selection;
		};
	};
	// that window is right of this window
	struct window *right;
	// that window is below this window
	struct window *below;
};

extern struct window *first_window;
extern struct window *focus_window;

void window_layout(struct window *win);
void window_render(struct window *win);

/* Key bindings */
enum {
	BIND_CALL_NULL,
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
	BIND_CALL_COLOR_TEST,
};

enum {
	FBIND_CALL_USENUMBER = 1 << 0,
	FBIND_CALL_AND = 1 << 1,
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
int mode_getmergepos(struct binding_mode *modes, U32 nModes, U32 *pos);
int mode_merge(struct binding_mode *modes, U32 nModes);

int bind_parse(FILE *fp);
int exec_bind(const int *keys, I32 amount, struct window *window);

/* Syntax highlighting for C */
enum {
	C_STATE_DEFAULT,
	C_STATE_IDENTF,
	C_STATE_STRING,

	C_STATE_NUMBER,
	C_STATE_NUMBER_ZERO,
	C_STATE_NUMBER_DECIMAL,
	C_STATE_NUMBER_HEX,
	C_STATE_NUMBER_OCTAL,
	C_STATE_NUMBER_BINARY,
	C_STATE_NUMBER_EXP,
	C_STATE_NUMBER_NEGEXP,
	C_STATE_NUMBER_FLOAT,
	C_STATE_NUMBER_ERRSUFFIX,
	C_STATE_NUMBER_LSUFFIX,
	C_STATE_NUMBER_USUFFIX,

	C_STATE_PREPROC,
	C_STATE_PREPROC_COMMON,
	C_STATE_PREPROC_INCLUDE,

	C_STATE_LINECOMMENT,
	C_STATE_MULTICOMMENT,
};

struct c_state {
	// the stack can be used to repurpose states (states within states)
	U32 stateStack[32];
	U32 iStack;
	// current active state
	U32 state;
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
};

int c_feed(struct c_state *s);

#endif
