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

int
fileviewer_render(struct window *win)
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
	const int statusbar1Color = all_settings[win == focus_window ? SET_COLOR_STATUSBAR1_FOCUS : SET_COLOR_STATUSBAR1];
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
	fc_getabsolutepath(win->base, basePath, sizeof(basePath));
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
			if(win == focus_window) {
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
		if(win == focus_window) {
			focus_y = win->line + win->lines - 1;
			focus_x = win->col + win->cursor;
		}
	}
	return 0;
}

int
fileviewer_type(struct window *win, const char *str, size_t nStr)
{
	if(iscntrl(*str))
		return 1;
	if(strlen(win->path) + nStr >= sizeof(win->path))
		return 1;
	memcpy(win->path + win->cursor + nStr, win->path + win->cursor, strlen(win->path + win->cursor) + 1);
	memcpy(win->path + win->cursor, str, nStr);
	win->cursor += nStr;
	return 0;
}

bool
fileviewer_bindcall(struct window *win, struct binding_call *bc, ssize_t param)
{
	switch(bc->type) {
	case BIND_CALL_CHOOSE: {
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
			struct buffer *buf;
			struct window *newWin;

			fc_unlock(fc);
			buf = buffer_new(file);
			if(!buf)
				break;
			newWin = edit_new(buf, edit_statesfromfiletype(file));
			if(newWin) {
				window_copylayout(win, newWin);
				window_delete(win);
				return true;
			}
		}
		return false;
	}
	default:
		// let the other switches take care of these types
	}
	if(win->bindMode->flags & FBIND_MODE_TYPE) {
		switch(bc->type) {
		case BIND_CALL_MOVEHORZ: {
			const ssize_t p = utf8_cnvdist(win->path, strlen(win->path), win->cursor, param);
			win->cursor += p;
			return p == param;
		}
		case BIND_CALL_DELETE: {
			const size_t n = strlen(win->path);
			const ssize_t p = utf8_cnvdist(win->path, n, win->cursor, param);
			if(p < 0) {
				memmove(win->path + win->cursor + p, win->path + win->cursor, n + 1 - win->cursor);
				win->cursor += p;
			} else {
				memmove(win->path + win->cursor, win->path + win->cursor + p, n + 1 - win->cursor - p);
			}
			return p == param;
		}
		default:
			return false;
		}
	} else {
		switch(bc->type) {
		case BIND_CALL_MOVEVERT:
			if((!win->selected && param < 0) ||
					(win->selected == win->maxSelected && param >= 0))
				return false;
			win->selected = SAFE_ADD(win->selected, param);
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
		case BIND_CALL_TOGGLEHIDDEN: win->flags.hidden = !win->flags.hidden; break;
		case BIND_CALL_TOGGLESORTTYPE: win->flags.sortType = !win->flags.sortType; break;
		case BIND_CALL_TOGGLESORTREVERSE: win->flags.sortReverse = !win->flags.sortReverse; break;
		case BIND_CALL_SORTALPHABETICAL: win->flags.sort = FILEVIEW_SORT_ALPHA; break;
		case BIND_CALL_SORTMODIFICATIONTIME: win->flags.sort = FILEVIEW_SORT_MODTIME; break;
		case BIND_CALL_SORTCHANGETIME: win->flags.sort = FILEVIEW_SORT_CHGTIME; break;
		default:
			return false;
		}
	}
	return true;
}
