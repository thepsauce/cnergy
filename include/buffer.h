#ifndef INCLUDED_BUFFER_H
#define INCLUDED_BUFFER_H

// buffer.c
/**
 * The gap buffer is a dynamic array that has a gap which is part of the data, this makes for very efficient
 * insertion and especially deletion operations but slightly slows down caret movement
 */

enum {
	BUFFER_EVENT_INSERT,
	BUFFER_EVENT_DELETE,
	BUFFER_EVENT_REPLACE,
};

// TODO: maybe make a global event queue that can be used from anywhere and is also stored in a file (restore session on crash)
struct buffer_event {
	unsigned flags;
	unsigned type;
	size_t iGap;
	unsigned vct, prevVct;
	char *ins, *del;
	size_t nIns, nDel;
};

/* Default size of the buffer gap */
#define BUFFER_GAP_SIZE 64

struct buffer {
	char *data;
	size_t nData;
	size_t iGap;
	size_t nGap;
	int vct;
	fileid_t file;
	unsigned saveEvent;
	struct buffer_event *events;
	unsigned nEvents;
	unsigned iEvent;
};

extern struct buffer *all_buffers;
extern bufferid_t n_buffers;

/** Create a new buffer based on the given file, it may be 0, then an empty buffer is created */
bufferid_t buffer_new(fileid_t file);
/**
 * Free all resources associated to this buffer
 * Make sure to delete all windows associated to this buffer before calling this function
 */
void buffer_free(bufferid_t bufid);
/** Saves the buffer to its file; if it has none, the user will be asked to choose a file */
int buffer_save(bufferid_t bufid);
/** Returns the moved amount */
ptrdiff_t unsafe_buffer_movecursor(struct buffer *buf, ptrdiff_t distance);
ptrdiff_t buffer_movecursor(bufferid_t bufid, ptrdiff_t distance);
/**
 * Moves the cursor horizontally
 * Returns the moved amount
 */
ptrdiff_t buffer_movehorz(bufferid_t bufid, ptrdiff_t distance);
/**
 * Move the cursor up or down by the specified number of lines
 * Returns the moved amount
 **/
ptrdiff_t buffer_movevert(bufferid_t bufid, ptrdiff_t distance);
/** Returns the number of characters inserted */
size_t buffer_insert(bufferid_t bufid, const char *str, size_t nStr);
/** Returns the number of characters inserted */
size_t buffer_insert_file(bufferid_t bufid, fileid_t file);
/** Returns the amount that was deleted */
ptrdiff_t buffer_delete(bufferid_t bufid, ptrdiff_t amount);
/** Returns the amount of lines that were deleted */
ptrdiff_t buffer_deleteline(bufferid_t bufid, ptrdiff_t amount);
/** Returns the amount of events undone */
int buffer_undo(bufferid_t bufid);
/** Returns the amount of events redone */
int buffer_redo(bufferid_t bufid);
/** Returns line index at cursor */
int buffer_line(bufferid_t bufid);
/** Returns column index at cursor */
int buffer_col(bufferid_t bufid);
/** Returns the length of the line */
size_t buffer_getline(bufferid_t bufid, int line, char *dest, size_t maxDest);


#endif
