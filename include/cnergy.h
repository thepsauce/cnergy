#ifndef INCLUDED_EDITOR_H
#define INCLUDED_EDITOR_H

#define _GNU_SOURCE // for qsort_r
#include "base.h"
#include <curses.h>

#define ersline(max) ({ \
	const int _m = (max); \
	printw("%*s", MAX(_m - getcurx(stdscr), 0), ""); \
})

/****************************
 * Settings and environment *
 ****************************/

/**
 * Settings allow for high customizability. Settings include
 * colors, tabsize and so on.
 *
 * The environment can be used to run
 * custom programs and calls
 */

typedef enum {
	SET_TABSIZE,
	// line numbers of a focused window
	SET_COLOR_LINENR_FOCUS,
	SET_COLOR_LINENR,
	// waves at end of buffer
	SET_COLOR_ENDOFBUFFER,
	// status bar
	SET_COLOR_STATUSBAR1_FOCUS,
	SET_COLOR_STATUSBAR2_FOCUS,
	SET_COLOR_STATUSBAR1,
	SET_COLOR_STATUSBAR2,
	SET_COLOR_ITEM,
	SET_COLOR_ITEMSELECTED,
	SET_COLOR_DIRSELECTED,
	SET_COLOR_FILESELECTED,
	SET_COLOR_EXECSELECTED,
	SET_COLOR_DIR,
	SET_COLOR_FILE,
	SET_COLOR_EXEC,
	SET_COLOR_BROKENFILE,
	SET_CHAR_DIR,
	SET_CHAR_EXEC,
	// control characters
	SET_COLOR_CNTRL,
	SET_MAX,
} setting_t;

typedef enum {
#define REGISTER_LD(r) \
	INSTR_LD##r
#define REGISTER_LDO(r) \
	INSTR_LD##r##O
#define REGISTER_COMPATIBLELD(r1, r2) \
	INSTR_LD##r1##r2
#define REGISTER_LDMEM(r) \
	INSTR_LD##r##MEMB, INSTR_LD##r##MEMI, INSTR_LD##r##MEMP
#define REGISTER_STACK(r) \
	INSTR_PSH##r, INSTR_POP##r
#define REGISTER_MATH(r) \
	INSTR_ADD##r, INSTR_SUB##r, INSTR_MUL##r, INSTR_DIV##r, INSTR_INC##r, INSTR_DEC##r
	/* ldr [val] */
	REGISTER_LD(A),
	REGISTER_LD(B),
	REGISTER_LD(M),
	REGISTER_LD(N),
	/* ldor [val] */
	REGISTER_LDO(A),
	REGISTER_LDO(B),
	REGISTER_LDO(M),
	REGISTER_LDO(N),
	/* ldr1 r2 */
	REGISTER_COMPATIBLELD(A, B),
	REGISTER_COMPATIBLELD(A, M),
	REGISTER_COMPATIBLELD(A, N),

	REGISTER_COMPATIBLELD(B, A),
	REGISTER_COMPATIBLELD(B, M),
	REGISTER_COMPATIBLELD(B, N),

	REGISTER_COMPATIBLELD(M, B),
	REGISTER_COMPATIBLELD(M, A),
	REGISTER_COMPATIBLELD(M, N),

	REGISTER_COMPATIBLELD(N, B),
	REGISTER_COMPATIBLELD(N, A),
	REGISTER_COMPATIBLELD(N, M),
	/* pushr */
	/* popr */
	REGISTER_STACK(A),
	REGISTER_STACK(B),
	REGISTER_STACK(M),
	REGISTER_STACK(N),
	/* [opr]r */
	REGISTER_MATH(A),
	REGISTER_MATH(B),
	REGISTER_MATH(M),
	REGISTER_MATH(N),
	INSTR_JMP,
	INSTR_JZ,
	INSTR_CALL,
	INSTR_EXIT,
#undef REGISTER_LD
#undef REGISTER_LDO
#undef REGISTER_COMPATIBLELD
#undef REGISTER_LDMEM
#undef REGISTER_STACK
#undef REGISTER_MATH
} instr_t;

typedef enum {
	CALL_NULL,
	// general callss, these have a default behavior which cannot be overwritten but only extended
	CALL_SETMODE,
	CALL_VSPLIT,
	CALL_HSPLIT,
	CALL_COLORWINDOW,
	CALL_OPENWINDOW,
	CALL_CLOSEWINDOW,
	CALL_NEWWINDOW,
	CALL_MOVEWINDOW_RIGHT,
	CALL_MOVEWINDOW_BELOW,
	CALL_QUIT,
	// these have no default behavior and it solely depends on the window type what the behavior is
	CALL_CREATE,
	CALL_DESTROY,
	CALL_RENDER,
	CALL_TYPE,
	CALL_ASSERT,
	CALL_ASSERTCHAR,
	CALL_MOVECURSOR,
	CALL_MOVEHORZ,
	CALL_MOVEVERT,
	CALL_INSERT,
	CALL_INSERTCHAR,
	CALL_DELETE,
	CALL_DELETELINE,
	CALL_DELETESELECTION,
	CALL_COPY,
	CALL_PASTE,
	CALL_UNDO,
	CALL_REDO,
	CALL_WRITEFILE,
	CALL_READFILE,
	CALL_FIND,

	CALL_CHOOSE,
	CALL_TOGGLEHIDDEN,
	CALL_TOGGLESORTTYPE,
	CALL_TOGGLESORTREVERSE,
	CALL_SORTMODIFICATIONTIME,
	CALL_SORTALPHABETICAL,
	CALL_SORTCHANGETIME,

	CALL_MAX,
} call_t;

extern const struct call_info {
	const char *name;
	unsigned paramType;
} infoCalls[CALL_MAX];

/**
 * Enviroment is like a mini computer that is
 * used to execute user programs and handle calls.
 */
extern struct environment {
	/* memory pointer, stack pointer, instruction pointer */
	size_t mp, sp, ip;
	/* general porpuse registers */
	ptrdiff_t B, M;
	/**
	 * A is mainly the first parameter to calls and
	 * is als the return value.
	 */
	ptrdiff_t A;
	/* N is the number the user types in front of a bind */
	ptrdiff_t N;
	/* z flag is set on mathematical operations and calls */
	int z: 1;
	/**
	 * A certain section (offset from the start) inside memory
	 * is occupied with the keys the user pressed.
	 */
	char memory[0xffff];
	int settings[SET_MAX];
} main_environment;

void environment_call(call_t type);
int environment_loadandexec(void *program, size_t szProgram);

/***************
 * Sorted list *
 ***************/
// sortedlist.c
/**
 * This list is sorted at all times and can use that to
 * allow for faster access times using binary search at the price
 * of slightly slower insertion times.
 */

struct sortedlist {
	struct sortedlist_entry {
		char *word;
		/* The parameter can be anything you want */
		void *param;
	} *entries;
	size_t nEntries;
};

/** Adds and entry to the sorted list, it will not add the element if it already exists (no duplicates) */
int sortedlist_add(struct sortedlist *s, const char *word, size_t nWord, void *param);
int sortedlist_remove(struct sortedlist *s, const char *word, size_t nWord);
bool sortedlist_exists(struct sortedlist *s, const char *word, size_t nWord, void **pParam);

/********
 * UTF8 *
 ********/
// utf8.c

/**
 * Returns the width this would visually take up
 * Note: Newlines are interpreted as ^J
 */
int utf8_widthnstr(const char *str, size_t nStr);
int utf8_widthstr(const char *str);
/**
 * Returns the number of columns this character takes up
 * Note: nStr is not allowed to be 0, it's undefined behavior
 */
int utf8_width(const char *utf8, size_t nStr, int tabRef);
/** Returns the length of bytes of a single character */
unsigned utf8_len(const char *str, size_t nStr);
/**
 * Checks if the given character is valid utf8
 * Note: nStr is not allowed to be 0, might cause segfault
 */
bool utf8_valid(const char *utf8, size_t nStr);
/** Convert byte distance to char distance */
ptrdiff_t utf8_cnvdist(const char *str, size_t nStr, size_t index, ptrdiff_t distance);

/*********
 * Regex *
 *********/
// regex.c

typedef unsigned regex_nodeid_t;

struct regex_group {
	regex_nodeid_t head, tail;
};

struct regex_matcher {
	// unnamed capture groups
	struct regex_group unnGroups[9];
	// named capture groups
	struct {
		char name[64];
		struct regex_group group;
	} *groups;
	unsigned nGroups;
};

int regex_addpattern(struct regex_matcher *matcher, const char *pattern);

/**********
 * Dialog *
 **********/
// dialog.c
/**
 * Dialogs temporarily block all user input to show a message and wait for a user's reaction
 */

int messagebox(const char *title, const char *msg, ...);
// These functions show a dialog to the user if the allocation failed
// The user may then retry allocating or cancel
void *dialog_alloc(size_t sz);
void *dialog_realloc(void *ptr, size_t newSz);

/*************
 * Clipboard *
 *************/
// clip_x11.c
// clip_none.c

// X11 support YES
// MacOS support NO

/**
 * This function copies the given text to the clipboard
 * Returns 0 if the function succeeded
 */
int clipboard_copy(const char *text, size_t nText);
/**
 * This function gets the clipboard text and stores it inside text,
 * the received pointer must be freed by the caller
 * Returns 0 if the function succeeded
 */
int clipboard_paste(char **text);

#define ID_NULL UINT_MAX

typedef unsigned fileid_t;
typedef unsigned windowid_t;
typedef unsigned bufferid_t;

#include "filecache.h"
#include "buffer.h"
#include "window.h"
#include "bind.h"
#include "parse.h"

/***********************
 * Syntax highlighting *
 ***********************/
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
