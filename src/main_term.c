#ifndef GUI

#include "cnergy.h"
#include <locale.h>

void print_modes(struct binding_mode *mode);

void getfilepos(fileid_t file, long pos, int *line, int *col)
{
	FILE *const fp = fc_open(file, "rb");
	if(!fp)
		return;
	int ln = 1, cl = 1;
	while(ftell(fp) != pos) {
		const int c = fgetc(fp);
		if(c == '\n') {
			ln++;
			cl = 1;
		} else
			cl++;
	}
	fclose(fp);
	*line = ln;
	*col = cl;
}

void drawframe(const char *title, int y, int x, int ny, int nx);

int
main(int argc, char **argv)
{
	(void) argc;
	(void) argv;

	setlocale(LC_ALL, "");

	// all below here is pretty much test code beside the main loop
	struct parser parser;
	memset(&parser, 0, sizeof(parser));
	if(parser_run(&parser, fc_cache(fc_getbasefile(), "cng/draft.cng")) == SUCCESS) {
		if(parser.nErrors == 0) {
			for(unsigned i = 0; i < parser.nAppendRequests; i++) {
				struct binding_mode *m;

				for(m = parser.firstModes[parser.appendRequests[i].windowType]; m; m = m->next)
					if(!strcmp(m->name, parser.appendRequests[i].donor))
						break;
				if(!m)
					continue; // TODO: print error
				const struct binding_mode *const donor = m;
				struct binding_mode *const receiver = parser.appendRequests[i].mode;
				receiver->bindings = realloc(receiver->bindings,
						sizeof(*receiver->bindings) * (receiver->nBindings + donor->nBindings));
				// TODO: add null check and abort
				memcpy(receiver->bindings + receiver->nBindings,
						donor->bindings,
						sizeof(*donor->bindings) * donor->nBindings);
				receiver->nBindings += donor->nBindings;
			}
			for(window_type_t i = 0; i < WINDOW_MAX; i++) {
				for(struct binding_mode *m = parser.firstModes[i]; m; m = m->next) {
					const char *strs[] = { "all", "edit", "bufferviewer", "fileviewer" };
					printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
					printf("MODE: %s::%s (%#x)(%u bindings)\n", strs[i], m->name, m->flags, m->nBindings);
					printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
					for(unsigned j = 0; j < m->nBindings; j++) {
						const struct binding bind = m->bindings[j];
							printf("  BIND([%p]%zu): ", (void*) bind.keys, bind.nKeys);
						fflush(stdout);
						for(char *k = bind.keys, *e = k + bind.nKeys;;) {
							for(;;) {
								const unsigned l = utf8_len(k, e - k);
								const int uc = utf8_tounicode(k, e - k);
								if(uc >= 0xf6b1 && uc <= 0xf6b1 + 0xff)
									printf("<%s>", keyname(uc - 0xf6b1 + 0xff) + 4);
								else if(uc <= 0x7f)
									printf("%s", keyname(uc));
								else
									printf("%.*s", l, k);
								k += l;
								if(!*k)
									break;
							}
							k++;
							if(k == e)
								break;
							printf(", ");
						}
						putchar('\n');
						printf("    ");
						for(size_t sz = 0; sz < bind.szProgram; sz++) {
							const uint8_t b = *(uint8_t*) (bind.program + sz);
							const uint8_t
								l = b & 0xf,
								h = b >> 4;
							printf("0x");
							putchar(h >= 0xa ? (h - 0xa + 'a') : h + '0');
							putchar(l >= 0xa ? (l - 0xa + 'a') : l + '0');
							putchar(' ');
						}
						printf("\n========\n");
						environment_loadandprint(bind.program, bind.szProgram);
						printf("========");
						putchar('\n');
					}
				}
			}
			modes_add(parser.firstModes);
		}
		parser_cleanup(&parser);
	}
	int line, col;
	for(unsigned j = 0; j < parser.nErrStack; j++) {
		char path[PATH_MAX];
		struct parser_token tok = parser.errStack[j].token;
		fc_getrelativepath(fc_getbasefile(), tok.file, path, sizeof(path));
		getfilepos(tok.file, tok.pos, &line, &col);
		printf("error in %s:%d:%d at token %c: %u\n", path, line, col, tok.type, parser.errStack[j].err);
	}
	if(parser.nErrors != 0)
		return -1;

	initscr();

	raw();
	noecho();
	keypad(stdscr, true);
	scrollok(stdscr, false);
	clearok(stdscr, false);
	set_tabsize(4);

	if(has_colors()) {
		static const uint8_t gruvbox_colors[16][3] = {
			{ 35, 35, 35 },   // 1: Black
			{ 204, 36, 29 },  // 2: Red
			{ 152, 151, 26 }, // 3: Green
			{ 215, 153, 33 }, // 4: Yellow
			{ 69, 133, 136 }, // 5: Blue
			{ 177, 98, 134 }, // 6: Magenta
			{ 104, 157, 106 }, // 7: Cyan
			{ 184, 184, 184 }, // 8: Light gray
			{ 70, 69, 69 },   // 9: Dark gray
			{ 251, 73, 52 },  // 10: Bright red
			{ 184, 187, 38 }, // 11: Bright green
			{ 250, 189, 47 }, // 12: Bright yellow
			{ 131, 165, 152 }, // 13: Bright blue
			{ 211, 134, 155 }, // 14: Bright magenta
			{ 108, 181, 131 }, // 15: Bright cyan
			{ 254, 254, 254 } // 16: White
		};
		start_color();
		for(unsigned i = 0; i < ARRLEN(gruvbox_colors); i++)
			init_color(i,
					(int) gruvbox_colors[i][0] * 1000 / 256,
					(int) gruvbox_colors[i][1] * 1000 / 256,
					(int) gruvbox_colors[i][2] * 1000 / 256);
		for(int i = 0; i < 16; i++)
		for(int j = 0; j < 16; j++)
			init_pair(1 + i + j * 16, i, j);
	}

#define COLOR(bg, fg) COLOR_PAIR(1 + ((bg) | ((fg) * 16)))
	// make default settings
	main_environment.settings[SET_TABSIZE] = 4;
	main_environment.settings[SET_COLOR_LINENR_FOCUS] = COLOR(11, 8);
	main_environment.settings[SET_COLOR_LINENR] = COLOR(3, 8);
	main_environment.settings[SET_COLOR_ENDOFBUFFER] = COLOR(6, 0);
	main_environment.settings[SET_COLOR_STATUSBAR1_FOCUS] = COLOR(14, 8);
	main_environment.settings[SET_COLOR_STATUSBAR2_FOCUS] = COLOR(11, 8);
	main_environment.settings[SET_COLOR_STATUSBAR1] = COLOR(6, 8);
	main_environment.settings[SET_COLOR_STATUSBAR2] = COLOR(3, 8);
	main_environment.settings[SET_COLOR_ITEM] = COLOR(3, 0);
	main_environment.settings[SET_COLOR_ITEMSELECTED] = COLOR(3, 8);
	main_environment.settings[SET_COLOR_DIRSELECTED] = COLOR(6, 8);
	main_environment.settings[SET_COLOR_FILESELECTED] = COLOR(7, 8);
	main_environment.settings[SET_COLOR_EXECSELECTED] = COLOR(3, 8);
	main_environment.settings[SET_COLOR_DIR] = COLOR(6, 0);
	main_environment.settings[SET_COLOR_FILE] = COLOR(7, 0);
	main_environment.settings[SET_COLOR_EXEC] = COLOR(3, 0);
	main_environment.settings[SET_COLOR_BROKENFILE] = A_REVERSE | COLOR(2, 0);
	main_environment.settings[SET_CHAR_DIR] = '/';
	main_environment.settings[SET_CHAR_EXEC] = '*';

	char *const keys = (char*) main_environment.memory;

	// setup some windows for testing
	const char *files[] = {
		"src/main_term.c",
		"cng/edit.cng",
		//"include/cnergy.h",
		//"src/bind.c",
	};
	for(unsigned i = 0; i < ARRLEN(files); i++) {
		const char *file = files[i % ARRLEN(files)];
		const fileid_t fileid = fc_cache(fc_getbasefile(), file);
		const bufferid_t bid = buffer_new(fileid);
		edit_new(bid);
	}
	all_windows[0].right = 1;
	all_windows[1].left = 0;

	// uncomment this code to add a fileviewer
	/*struct window *w = window_new(WINDOW_FILEVIEWER);
	windows[2]->below = w;
	w->above = windows[2];*/

	/* stack pointer grows downwards */
	main_environment.sp = sizeof(main_environment.memory);
	while(1) {
		int c;
		int nextLine, nextCol, nextLines, nextCols;
		bool isPreDigit;

		/* -1 to reserve a line to show the global status bar */
		nextLine = 0;
		nextCol = 0;
		nextLines = LINES - 1;
		nextCols = COLS;
		/* find all windows that are layout origins and render them */
		for(windowid_t i = 0; i < n_windows; i++) {
			struct window *const w = all_windows + i;
			if(w->flags.dead)
				continue;
			if(w->left == ID_NULL && w->above == ID_NULL) {
				if(nextLine)
					drawframe("", nextLine - 1, nextCol - 1, nextLines + 2, nextCols + 2);
				w->line = nextLine;
				w->col = nextCol;
				w->lines = nextLines;
				w->cols = nextCols;
				window_layout(i);
				environment_call(CALL_RENDER);
				nextLine += nextLines / 6;
				nextCol += nextCols / 6;
				nextLines = nextLines * 4 / 6;
				nextCols = nextCols * 4 / 6;
			}
		}
		if(window_getbindmode(focus_window)->flags & FBIND_MODE_SELECTION)
			curs_set(0);
		else {
			curs_set(1);
			move(focus_y, focus_x);
		}
		c = getch();
		if(c == 0x03) /* ^C */
			break;
		switch(c) {
		case 0x7f: /* delete */
			c = KEY_BACKSPACE;
			break;
		case 0x1b: /* escape */
			/* if we have anything already, discard it,
			 * meaning that escape cannot be repeated or be part of a bind
			 * it can only be the start of a bind
			 */
			if(main_environment.C || main_environment.mp) {
				main_environment.C = 0;
				main_environment.mp = 0;
				continue;
			}
			break;
		}
		isPreDigit = !(window_getbindmode(focus_window)->flags & FBIND_MODE_TYPE) &&
				isdigit(c) && (c != '0' || main_environment.C);
		main_environment.A = (ptrdiff_t) (main_environment.memory + main_environment.mp);
		if(c > 0xff) {
			c = SPECIAL_KEY(c);
			main_environment.mp += utf8_convunicode(c,
					main_environment.memory + main_environment.mp);
		} else if(!isPreDigit) {
			keys[main_environment.mp++] = c;
			while(c & 0x80) {
				c = getch();
				keys[main_environment.mp++] = c;
			}
		}
		keys[main_environment.mp] = 0;
		if((window_getbindmode(focus_window)->flags & FBIND_MODE_TYPE) &&
				(isprint(c) || isspace(c)))
			environment_call(CALL_TYPE);
		/* either append the digit to the number or try to execute a bind
		 * render status bar
		 */
		attrset(COLOR(3, 0));
		move(LINES - 1, 0);
		if(isPreDigit) {
			main_environment.C = SAFE_MUL(main_environment.C, 10);
			main_environment.C = SAFE_ADD(main_environment.C, c - '0');
		} else {
			struct binding *bind;

			main_environment.mp++;
			switch(bind_find(keys, &bind)) {
			case 0:
				if(main_environment.C == 0)
					main_environment.C = 1;
				environment_loadandexec(bind->program, bind->szProgram);
			/* fall through */
			case 2:
				main_environment.mp = 0;
				main_environment.C = 0;
				break;
			case 1:
				main_environment.mp--;
				break;
			}
		}
		printw("%s ", window_getbindmode(focus_window)->name);
		if(main_environment.C)
			printw("%zd", main_environment.C);
		for(unsigned i = 0; i < main_environment.mp; i++) {
			const char *const utf8 = (char*) main_environment.memory + i;
			const unsigned l = utf8_len(utf8, main_environment.mp);
			const int uc = utf8_tounicode(utf8, main_environment.mp - i);
			if(uc >= 0xf6b1 && uc <= 0xf6b1 + 0xff)
				printw("<%s>", keyname(uc - 0xf6b1 + 0xff) + 4);
			else if(uc <= 0x7f)
				printw("%s", keyname(uc));
			else
				addnstr((char*) main_environment.memory + i, l);
			i += l - 1;
		}
		ersline(COLS - getcurx(stdscr));
	}
	endwin();
	printf("^C exit\n");
	return 0;
}

#endif
