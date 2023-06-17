#include "cnergy.h"

int (**edit_statesfromfiletype(fileid_t file))(struct state *s)
{
	struct {
		const char *fileExt;
		int (**states)(struct state *s);
	} sfft[] = {
		{ "c", c_states },
		{ "h", c_states },
		{ "c++", c_states },
		{ "cpp", c_states },
		{ "cxx", c_states },
		{ "hpp", c_states },
		{ "cng", cnergy_states },
		{ "cnergy", cnergy_states },
	};
	int (**states)(struct state *s) = NULL;
	const char *ext;
	struct filecache *fc;

	fc = fc_lock(file);
	// just check file ending for now, maybe also use libmagic further down the road
	ext = strrchr(fc->name, '.');
	if(ext) {
		ext++;
		for(unsigned i = 0; i < ARRLEN(sfft); i++)
			if(!strcmp(ext, sfft[i].fileExt)) {
				states = sfft[i].states;
				break;
			}
	}
	fc_unlock(fc);
	return states;
}

windowid_t
edit_new(bufferid_t bufid, int (**states)(struct state *s))
{
	windowid_t winid;
	struct window *win;

	winid = window_new(WINDOW_EDIT);
	if(winid == ID_NULL)
		return ID_NULL;
	win = all_windows + winid;
	win->buffer = bufid;
	win->states = states;
	return winid;
}

static inline int
edit_render(windowid_t winid)
{
	bufferid_t bufid;
	struct buffer *buf;
	int curLine, curCol;
	size_t saveiGap;
	int nLineNumbers;
	struct state state;
	size_t minSel, maxSel;

	struct window *const win = all_windows + winid;
	// get buffer and cursor position
	bufid = win->buffer;
	buf = all_buffers + bufid;
	curLine = buffer_line(bufid);
	curCol = buffer_col(bufid);
	// move gap out of the way
	saveiGap = buf->iGap;
	unsafe_buffer_movecursor(buf, buf->nData - buf->iGap);
	// make sure there is an EOF at the end of the data (this is for convenience)
	if(!buf->nGap) {
		char *const newData = dialog_realloc(buf->data, buf->nData + BUFFER_GAP_SIZE);
		if(!newData)
			return -1;
		buf->data = newData;
		buf->nGap = BUFFER_GAP_SIZE;
	}
	buf->data[buf->nData] = EOF;

	// prepare state
	memset(&state, 0, sizeof(state));

	// scroll the text if the caret is out of view
	if(curLine < win->vScroll)
		win->vScroll = curLine;
	if(curLine >= win->lines - 1 + win->vScroll)
		win->vScroll = curLine - (win->lines - 2);

	state.minLine = win->vScroll;
	state.maxLine = win->vScroll + win->lines - 1;

	// get width of line numbers
	nLineNumbers = 1; // initial padding to the right side
	if(window_left(winid) == ID_NULL)
		nLineNumbers++; // add some padding
	for(int i = state.maxLine; i; i /= 10)
		nLineNumbers++;

	// note that we scroll a little extra horizontally
	// TODO: fix bug where scrolling will sometimes cause the cursor to be misaligned with a multi width character
	if(curCol <= win->hScroll)
		win->hScroll = MAX((int) curCol - win->cols / 4, 0);
	if(curCol >= win->cols - nLineNumbers + win->hScroll)
		win->hScroll = curCol - (win->cols - nLineNumbers - 2) + win->cols / 4;

	state.minCol = win->hScroll;
	state.maxCol = win->hScroll - nLineNumbers + win->cols;

	// get bounds of selection
	if(winid == focus_window && (win->bindMode->flags & FBIND_MODE_SELECTION)) {
		if(win->selection > saveiGap) {
			minSel = saveiGap;
			maxSel = win->selection;
		} else {
			minSel = win->selection;
			maxSel = saveiGap;
		}
	} else {
		// no selection
		minSel = 1;
		maxSel = 0;
	}

	const int tabsize = all_settings[SET_TABSIZE];
	const int lnrColor = winid == focus_window ? all_settings[SET_COLOR_LINENR_FOCUS] : all_settings[SET_COLOR_LINENR];
	const int cntrlColor = all_settings[SET_COLOR_CNTRL];
	const int endofbufferColor = all_settings[SET_COLOR_ENDOFBUFFER];
	const int statusbar1Color = winid == focus_window ? all_settings[SET_COLOR_STATUSBAR1_FOCUS] : all_settings[SET_COLOR_STATUSBAR1];
	const int statusbar2Color = winid == focus_window ? all_settings[SET_COLOR_STATUSBAR2_FOCUS] : all_settings[SET_COLOR_STATUSBAR2];

	// setup loop
	move(win->line, win->col);
	// draw first line if needed
	if(!win->vScroll) {
		attrset(lnrColor);
		printw("%*d ", nLineNumbers - 1, 1);
	}
	state.win = win;
	state.data = buf->data;
	state.nData = buf->nData;
	state.cursor = saveiGap;
	state.line = 0;
	state.col = 0;

	// draw all data
	for(size_t i = 0; i < buf->nData; state.index = i) {
		if(win->states)
			state_continue(&state);
		if(state.conceal) {
			const char *conceal;

			conceal = state.conceal;
			state.conceal = NULL;
			if(state.line != curLine) {
				i = state.index + 1;
				if(state.line >= state.minLine && state.line < state.maxLine) {
					attrset(state.attr);
					for(size_t len = strlen(conceal), utf8Len; len; len -= utf8Len) {
						utf8Len = utf8_len(conceal, len);
						if(state.col >= state.minCol && state.col < state.maxCol)
							addnstr(conceal, utf8Len);
						state.col += utf8_width(conceal, strlen(conceal), state.col);
						conceal += utf8Len;
					}
				}
				continue;
			}
		}
		// check if the state interpreted the utf8 encoded character correctly (if not we handle it here)
		if((buf->data[i] & 0x80) && state.index == i) {
			attrset(state.attr);
			const size_t len = utf8_len(buf->data + i, buf->nData - i);
			if(state.col >= state.minCol && state.col < state.maxCol) {
				if(i >= minSel && i <= maxSel)
					attron(A_REVERSE);
				if(!utf8_valid(buf->data + i, buf->nData - i)) {
					attrset(cntrlColor);
					if(state.col >= state.minCol && state.col < state.maxCol)
						addch('^');
					if(state.col + 1 >= state.minCol && state.col + 1 < state.maxCol)
						addch('?');
				} else {
					addnstr(buf->data + i, len);
				}
			}
			state.col += utf8_width(buf->data + i, buf->nData - i, state.col);
			i += len;
			continue;
		}
		for(; i <= state.index; i++) {
			const char ch = buf->data[i];
			attrset(state.attr);
			if(i >= minSel && i <= maxSel)
				attron(A_REVERSE);
			if(ch == '\t') {
				do {
					if(state.line >= state.minLine && state.line < state.maxLine &&
							state.col >= state.minCol && state.col < state.maxCol)
						addch(' ');
				} while((++state.col) % tabsize);
			} else if(ch == '\n') {
				if(state.line >= state.minLine && state.line < state.maxLine) {
					if(i >= minSel && i <= maxSel) {
						if(state.col >= state.minCol && state.col < state.maxCol) {
							// draw extra space for selection at end of line
							attrset(A_REVERSE);
							addch(' ');
						}
						state.col++;
					}
					// erase rest of line
					attrset(0);
					state.col = MAX(state.col, state.minCol);
					if(state.col < state.maxCol)
						printw("%*s", state.maxCol - state.col, "");
				}
				state.line++;
				if(state.line >= state.minLine && state.line < state.maxLine) {
					// draw line prefix
					attrset(lnrColor);
					mvprintw(win->line + state.line - state.minLine, win->col, "%*d ", nLineNumbers - 1, state.line + 1);
				}
				state.col = 0;
			} else if(iscntrl(ch)) {
				if(state.line >= state.minLine && state.line < state.maxLine) {
					attron(cntrlColor);
					if(state.col >= state.minCol && state.col < state.maxCol)
						addch('^');
					if(state.col + 1 >= state.minCol && state.col + 1 < state.maxCol)
						addch(ch == 0x7f ? '?' : ch + 'A' - 1);
				}
				state.col += 2;
			} else {
				if(state.line >= state.minLine && state.line < state.maxLine && state.col >= state.minCol && state.col < state.maxCol)
					addch(ch);
				state.col++;
			}
		}
	}
	// erase rest of line and draw little waves until we hit the status bar
	attrset(endofbufferColor);
	while(state.line < state.maxLine) {
		state.col = MAX(state.col, state.minCol);
		if(state.col < state.maxCol)
			printw("%*s", state.maxCol - state.col, "");
		state.line++;
		if(state.line >= state.maxLine)
			break;
		mvprintw(state.line + win->line - state.minLine, win->col, "%*s~ ", nLineNumbers - 2, "");
		state.col = 0;
	}
	state_cleanup(&state);
	// draw status bar (TODO: what if the status bar doesn't fit?)
	attrset(statusbar1Color);
	mvaddstr(win->line + win->lines - 1, win->col, " Buffer");
	attrset(statusbar2Color);
	if(buf->saveEvent != buf->iEvent)
		addch('*');
	if(buf->file) {
		struct filecache *fc;
		fc = fc_lock(buf->file);
		printw(" %s", fc->name);
		fc_unlock(fc);
	}
	printw(" %u, %u", curLine, curCol);
	printw("%*s", MAX(win->col + win->cols - getcurx(stdscr), 0), "");
	// set the global end caret position
	if(winid == focus_window) {
		focus_y = win->line + curLine - state.minLine;
		focus_x = win->col + curCol + nLineNumbers - state.minCol;
	}
	// move back the gap before we leave
	unsafe_buffer_movecursor(buf, saveiGap - buf->iGap);
	return 0;
}

bool
edit_event(windowid_t winid, struct event *ev)
{
	struct window *const win = all_windows + winid;
	const bufferid_t bufid = win->buffer;
	struct buffer *const buf = all_buffers + bufid;
	switch(ev->type) {
	case EVENT_TYPE:
		buffer_insert(bufid, ev->str, strlen(ev->str));
		break;
	case EVENT_RENDER:
		edit_render(winid);
		break;
	case EVENT_SETMODE:
		if(win->bindMode->flags & FBIND_MODE_SELECTION)
			win->selection = buf->iGap;
		return true;
	case EVENT_ASSERT:
		return !memcmp(ev->str, buf->data + buf->iGap + buf->nGap, MIN(strlen(ev->str), buf->nData - buf->iGap));
	case EVENT_ASSERTCHAR:
		return buf->iGap != buf->nData && ev->c == buf->data[buf->iGap + buf->nGap];
	case EVENT_MOVECURSOR: return buffer_movecursor(bufid, ev->amount) == ev->amount;
	case EVENT_MOVEHORZ: return buffer_movehorz(bufid, ev->amount) == ev->amount;
	case EVENT_INSERTCHAR:
		buffer_insert(bufid, &ev->c, 1);
		return true;
	case EVENT_MOVEVERT: return buffer_movevert(bufid, ev->amount) == ev->amount;
	case EVENT_INSERT:
		buffer_insert(bufid, ev->str, strlen(ev->str));
		return true;
	case EVENT_DELETE: return buffer_delete(bufid, ev->amount) == ev->amount;
	case EVENT_DELETELINE: return buffer_deleteline(bufid, ev->amount) == ev->amount;
	case EVENT_DELETESELECTION: {
		size_t n;

		if(!(win->bindMode->flags & FBIND_MODE_SELECTION) || !buf->nData)
			return false;
		if(win->selection > buf->iGap)
			n = win->selection - buf->iGap + 1;
		else {
			n = buf->iGap - win->selection + 1;
			buffer_movecursor(bufid, win->selection - buf->iGap);
		}
		buffer_delete(bufid, n);
		win->selection = buf->iGap;
		return true;
	}
	case EVENT_COPY: {
		char *text;
		size_t nText;

		if(!(win->bindMode->flags & FBIND_MODE_SELECTION) || !buf->nData)
			break;
		if(win->selection > buf->iGap) {
			text = buf->data + buf->iGap + buf->nGap;
			nText = win->selection - buf->iGap + 1;
		} else {
			text = buf->data + win->selection;
			nText = buf->iGap - win->selection + 1;
		}
		return !clipboard_copy(text, nText);
	}
	case EVENT_PASTE: {
		char *text;

		if(!clipboard_paste(&text)) {
			buffer_insert(bufid, text, strlen(text));
			free(text);
			return true;
		}
		return false;
	}
	case EVENT_UNDO:
		return buffer_undo(bufid);
	case EVENT_REDO:
		return buffer_redo(bufid);
	case EVENT_WRITEFILE:
		return buffer_save(bufid);
	case EVENT_FIND:
		// TODO: find should get a rework soon where regex is used
		for(size_t i = buf->iGap + buf->nGap; i < buf->nData + buf->nGap; i++)
			if(buf->data[i] == ev->amount) {
				ev->cached = i - buf->iGap - buf->nGap;
				return true;
			}
		return false;
	default:
		return false;
	}
}
