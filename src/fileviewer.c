#include "cnergy.h"
#include <dirent.h>
#include <unistd.h>

// There is not multithreading, so this struct is save to use for all windows one after another
static struct fileview {
	struct window *win;
	char curPath[PATH_MAX]; // path we are currently in
		char basePath[PATH_MAX]; // path to start from
	struct fileview_stack_frame {
		char **names;
		unsigned nNames;
		unsigned iName; // index of current file
	} stack[32];
	unsigned nStack;
	struct sortedlist collapsed;
} fileview;

static int
files_compare(const void *a, const void *b)
{
	char path1[PATH_MAX], path2[PATH_MAX];
	size_t nPath;
	struct file_info fi1, fi2;
	int cmp = 0;
	window_flags_t flags;

	nPath = strlen(fileview.curPath);

	memcpy(path1, fileview.curPath, nPath);
	path1[nPath] = '/';
	strcpy(path1 + nPath + 1, *(const char**) a);

	memcpy(path2, fileview.curPath, nPath);
	path2[nPath] = '/';
	strcpy(path2 + nPath + 1, *(const char**) b);

	getfileinfo(&fi1, path1);
	getfileinfo(&fi2, path2);
	flags = fileview.win->flags;
	if(!flags.sortType && fi1.type != fi2.type)
		cmp = fi1.type - fi2.type;
	else
		switch(flags.sort) {
		case FILEVIEW_SORT_ALPHA: cmp = strcasecmp(*(const char**) a, *(const char**) b); break;
		case FILEVIEW_SORT_MODTIME: cmp = fi1.modTime - fi2.modTime; break;
		case FILEVIEW_SORT_CHGTIME: cmp = fi1.chgTime - fi2.chgTime; break;
		}
	if(flags.sortReverse)
		cmp = -cmp;
	return cmp;
}

static const char *
fileview_nextentry(void)
{
	const char *name;
	struct fileview_stack_frame *frame;

	frame = fileview.stack + fileview.nStack - 1;
	do {
		while(frame->iName == frame->nNames) {
			fileview.nStack--;
			if(!fileview.nStack)
				return NULL;
			// chop off the end
			*strrchr(fileview.curPath, '/') = 0;
			frame--;
		}
		name = frame->names[frame->iName++];
	} while(!fileview.win->flags.hidden && name[0] == '.');
	return name;
}

static int
fileview_recurseinto(void)
{
	DIR *dir;
	struct fileview_stack_frame *frame;
	struct dirent *ent;
	char **newNames;
	char *name;

	if(fileview.nStack == ARRLEN(fileview.stack))
		return 1;
	// read all names to be able to sort them
	dir = opendir(fileview.curPath);
	if(!dir)
		return -1;
	frame = fileview.stack + fileview.nStack++;
	frame->iName = 0;
	frame->nNames = 0;
	while((ent = readdir(dir))) {
		if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
			continue;
		newNames = realloc(frame->names, sizeof(*frame->names) * (frame->nNames + 1));
		if(!newNames)
			break;
		name = const_alloc(ent->d_name, strlen(ent->d_name) + 1);
		frame->names = newNames;
		frame->names[frame->nNames++] = name;
	}
	closedir(dir);
	qsort(frame->names, frame->nNames, sizeof(*frame->names), files_compare);
	return 0;
}

int
fileviewer_render(struct window *win)
{
	const char *name, *nextName;
	int y = 0;
	fileview.win = win;
	// TODO: set the base path somewhere else so each fileview can have its own
	getcwd(fileview.basePath, PATH_MAX);
	strcpy(fileview.curPath, fileview.basePath);
	fileview.nStack = 0;
	fileview_recurseinto();
	attrset(COLOR_PAIR(3));
	move(win->line, win->col);
	addstr(fileview.basePath);
	while((nextName = fileview_nextentry())) {
		struct file_info fi;
		name = nextName;
		const size_t nName = strlen(name);
		const size_t nOld = strlen(fileview.curPath);
		if(nOld + nName + 1 >= PATH_MAX)
			goto draw_broken_item;
		fileview.curPath[nOld] = '/';
		strcpy(fileview.curPath + nOld + 1, name);
		getfileinfo(&fi, fileview.curPath);
		if(win->selected == y && !(win->bindMode->flags & FBIND_MODE_TYPE)) {
			strcpy(win->selPath, fileview.curPath);
			if(win == focus_window) {
				focus_x = win->col;
				focus_y = win->line + y + 1 - win->scroll;
			}
		}
		if(y >= win->scroll && y - win->scroll < win->line + win->lines - 2) {
			attrset(COLOR(fi.type == FI_TYPE_DIR ? 6 : fi.type == FI_TYPE_REG ? 7 : 3, win->selected == y ? 8 : 0));
			move(win->line + y - win->scroll + 1, win->col);
			printw("%*s", (fileview.nStack - 1) * 2, "");
			if(fi.type == FI_TYPE_EXEC)
				addch('*');
			addstr(name);
			if(fi.type == FI_TYPE_DIR)
				addch('/');
			ersline(win->col + win->cols);
		}
		if(fi.type == FI_TYPE_DIR && sortedlist_exists(&fileview.collapsed, fileview.curPath, strlen(fileview.curPath), NULL))
			fileview_recurseinto();
		else
			fileview.curPath[nOld] = 0;
		y++;
		continue;
	draw_broken_item:
		if(y >= win->scroll && y - win->scroll < win->line + win->lines - 2) {
			attrset(COLOR_PAIR(0));
			mvaddstr(win->line + y, win->col, name);
		}
		y++;
	}
	win->maxSelected = y - 1;
	if(win->selected > win->maxSelected)
		win->selected = win->maxSelected;
	// erase rest
	attrset(0);
	for(int p = y - win->scroll + win->line + 1; p < win->line + win->lines - 1; p++)
		mvprintw(p, win->col, "%*s", win->cols, "");

	attrset(COLOR_PAIR(3));
	move(win->line + win->lines - 1, win->col);
	addstr(win->selPath);
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
	if(strlen(win->selPath) + nStr >= sizeof(win->selPath))
		return 1;
	memcpy(win->selPath + win->cursor + nStr, win->selPath + win->cursor, strlen(win->selPath + win->cursor) + 1);
	memcpy(win->selPath + win->cursor, str, nStr);
	win->cursor += nStr;
	return 0;
}

bool
fileviewer_bindcall(struct window *win, struct binding_call *bc, ssize_t param)
{
	switch(bc->type) {
	case BIND_CALL_CHOOSE: {
		struct file_info fi;
		if(!getfileinfo(&fi, win->selPath)) {
			if(fi.type == FI_TYPE_DIR) {
				const size_t n = strlen(win->selPath);
				if(sortedlist_add(&fileview.collapsed, win->selPath, n, NULL))
					sortedlist_remove(&fileview.collapsed, win->selPath, n);
			} else if(fi.type == FI_TYPE_REG) {
				struct buffer *buf;
				struct window *newWin;

				buf = buffer_new(win->selPath);
				if(!buf)
					break;
				newWin = edit_new(buf, edit_statesfromfiletype(win->selPath));
				if(newWin) {
					window_copylayout(win, newWin);
					window_delete(win);
					return true;
				}
			}
		}
		return false;
	}
	default:
		// let the other switches take care about these types
	}
	if(win->bindMode->flags & FBIND_MODE_TYPE) {
		switch(bc->type) {
		case BIND_CALL_MOVEHORZ: {
			const ssize_t p = utf8_cnvdist(win->selPath, strlen(win->selPath), win->cursor, param);
			win->cursor += p;
			return p == param;
		}
		case BIND_CALL_DELETE: {
			const size_t n = strlen(win->selPath);
			const ssize_t p = utf8_cnvdist(win->selPath, n, win->cursor, param);
			if(p < 0) {
				memmove(win->selPath + win->cursor + p, win->selPath + win->cursor, n + 1 - win->cursor);
				win->cursor += p;
			} else {
				memmove(win->selPath + win->cursor, win->selPath + win->cursor + p, n + 1 - win->cursor - p);
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
