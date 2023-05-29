#include "cnergy.h"
#include <dirent.h>
#include <unistd.h>

enum {
	FFILEVIEW_USECURSOR = 1 << 0,
	FFILEVIEW_SORTALPHA = 1 << 1,
	FFILEVIEW_SORTMODIFY = 1 << 2,
	FFILEVIEW_SORTCHANGE = 1 << 3,
	FFILEVIEW_SHOWHIDDEN = 1 << 4,
	FFILEVIEW_SORTTYPE = 1 << 5,
};
static struct fileview {
	U32 flags;
	U32 selected;
	U32 scroll;
	U32 height; // total height of the render
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
	//qsort(names, sizeof(*names), nNames, files_compare);
	return 0;
}

int
file_viewer_render(struct window *win)
{
	const char *name;
	int y = 0;
	getcwd(fileview.basePath, PATH_MAX);
	strcpy(fileview.curPath, fileview.basePath);
	fileview_recurseinto();
	while((name = fileview_nextentry())) {
		struct file_info fi;
		const size_t nName = strlen(name);
		const size_t nOld = strlen(fileview.curPath);
		if(nOld + nName + 1 >= PATH_MAX)
			goto draw_broken_item;
		fileview.curPath[nOld] = '/';
		strcpy(fileview.curPath + nOld + 1, name);
		getfileinfo(&fi, fileview.curPath);
		if(fi.type == FI_TYPE_DIR)
			fileview_recurseinto();
		else
			fileview.curPath[nOld] = 0;
		attrset(COLOR_PAIR(3));
		mvaddstr(win->line + y, win->col, name);
		y++;
		continue;
	draw_broken_item:
		attrset(COLOR_PAIR(0));
		mvaddstr(win->line + y, win->col, name);
		y++;
	}
	return 0;
}


bool file_viewer_bindcall(struct window *win, struct binding_call *bc, int param)
{
	return false;
}
