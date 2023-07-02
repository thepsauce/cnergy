#ifndef INCLUDED_EDITOR_H
#define INCLUDED_EDITOR_H

#define _GNU_SOURCE // for qsort_r
#include "base.h"
#include <curses.h>

/* there is a big private use area after U+f6b1 (technically from U+e000 to U+f8ff),
 * we use this for special keys whose value depend on the terminfo entry
 * that are larger than 0xff
 */
#define SPECIAL_KEY(key) ((key) + (0xf6b1 - 0xff))

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

#define ALLINSTRUCTIONS  \
	/* ldr [val] */ \
	REGISTER_LD(A) \
	REGISTER_LD(B) \
	REGISTER_LD(C) \
	REGISTER_LD(D) \
	/* ldor [val] */ \
	REGISTER_LDO(A) \
	REGISTER_LDO(B) \
	REGISTER_LDO(C) \
	REGISTER_LDO(D) \
	/* ldr1 r2 */ \
	REGISTER_COMPATIBLELD(A, B) \
	REGISTER_COMPATIBLELD(A, C) \
	REGISTER_COMPATIBLELD(A, D) \
	REGISTER_COMPATIBLELD(B, A) \
	REGISTER_COMPATIBLELD(B, C) \
	REGISTER_COMPATIBLELD(B, D) \
	REGISTER_COMPATIBLELD(C, B) \
	REGISTER_COMPATIBLELD(C, A) \
	REGISTER_COMPATIBLELD(C, D) \
	REGISTER_COMPATIBLELD(D, B) \
	REGISTER_COMPATIBLELD(D, A) \
	REGISTER_COMPATIBLELD(D, C) \
	/* ldr byte|int|double [value] */ \
	REGISTER_LDMEM(A) \
	REGISTER_LDMEM(B) \
	REGISTER_LDMEM(C) \
	REGISTER_LDMEM(D) \
	/* pushr */ \
	/* popr */ \
	REGISTER_STACK(A) \
	REGISTER_STACK(B) \
	REGISTER_STACK(C) \
	REGISTER_STACK(D) \
	/* [opr]r */ \
	REGISTER_MATH(A) \
	REGISTER_MATH(B) \
	REGISTER_MATH(C) \
	REGISTER_MATH(D) \
	REGISTER_MISC

typedef enum {
#define REGISTER_LD(r) \
	INSTR_LD##r,
#define REGISTER_LDO(r) \
	INSTR_LD##r##O,
#define REGISTER_COMPATIBLELD(r1, r2) \
	INSTR_LD##r1##r2,
#define REGISTER_LDMEM(r) \
	INSTR_LD##r##BYTE, INSTR_LD##r##INT, INSTR_LD##r##PTR,
#define REGISTER_STACK(r) \
	INSTR_PSH##r, INSTR_POP##r,
#define REGISTER_MATH(r) \
	INSTR_ADD##r, INSTR_SUB##r, INSTR_MUL##r, INSTR_DIV##r, INSTR_INC##r, INSTR_DEC##r,
#define REGISTER_MISC \
	INSTR_JMP, \
	INSTR_JZ, \
	INSTR_JNZ, \
	INSTR_CALL, \
	INSTR_EXIT, \
	INSTR_MAX,
	ALLINSTRUCTIONS
#undef REGISTER_LD
#undef REGISTER_LDO
#undef REGISTER_COMPATIBLELD
#undef REGISTER_LDMEM
#undef REGISTER_STACK
#undef REGISTER_MATH
#undef REGISTER_MISC
} instr_t;

extern const char *instrNames[INSTR_MAX];

typedef enum {
	CALL_NULL,
	/* page calls */
	PAGE_CONSTRUCT,
	PAGE_LAYOUT,
	PAGE_DESTRUCT,
	/* component calls */
	COMP_RENDER,
	// general calls, these have a default behavior which cannot be overwritten but only extended
	CALL_NORMAL,
	CALL_INSERT,
	CALL_VISUAL,
	CALL_VSPLIT,
	CALL_HSPLIT,
	CALL_COLORPAGE,
	CALL_OPENPAGE,
	CALL_CLOSEPAGE,
	CALL_NEWPAGE,
	CALL_MOVERIGHTPAGE,
	CALL_MOVEBELOWPAGE,
	CALL_QUIT,
	// these have no default behavior and it solely depends on the window type what the behavior is
	CALL_ASSERTSTRING,
	CALL_ASSERTCHAR,
	CALL_MOVECURSOR,
	CALL_MOVEHORZ,
	CALL_MOVEVERT,
	CALL_INSERTSTRING,
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

	CALL_CONFIRM,
	CALL_CANCEL,
	CALL_SWAP,
	CALL_CHOOSE,
	CALL_TOGGLEHIDDEN,
	CALL_TOGGLESORTTYPE,
	CALL_TOGGLESORTREVERSE,
	CALL_SORTMODIFICATIONTIME,
	CALL_SORTALPHABETICAL,
	CALL_SORTCHANGETIME,

	CALL_MAX,
} call_t;

extern const char *callNames[CALL_MAX];

/**
 * Enviroment is like a mini computer that is
 * used to execute user programs and handle calls.
 */
extern struct environment {
	/* memory pointer, stack pointer, instruction pointer */
	size_t mp, sp, ip;
	/* general porpuse registers */
	ptrdiff_t B, D;
	/**
	 * A is mainly the first parameter to calls and
	 * is als the return value.
	 */
	ptrdiff_t A;
	/* C is the number the user types in front of a bind */
	ptrdiff_t C;
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
int environment_loadandprint(void *program, size_t szProgram);

/********
 * UTF8 *
 ********/
// utf8.c

/**
 * Converts given unicode character to utf8 and
 * returns the length of the utf8 sequence or
 * 0 if the uc is invalid
 */
unsigned utf8_convunicode(int uc, char *utf8);
/**
 * Converts given utf8 sequence to a unicode character and
 * returns that uc or -1 if the utf8 sequence is invalid
 */
int utf8_tounicode(const char *utf, size_t n);
/**
 * Returns the width this would visually take up
 * Note: Newlines are interpreted as ^J
 */
int utf8_widthnstr(const char *utf8, size_t n);
int utf8_widthstr(const char *utf8);
/**
 * Returns the number of columns this character takes up
 * Note: nStr is not allowed to be 0, it's undefined behavior
 */
int utf8_width(const char *utf8, size_t n, int tabRef);
/** Returns the length of bytes of a single character */
unsigned utf8_len(const char *utf8, size_t n);
/**
 * Checks if the given character is valid utf8
 * Note: nStr is not allowed to be 0, might cause segfault
 */
bool utf8_valid(const char *utf8, size_t n);
/** Convert byte distance to char distance */
ptrdiff_t utf8_cnvdist(const char *utf8, size_t n, size_t index, ptrdiff_t distance);

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
typedef unsigned pageid_t;
typedef unsigned compid_t;
typedef unsigned bufferid_t;

#include "filecache.h"
#include "buffer.h"
#include "page.h"
#include "bind.h"
#include "parse.h"

#endif
