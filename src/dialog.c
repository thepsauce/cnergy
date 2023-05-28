#include "cnergy.h"

static void
drawframe(const char *title, int y, int x, int ny, int nx)
{
	mvhline(y, x, ACS_HLINE, nx);
	mvhline(y + ny - 1, x, ACS_HLINE, nx);
	mvvline(y, x, ACS_VLINE, ny);
	mvvline(y, x + nx - 1, ACS_VLINE, ny);
	// draw corners
	mvaddch(y, x, ACS_ULCORNER);
	mvaddch(y, x + nx - 1, ACS_URCORNER);
	mvaddch(y + ny - 1, x, ACS_LLCORNER);
	mvaddch(y + ny - 1, x + nx - 1, ACS_LRCORNER);
	// draw title
	mvaddstr(y, x + 3, title);
	// erase content
	for(ny--, nx--; --ny;)
	for(int n = 1; n < nx; n++)
		mvaddch(y + ny, x + n, ' ');
}

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
	char workingDir[PATH_MAX];
	char selectedPath[PATH_MAX];
	char path[PATH_MAX];
	int scroll;
	int height;
	int selected;
	U32 cursor; // cursor within path
	bool useCursor; // you can switch modes with TAB
	char **collapsedPaths;
	U32 nCollapsedPaths;
	struct fileview_stack {
		U32 stop;
		char **dirs;
		U32 nDirs;
		U32 iDir;
	} *stack;
	U32 depth; // nStack - 1
} fileview;

enum {
	FILETYPE_FOLDER,
	FILETYPE_NORMAL,
	FILETYPE_EXEC,
	FILETYPE_OTHER,
};

// TODO: Maybe create filesystem.c
int
getfiletime(const char *file, time_t *pTime)
{
	struct stat statbuf;
	if(stat(file, &statbuf) < 0)
		return -1;
	*pTime = statbuf.st_mtime;
	return 0;
}

static inline int
extractfiletype(struct stat *statbuf)
{
	bool isDir, isReg, isExec;

	isDir = S_ISDIR(statbuf->st_mode);
	isReg = S_ISREG(statbuf->st_mode);
	isExec = isReg && (statbuf->st_mode & 0111);
	return isDir ? FILETYPE_FOLDER : isExec ? FILETYPE_EXEC : isReg ? FILETYPE_NORMAL : FILETYPE_OTHER;
}

static int
files_compare(const void *a, const void *b)
{
	const char *n1, *n2;
	char *path1, *path2;
	U32 nPath, nPath1, nPath2;
	struct stat sb1, sb2;
	int cmp = 0;

	nPath = strlen(fileview.path);

	n1 = *(const char *const *) a;
	nPath1 = strlen(n1);
	path1 = malloc(nPath + 1 + nPath1 + 1);
	memcpy(path1, fileview.path, nPath);
	path1[nPath] = '/';
	strcpy(path1 + nPath + 1, n1);

	n2 = *(const char *const *) b;
	nPath2 = strlen(n2);
	path2 = malloc(nPath + 1 + nPath2 + 1);
	memcpy(path2, fileview.path, nPath);
	path2[nPath] = '/';
	strcpy(path2 + nPath + 1, n2);

	if(path1)
		lstat(path1, &sb1);
	if(path2)
		lstat(path2, &sb2);
	if(fileview.flags & FFILEVIEW_SORTALPHA)
		cmp = strcasecmp(n1, n2);
	else if(fileview.flags & FFILEVIEW_SORTMODIFY)
		cmp = sb1.st_mtime - sb2.st_mtime;
	else if(fileview.flags & FFILEVIEW_SORTCHANGE)
		cmp = sb1.st_ctime - sb2.st_ctime;
	if(fileview.flags & FFILEVIEW_SORTTYPE) {
		const int t1 = extractfiletype(&sb1);
		const int t2 = extractfiletype(&sb2);
		if(t1 != t2)
			cmp = t1 - t2;
	}
	free(path1);
	free(path2);	
	return cmp;
}

static inline int
fileview_prepareframe(U32 stop)
{
	DIR *dir;
	struct dirent *ent;
	char **dirs;
	char **newDirs;
	U32 nDirs;
	char *name;

	fileview.stack[fileview.depth].stop = stop;
	fileview.stack[fileview.depth].iDir = 0;
	fileview.stack[fileview.depth].nDirs = 0;
	dir = opendir(fileview.path);
	if(!dir)
		return -1;
	dirs = NULL;
	newDirs = NULL;
	nDirs = 0;
	dirs = malloc(sizeof(*dirs) * 6);
	if(!dirs) {
		closedir(dir);
		return -1;
	}
	while((ent = readdir(dir))) {
		newDirs = realloc(dirs, sizeof(*dirs) * (nDirs + 1));
		if(!newDirs)
			break;
		dirs = newDirs;
		name = const_alloc(ent->d_name, strlen(ent->d_name) + 1);
		if(!name)
			break;
		dirs[nDirs++] = name;
	}
	closedir(dir);
	qsort(dirs, nDirs, sizeof(*dirs), files_compare);
	fileview.stack[fileview.depth].dirs = dirs;
	fileview.stack[fileview.depth].nDirs = nDirs;
	return 0;
}

static inline int
fileview_init(void)
{
	struct fileview_stack *newStack;
	char **newCollapsedPaths;

	newStack = realloc(fileview.stack, sizeof(*fileview.stack) * 4);
	if(!newStack)
		return -1;
	fileview.stack = newStack;
	newCollapsedPaths = realloc(fileview.collapsedPaths, sizeof(*fileview.collapsedPaths) * 10);
	if(!newCollapsedPaths)
		return -1;
	fileview.collapsedPaths = newCollapsedPaths;
	fileview.depth = 0;
	strcpy(fileview.path, fileview.workingDir);
	return fileview_prepareframe(0);
}

static inline U32
fileview_append(const char *name)
{
	const U32 len = strlen(fileview.path);
	if(len + 1 + strlen(name) >= sizeof(fileview.path))
		return 0; // path is too long
	fileview.path[len] = '/';
	strcpy(fileview.path + len + 1, name);
	return len;
}

static inline int
fileview_enter(const char *name)
{
	// go down the stack
	fileview.depth++;
	fileview.stack = realloc(fileview.stack, sizeof(*fileview.stack) * (fileview.depth + 1));
	if(!fileview.stack)
		return -1;
	const U32 len = fileview_append(name);
	if(!len)
		return -1;
	return fileview_prepareframe(len);
}

static inline bool
isdir(const char *path)
{
	struct stat statbuf;
	if(lstat(path, &statbuf) < 0)
		return false;
	return S_ISDIR(statbuf.st_mode);
}

static inline int
fileview_collapse(const char *path)
{
	char real[PATH_MAX + 1];
	char **newCollapsedPaths;
	char *pathDup;

	realpath(path, real);
	for(U32 i = 0; i < fileview.nCollapsedPaths; i++)
		if(!strcmp(real, fileview.collapsedPaths[i])) {
			fileview.nCollapsedPaths--;
			memmove(fileview.collapsedPaths + i, fileview.collapsedPaths + i + 1, sizeof(*fileview.collapsedPaths) * (fileview.nCollapsedPaths - i));
			return 0;
		}
	newCollapsedPaths = realloc(fileview.collapsedPaths, sizeof(*fileview.collapsedPaths) * (fileview.nCollapsedPaths + 1));
	if(!newCollapsedPaths)
		return -1;
	pathDup = const_alloc(real, strlen(real) + 1);
	if(!pathDup)
		return -1;
	fileview.collapsedPaths = newCollapsedPaths;
	fileview.collapsedPaths[fileview.nCollapsedPaths++] = pathDup;
	return 0;
}

static inline bool
fileview_iscollapsed(const char *realPath)
{
	for(U32 i = 0; i < fileview.nCollapsedPaths; i++)
		if(!strcmp(fileview.collapsedPaths[i], realPath))
			return true;
	return false;
}

static inline void
fileview_leave(void)
{
	free(fileview.stack[fileview.depth].dirs);
	// cut the end off the path
	fileview.path[fileview.stack[fileview.depth].stop] = 0;
	// go up the stack
	fileview.depth--;
}

static inline const char *
fileview_nextentry(void)
{
	const char *name;
	do {
		while(fileview.stack[fileview.depth].iDir == fileview.stack[fileview.depth].nDirs) {
			if(!fileview.depth)
				return NULL;
			fileview_leave();
		}
		name = fileview.stack[fileview.depth].dirs[fileview.stack[fileview.depth].iDir++];
	} while((!(fileview.flags & FFILEVIEW_SHOWHIDDEN) && name[0] == '.')
		   	|| !strcmp(name, ".") || !strcmp(name, ".."));
	return name;
}

const char *
choosefile(void)
{
	const char *ent_name;
	int line, col;
	int lines, cols;
	int y;
	struct stat statbuf;

	line = LINES / 5;
	col = COLS / 5;
	lines = LINES * 3 / 5;
	cols = COLS * 3 / 5;
	attrset(COLOR_PAIR(3));
	drawframe("Choose file", line, col, lines, cols);
	if(!fileview.workingDir[0])
		getcwd(fileview.workingDir, sizeof(fileview.workingDir));
	mvaddstr(line + 1, col + 1, fileview.workingDir);
	line += 2;
	col++;
	cols -= 2;
	lines -= 4;
	while(1) {
		y = line - fileview.scroll;
		// draw file tree
		if(fileview_init() < 0)
			return NULL;
		fileview.height = 0;
		while((ent_name = fileview_nextentry())) {
			char path[PATH_MAX + 1];
			char real[PATH_MAX + 1];
			int type;
			bool isCollapsed;
			bool isSelected = false;
			int rHighlight = 0;

			const U32 l = strlen(fileview.path);
			strcpy(path, fileview.path);
			path[l] = '/';
			strcpy(path + l + 1, ent_name);
			realpath(path, real);
			lstat(path, &statbuf);
			if(fileview.selected == fileview.height && !(fileview.flags & FFILEVIEW_USECURSOR)) {
				strcpy(fileview.selectedPath, path);
				fileview.cursor = l + 1 + strlen(ent_name);
				isSelected = true;
			}
			if(fileview.flags & FFILEVIEW_USECURSOR) {
				char real2[PATH_MAX + 1];
				realpath(fileview.selectedPath, real2);
				if(!strcmp(real, real2)) {
					fileview.selected = fileview.height;
					isSelected = true;
				} else if(!strncmp(real, real2, strlen(real2))) {
					rHighlight = strlen(real) - strlen(real2);
					if(rHighlight < strlen(ent_name)) {
						rHighlight = strlen(ent_name) - rHighlight;
					} else
						rHighlight = 0;
				}
			}
			fileview.height++;
			type = extractfiletype(&statbuf);
			isCollapsed = type == FILETYPE_FOLDER && fileview_iscollapsed(real);
			if(y >= line && y < line + lines) {
				attrset(COLOR(type == FILETYPE_FOLDER ? 4 : type == FILETYPE_NORMAL ? 7 : 3, isSelected ? 8 : 0));
				move(y, col + 1);
				printw("%*s", fileview.depth * 2, "");
				attron(A_BOLD);
				addnstr(ent_name, rHighlight);
				attroff(A_BOLD);
				addstr(ent_name + rHighlight);
				if(type == FILETYPE_EXEC)
					addch('*');
				if(type == FILETYPE_FOLDER)
					addch('/');
				ersline(col + cols);
			}
			if(isCollapsed)
				// go deeper
				fileview_enter(ent_name);
			y++;
		}
		// erase rest
		attrset(0);
		for(; y < line + lines; y++)
		for(int x = col + 1; x < col + cols; x++)
			mvaddch(y, x, ' ');
		attrset(COLOR_PAIR(3));
		mvaddstr(line + lines, col, fileview.selectedPath);
		ersline(col + cols);
		if(fileview.useCursor)
			move(line + lines, col + fileview.cursor);
		else
			move(line + fileview.selected - fileview.scroll, col);
		const int c = getch();
		if(c == 0x1b) // escape
			return NULL;
		if(c == '\n') {
			if(!isdir(fileview.selectedPath))
				return fileview.selectedPath;
			fileview_collapse(fileview.selectedPath);
		}
		if(fileview.flags & FFILEVIEW_USECURSOR) {
			switch(c) {
			case '\t': fileview.useCursor = false; break;
			case KEY_LEFT: if(fileview.cursor) fileview.cursor--; break;
			case KEY_RIGHT: if(fileview.cursor != strlen(fileview.selectedPath)) fileview.cursor++; break;
			case KEY_HOME: fileview.cursor = 0; break;
			case KEY_END: fileview.cursor = strlen(fileview.selectedPath); break;
			case KEY_BACKSPACE:
				if(!fileview.cursor)
					break;
				fileview.cursor--;
			/* fall through */
			case KEY_DC:
				memcpy(fileview.selectedPath + fileview.cursor, fileview.selectedPath + fileview.cursor + 1, strlen(fileview.selectedPath + fileview.cursor));
				break;
			default:
				if(isprint(c)) {
					if(strlen(fileview.selectedPath) == sizeof(fileview.selectedPath))
						break;
					memcpy(fileview.selectedPath + fileview.cursor + 1, fileview.selectedPath + fileview.cursor, strlen(fileview.selectedPath + fileview.cursor) + 1);
					fileview.selectedPath[fileview.cursor++] = c;
				}
			}
		} else {
			switch(c) {
			case '\t': fileview.flags ^= FFILEVIEW_USECURSOR; break;
			case 'I': case 'h': fileview.flags ^= FFILEVIEW_SHOWHIDDEN; break;
			case 'S': case 's': fileview.flags ^= FFILEVIEW_SORTALPHA; fileview.flags &= ~(FFILEVIEW_SORTMODIFY | FFILEVIEW_SORTCHANGE); break;
			case 'O': case 't': fileview.flags ^= FFILEVIEW_SORTTYPE; break;
			case 'U': fileview.flags ^= FFILEVIEW_SORTMODIFY; fileview.flags &= ~(FFILEVIEW_SORTALPHA | FFILEVIEW_SORTCHANGE); break;
			case 'P': fileview.flags ^= FFILEVIEW_SORTCHANGE; fileview.flags &= ~(FFILEVIEW_SORTALPHA | FFILEVIEW_SORTMODIFY); break;
			case KEY_HOME: case 'g':
				fileview.selected = 0;
				fileview.scroll = 0;
				break;
			case KEY_END: case 'G':
				fileview.selected = fileview.height - 1;
				fileview.scroll = MAX(fileview.height - (lines - 3), 0);
				break;
			case KEY_UP: case 'k':
				if(!fileview.selected)
					break;
				fileview.selected--;
				if(fileview.selected - fileview.scroll < 0)
					fileview.scroll--;
				break;
			case KEY_DOWN: case 'j':
				if(fileview.selected + 1 == fileview.height)
					break;
				fileview.selected++;
				if(fileview.selected - fileview.scroll >= lines - 3)
					fileview.scroll++;
				break;
			}
		}
	}
	return NULL;
}

int
messagebox(const char *title, const char *msg, ...)
{
	va_list l;
	const char *opt;
	const char *storedMsg;
	char buf[88];
	char options[8];
	int nOption = 0;
	int defaultOption = -1;
	int line, col;
	int lines, cols;
	int y, x;
	int maxY, maxX;

	line = LINES / 5;
	col = COLS / 5;
	lines = LINES * 3 / 5;
	cols = COLS * 3 / 5;
	drawframe(title, line, col, lines, cols);
	// draw message
	y = line + lines / 5;
	x = col + cols / 5;
	maxY = line + lines * 4 / 5;
	maxX = col + cols * 4 / 5;
	move(y, x);
	// for format
	va_start(l, msg);
	while(1) {
		char *space;
		U32 n;

		while(!*msg) {
			if(!storedMsg)
				break;
			msg = storedMsg;
			storedMsg = NULL;
		}
		// intepret %
		if(*msg == '%') {
			msg++;
			switch(*msg) {
			case 0: msg--; break;
			case 's':
				storedMsg = msg;
				msg = va_arg(l, const char*);
				break;
			case 'u':
				storedMsg = msg;
				snprintf(buf, sizeof(buf), "%u", va_arg(l, U32));
				msg = buf;
				break;
			case 'd':
				storedMsg = msg;
				snprintf(buf, sizeof(buf), "%d", va_arg(l, I32));
				msg = buf;
				break;
			}
		}
		// break words at spaces
		space = strchr(msg, ' ');
		n = space ? space - msg : strlen(msg);
		if(x + n >= maxX && x != col + cols / 5) {
			x = col + cols / 5;
			y++;
			if(y == maxY)
				break;
			move(y, x);
		}
		n = MIN(n, maxX - x);
		addnstr(msg, n);
		if(space && x + n < maxX) {
			addch(' ');
			x++;
		}
		if(space)
			msg++;
		msg += n;
		x += n;
	}

	move(line + lines - 1, col + 3);
	while((opt = va_arg(l, const char*))) {
		const char *b = strchr(opt, '[');
		if(!b || nOption == ARRLEN(options))
			return -1;
		if(b[1] == '*') {
			defaultOption = nOption;
			b++;
		}
		options[nOption++] = b[1];
		printw("%s ", opt);
	}
	va_end(l);

	const int choice = tolower(getch());
	for(U32 i = 0; i < nOption; i++)
		if(choice == tolower(options[i]))
			return i;
	return defaultOption;
}

void *
safe_alloc(U32 sz)
{
	void *ptr;

	do {
		ptr = malloc(sz);
	} while(!ptr && !messagebox("Memory error", 
				"Your system returned NULL when trying to allocate %u bytes of memory, what do you want to do?", 
				sz, "[T]ry again", "[C]ancel", NULL));
	return ptr;
}

void *
safe_realloc(void *ptr, U32 newSz)
{
	do {
		ptr = realloc(ptr, newSz);
	} while(!ptr && !messagebox("Memory error", 
				"Your system returned NULL when trying to allocate %u bytes of memory, what do you want to do?", 
				newSz, "[T]ry again", "[C]ancel", NULL));
	return ptr;
}



