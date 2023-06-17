#include "cnergy.h"
#include <dirent.h>
#include <unistd.h>

static int
files_compare(const void *a, const void *b, void *arg)
{
	struct filecache *fc1, *fc2;
	int cmp = 0;
	window_flags_t flags;
	struct window *win = (struct window*) arg;

	fc1 = fc_lock(*(fileid_t*) a);
	fc2 = fc_lock(*(fileid_t*) b);
	flags = win->flags;
	if(!flags.sortType)
		cmp = fc_type(fc1) - fc_type(fc2);
	if(!cmp)
		switch(flags.sort) {
		case FILEVIEW_SORT_ALPHA: cmp = strcasecmp(fc1->name, fc2->name); break;
		case FILEVIEW_SORT_MODTIME: cmp = fc1->mtime - fc2->mtime; break;
		case FILEVIEW_SORT_CHGTIME: cmp = fc1->ctime - fc2->ctime; break;
		}
	if(flags.sortReverse)
		cmp = -cmp;
	fc_unlock(fc1);
	fc_unlock(fc2);
	return cmp;
}

static inline int
fileviewer_render(windowid_t winid)
{
	struct {
		struct filecache *fc;
		unsigned index;
	} stack[32];
	struct filecache *fc;
	unsigned iStack = 0;
	fileid_t file;
	int y = 0;
	char basePath[PATH_MAX];
	struct window *const win = all_windows + winid;
	const int statusbar1Color = all_settings[winid == focus_window ? SET_COLOR_STATUSBAR1_FOCUS : SET_COLOR_STATUSBAR1];
	const int statusbar2Color = all_settings[SET_COLOR_STATUSBAR2];
	const int dirselectedColor = all_settings[SET_COLOR_DIRSELECTED];
	const int fileselectedColor = all_settings[SET_COLOR_FILESELECTED];
	const int execselectedColor = all_settings[SET_COLOR_EXECSELECTED];
	const int brokenfileColor = all_settings[SET_COLOR_BROKENFILE];
	const int dirColor = all_settings[SET_COLOR_DIR];
	const int fileColor = all_settings[SET_COLOR_FILE];
	const int execColor = all_settings[SET_COLOR_EXEC];
	const int dirChar = all_settings[SET_CHAR_DIR];
	const int execChar = all_settings[SET_CHAR_EXEC];
	win->base = fc_getbasefile(); // TODO: let each fileview have it's own base
	attrset(statusbar2Color);
	move(win->line, win->col);
	if(fc_getabsolutepath(win->base, basePath, sizeof(basePath)) < 0)
		return -1;
	addstr(basePath);
	ersline(win->col + win->cols);
	stack[0].fc = fc = fc_lock(win->base);
	stack[0].index = 0;
	qsort_r(fc->children, fc->nChildren, sizeof(*fc->children), files_compare, win);
	// this is used if the fileviewer is in type mode and we want to find the path the user typed
	const fileid_t selFile = fc_find(win->base, win->path);
	if(win->bindMode->flags & FBIND_MODE_TYPE)
		win->selected = -1;
	while(1) {
		while(stack[iStack].index == stack[iStack].fc->nChildren) {
			fc_unlock(stack[iStack].fc);
			if(!iStack)
				goto end;
			iStack--;
		}
		file = stack[iStack].fc->children[stack[iStack].index++];
		fc = fc_lock(file);
		const int type = fc_type(fc);
		if(win->selected == y) {
			fc_getrelativepath(win->base, file, win->path, sizeof(win->path));
			win->cursor = strlen(win->path);
			if(winid == focus_window) {
				focus_x = win->col;
				focus_y = win->line + y + 1 - win->scroll;
			}
		}
		if(win->selected < 0 && file == selFile) {
			win->selected = y;
			focus_x = win->col;
			focus_y = win->line + y + 1 - win->scroll;
		}
		if(y >= win->scroll && y - win->scroll < win->lines - 2) {
			if(win->selected == y)
				attrset(type == FC_TYPE_DIR ? dirselectedColor :
						type == FC_TYPE_REG ? fileselectedColor : execselectedColor);
			else
				attrset(type == FC_TYPE_DIR ? dirColor :
						type == FC_TYPE_REG ? fileColor : execColor);
			move(win->line + y - win->scroll + 1, win->col);
			printw("%*s", iStack * 2, "");
			if(type == FC_TYPE_EXEC)
				addch(execChar);
			addstr(fc->name);
			if(type == FC_TYPE_DIR)
				addch(dirChar);
			ersline(win->col + win->cols);
		}
		if(type == FC_TYPE_DIR && (fc->flags & FC_COLLAPSED)) {
			iStack++;
			stack[iStack].fc = fc;
			stack[iStack].index = 0;
			qsort_r(fc->children, fc->nChildren, sizeof(*fc->children), files_compare, win);
		} else {
			fc_unlock(fc);
		}
		y++;
	}
end:
	win->maxSelected = y - 1;
	if(win->selected > win->maxSelected)
		win->selected = win->maxSelected;
	// erase rest
	attrset(0);
	for(int p = y - win->scroll + win->line + 1; p < win->line + win->lines - 1; p++)
		mvprintw(p, win->col, "%*s", win->cols, "");

	attrset(statusbar1Color);
	move(win->line + win->lines - 1, win->col);
	addstr(win->path);
	ersline(win->col + win->cols);
	if(win->bindMode->flags & FBIND_MODE_TYPE) {
		if(winid == focus_window) {
			focus_y = win->line + win->lines - 1;
			focus_x = win->col + win->cursor;
		}
	}
	return 0;
}

bool
fileviewer_event(windowid_t winid, struct event *ev)
{
	struct window *const win = all_windows + winid;
	switch(ev->type) {
	case EVENT_TYPE:
		if(iscntrl(*ev->str))
			return false;
		if(strlen(win->path) + strlen(ev->str) >= sizeof(win->path))
			return false;
		memcpy(win->path + win->cursor + strlen(ev->str), win->path + win->cursor, strlen(win->path + win->cursor) + 1);
		memcpy(win->path + win->cursor, ev->str, strlen(ev->str));
		win->cursor += strlen(ev->str);
		return true;
	case EVENT_CHOOSE: {
		fileid_t file;
		struct filecache *fc;
		file = fc_find(win->base, win->path);
		if(!file)
			return false;
		fc = fc_lock(file);
		if(fc_isdir(fc)) {
			fc->flags ^= FC_COLLAPSED;
			fc_unlock(fc);
			fc_recache(file);
		} else if(fc_isreg(fc)) {
			bufferid_t bid;
			windowid_t wid;

			fc_unlock(fc);
			bid = buffer_new(file);
			if(bid == ID_NULL)
				break;
			wid = edit_new(bid, edit_statesfromfiletype(file));
			if(wid != ID_NULL) {
				window_copylayout(wid, winid);
				window_delete(winid);
				return true;
			}
		}
		return false;
	}
	default:
		// let the other switches take care of these types
		break;
	}
	if(win->bindMode->flags & FBIND_MODE_TYPE) {
		switch(ev->type) {
		case EVENT_MOVEHORZ: {
			const ssize_t p = utf8_cnvdist(win->path, strlen(win->path), win->cursor, ev->amount);
			win->cursor += p;
			return p == ev->amount;
		}
		case EVENT_DELETE: {
			const size_t n = strlen(win->path);
			const ssize_t p = utf8_cnvdist(win->path, n, win->cursor, ev->amount);
			if(p < 0) {
				memmove(win->path + win->cursor + p, win->path + win->cursor, n + 1 - win->cursor);
				win->cursor += p;
			} else {
				memmove(win->path + win->cursor, win->path + win->cursor + p, n + 1 - win->cursor - p);
			}
			return p == ev->amount;
		}
		default:
			return false;
		}
	} else {
		switch(ev->type) {
		case EVENT_MOVEVERT:
			if((!win->selected && ev->amount < 0) ||
					(win->selected == win->maxSelected && ev->amount >= 0))
				return false;
			win->selected = SAFE_ADD(win->selected, ev->amount);
			if(win->selected < 0) {
				win->selected = 0;
				win->scroll = 0;
				return false;
			}
			if(win->selected > win->maxSelected) {
				win->selected = win->maxSelected;
				win->scroll = MAX(win->maxSelected - win->lines + 3, 0);
				return false;
			}
			if(win->selected < win->scroll)
				win->scroll = win->selected;
			else if(win->selected - win->scroll >= win->lines - 3)
				win->scroll = win->selected - win->lines + 3;
			break;
		case EVENT_TOGGLEHIDDEN: win->flags.hidden = !win->flags.hidden; break;
		case EVENT_TOGGLESORTTYPE: win->flags.sortType = !win->flags.sortType; break;
		case EVENT_TOGGLESORTREVERSE: win->flags.sortReverse = !win->flags.sortReverse; break;
		case EVENT_SORTALPHABETICAL: win->flags.sort = FILEVIEW_SORT_ALPHA; break;
		case EVENT_SORTMODIFICATIONTIME: win->flags.sort = FILEVIEW_SORT_MODTIME; break;
		case EVENT_SORTCHANGETIME: win->flags.sort = FILEVIEW_SORT_CHGTIME; break;
		default:
			return false;
		}
	}
	return true;
}
