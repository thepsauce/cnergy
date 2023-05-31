#include "cnergy.h"
#include <dirent.h>
#include <unistd.h>

enum {
	FFILEVIEW_SHOWHIDDEN = 1 << 1,
	FFILEVIEW_SORTALPHA = 1 << 2,
	FFILEVIEW_SORTMODIFY = 1 << 3,
	FFILEVIEW_SORTCHANGE = 1 << 4,
	FFILEVIEW_SORTTYPE = 1 << 5,
};
static struct fileview {
	U32 flags;
	int selected;
	int maxSelected; // each render call figures this one out
	int scroll;
	int cursor; // cursor inside of selPath
	char basePath[PATH_MAX]; // path to start from
	char selPath[PATH_MAX]; // selected path (path visible at the bottom)
	char curPath[PATH_MAX]; // path we are currently in
	struct fileview_stack_frame {
		char **names;
		U32 nNames;
		U32 iName; // index of current file
	} stack[32];
	U32 nStack;
} fileview;

static int
files_compare(const void *a, const void *b)
{
	char path1[PATH_MAX], path2[PATH_MAX];
	U32 nPath;
	struct file_info fi1, fi2;
	int cmp = 0;

	nPath = strlen(fileview.curPath);

	memcpy(path1, fileview.curPath, nPath);
	path1[nPath] = '/';
	strcpy(path1 + nPath + 1, *(const char**) a);

	memcpy(path2, fileview.curPath, nPath);
	path2[nPath] = '/';
	strcpy(path2 + nPath + 1, *(const char**) b);

	getfileinfo(&fi1, path1);
	getfileinfo(&fi2, path2);
	if(fileview.flags & FFILEVIEW_SORTALPHA)
		cmp = strcasecmp(*(const char**) a, *(const char**) b);
	else if(fileview.flags & FFILEVIEW_SORTMODIFY)
		cmp = fi1.modTime - fi2.modTime;
	else if(fileview.flags & FFILEVIEW_SORTCHANGE)
		cmp = fi1.chgTime - fi2.chgTime;
	if(fileview.flags & FFILEVIEW_SORTTYPE) {
		if(fi1.type != fi2.type)
			cmp = fi1.type - fi2.type;
	}
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
	} while(!(fileview.flags & FFILEVIEW_SHOWHIDDEN) && name[0] == '.');
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
		if(fileview.selected == y && !(win->bindMode->flags & FBIND_MODE_TYPE))
			strcpy(fileview.selPath, fileview.curPath);
		if(y >= fileview.scroll && y - fileview.scroll < win->line + win->lines - 2) {
			attrset(COLOR(fi.type == FI_TYPE_DIR ? 6 : fi.type == FI_TYPE_REG ? 7 : 3, fileview.selected == y ? 8 : 0));
			move(win->line + y - fileview.scroll + 1, win->col);
			printw("%*s", (fileview.nStack - 1) * 2, "");
			if(fi.type == FI_TYPE_EXEC)
				addch('*');
			addstr(name);
			if(fi.type == FI_TYPE_DIR)
				addch('/');
			ersline(win->col + win->cols);
		}
		if(fi.type == FI_TYPE_DIR)
			fileview_recurseinto();
		else
			fileview.curPath[nOld] = 0;
		y++;
		continue;
	draw_broken_item:
		if(y >= fileview.scroll && y - fileview.scroll < win->line + win->lines - 2) {
			attrset(COLOR_PAIR(0));
			mvaddstr(win->line + y, win->col, name);
		}
		y++;
	}
	fileview.maxSelected = y - 1;
	if(fileview.selected > fileview.maxSelected)
		fileview.selected = fileview.maxSelected;
	// erase rest
	attrset(0);
	for(int p = y - fileview.scroll + win->line + 1; p < win->line + win->lines - 1; p++)
		mvprintw(p, win->col, "%*s", win->cols, "");

	attrset(COLOR_PAIR(3));
	move(win->line + win->lines - 1, win->col);
	addstr(fileview.selPath);
	ersline(win->col + win->cols);
	focus_y = win->line + win->lines - 1;
	focus_x = win->col + fileview.cursor;
	return 0;
}

int
fileviewer_type(struct window *win, const char *str, U32 nStr)
{
	if(iscntrl(*str))
		return 1;
	if(strlen(fileview.selPath) + nStr >= sizeof(fileview.selPath))
		return 1;
	memcpy(fileview.selPath + fileview.cursor + nStr, fileview.selPath + fileview.cursor, strlen(fileview.selPath + fileview.cursor) + 1);
	memcpy(fileview.selPath + fileview.cursor, str, nStr);
	fileview.cursor += nStr;
	return 0;
}

bool
fileviewer_bindcall(struct window *win, struct binding_call *bc, I32 param)
{
	switch(bc->type) {
	case BIND_CALL_CHOOSE: {
		struct file_info fi;
		if(!getfileinfo(&fi, fileview.selPath)) {
			if(fi.type == FI_TYPE_DIR)
				; // TODO: (un/)collapse folder
			else if(fi.type == FI_TYPE_REG) {
				struct buffer *buf;
				struct window *newWin;

				buf = buffer_new(fileview.selPath);
				if(!buf)
					break;
				newWin = edit_new(buf, edit_statesfromfiletype(fileview.selPath));
				if(newWin) {
					window_copylayout(win, newWin);
					window_delete(win);
					return true;
				}
			}
		}
		break;
	}
	}
	if(win->bindMode->flags & FBIND_MODE_TYPE) {
		switch(bc->type) {
		case BIND_CALL_MOVEHORZ: {
			const I32 p = utf8_cnvdist(fileview.selPath, strlen(fileview.selPath), fileview.cursor, param);
			fileview.cursor += p;
			return p == param;
		}
		case BIND_CALL_DELETE: {
			const U32 n = strlen(fileview.selPath);
			const I32 p = utf8_cnvdist(fileview.selPath, n, fileview.cursor, param);
			if(p < 0) {
				memmove(fileview.selPath + fileview.cursor + p, fileview.selPath + fileview.cursor, n + 1 - fileview.cursor);
				fileview.cursor += p;
			} else {
				memmove(fileview.selPath + fileview.cursor, fileview.selPath + fileview.cursor + p, n + 1 - fileview.cursor - p);
			}
			return p == param;
		}
		}
	} else {
		switch(bc->type) {
		case BIND_CALL_MOVEVERT:
			if((!fileview.selected && param < 0) ||
					(fileview.selected == fileview.maxSelected && param >= 0))
				return false;
			fileview.selected = SAFE_ADD(fileview.selected, param);
			if(fileview.selected < 0) {
				fileview.selected = 0;
				fileview.scroll = 0;
				break;
			}
			if(fileview.selected > fileview.maxSelected) {
				fileview.selected = fileview.maxSelected;
				fileview.scroll = MAX(fileview.maxSelected - win->lines + 3, 0);
				break;
			}
			if(fileview.selected < fileview.scroll)
				fileview.scroll = fileview.selected;
			else if(fileview.selected - fileview.scroll >= win->lines - 3)
				fileview.scroll = fileview.selected - win->lines + 3;
			return true;
		case BIND_CALL_TOGGLEHIDDEN:
			fileview.flags ^= FFILEVIEW_SHOWHIDDEN;
			return true;
		case BIND_CALL_TOGGLESORTTYPE:
			fileview.flags ^= FFILEVIEW_SORTTYPE; break;
			return true;
		case BIND_CALL_SORTALPHABETICAL:
			fileview.flags |= FFILEVIEW_SORTALPHA;
			fileview.flags &= ~(FFILEVIEW_SORTMODIFY | FFILEVIEW_SORTCHANGE);
			return true;
		case BIND_CALL_SORTMODIFICATIONTIME:
			fileview.flags |= FFILEVIEW_SORTMODIFY;
			fileview.flags &= ~(FFILEVIEW_SORTALPHA | FFILEVIEW_SORTCHANGE);
			return true;
		case BIND_CALL_SORTCHANGETIME:
			fileview.flags |= FFILEVIEW_SORTCHANGE;
			fileview.flags &= ~(FFILEVIEW_SORTALPHA | FFILEVIEW_SORTMODIFY);
			return true;
		}
	}
	return false;
}
