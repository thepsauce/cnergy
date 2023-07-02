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
				struct binding_mode *r, *d;

				r = parser_findbindmode(&parser, parser.appendRequests[i].receiver);
				d = parser_findbindmode(&parser, parser.appendRequests[i].donor);
				if(!d)
					exit(-3);
				mode_addallbinds(r, d);

			}
			debug_modes_print(parser.modes, parser.nModes);
			for(unsigned i = 0; i < parser.nModes; i++) {
				struct binding_mode *m;

				m = mode_new(parser.modes[i].name);
				if(!m)
					exit(-1);
				mode_addallbinds(m, parser.modes + i);
			}
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
	start_color();
	endwin();

	// make default settings
	main_environment.settings[SET_TABSIZE] = 4;
	main_environment.settings[SET_COLOR_LINENR_FOCUS] = 0;
	main_environment.settings[SET_COLOR_LINENR] = 0;
	main_environment.settings[SET_COLOR_ENDOFBUFFER] = 0;
	main_environment.settings[SET_COLOR_STATUSBAR1_FOCUS] = 0;
	main_environment.settings[SET_COLOR_STATUSBAR2_FOCUS] = 0;
	main_environment.settings[SET_COLOR_STATUSBAR1] = 0;
	main_environment.settings[SET_COLOR_STATUSBAR2] = 0;
	main_environment.settings[SET_COLOR_ITEM] = 0;
	main_environment.settings[SET_COLOR_ITEMSELECTED] = 0;
	main_environment.settings[SET_COLOR_DIRSELECTED] = 0;
	main_environment.settings[SET_COLOR_FILESELECTED] = 0;
	main_environment.settings[SET_COLOR_EXECSELECTED] = 0;
	main_environment.settings[SET_COLOR_DIR] = 0;
	main_environment.settings[SET_COLOR_FILE] = 0;
	main_environment.settings[SET_COLOR_EXEC] = 0;
	main_environment.settings[SET_COLOR_BROKENFILE] = A_REVERSE | 0;
	main_environment.settings[SET_CHAR_DIR] = '/';
	main_environment.settings[SET_CHAR_EXEC] = '*';

	char *const keys = (char*) main_environment.memory;

	// setup some pages for testing
	const char *files[] = {
		"src/main_term.c",
		"cng/edit.cng",
		//"include/cnergy.h",
		//"src/bind.c",
	};
	for(unsigned i = 0; i < 1; i++) {
		const char *file = files[i % ARRLEN(files)];
		const fileid_t fileid = fc_cache(fc_getbasefile(), file);
		const bufferid_t bid = buffer_new(fileid);
		const pageid_t pid = page_new(PAGE_TYPE_EDIT);
		struct page_edit *page = (struct page_edit*) (all_pages + pid);
		page->bufid = bid;
	}

	/* stack pointer grows downwards */
	main_environment.sp = sizeof(main_environment.memory);
	while(1) {
		int c;
		bool isPreDigit;
		struct binding_mode *mode;

		/* -1 to reserve a line to show the global status bar */
		page_displayall(0, 0, COLS, LINES - 1);
		mode = comp_getbindmode(focus_comp);
		if(mode_isvisual(mode)) {
			curs_set(0);
		} else {
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
		isPreDigit = !mode_isinsert(mode) && isdigit(c) &&
			(c != '0' || main_environment.C);
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
		if(mode_isinsert(mode) && (isprint(c) || isspace(c)))
			environment_call(CALL_INSERTSTRING);
		/* either append the digit to the number or try to execute a bind
		 * and render status bar
		 */
		attrset(0);
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
		printw("%s ", comp_getclassname(focus_comp));
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
