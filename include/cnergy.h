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

extern int settings_tabsize;

#define FMODE_SELECTION (1 << 0)
#define FMODE_TYPE (1 << 1)

struct editormode {
	unsigned flags;
};

extern struct editormode *cur_mode;

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

#endif
