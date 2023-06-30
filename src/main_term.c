#ifndef GUI

#include "cnergy.h"
#include <locale.h>

int
main(int argc, char **argv)
{
	(void) argc;
	(void) argv;

	setlocale(LC_ALL, "");

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
	settings_tabsize = 4;

	char keys[1024];
	ptrdiff_t num = 0;
	size_t nKeys = 0;

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

	// uncomment this code to add a fileviewer
	window_new(WINDOW_FILEVIEWER);

	window_setposition(0, 0, 0, LINES / 2, COLS);
	window_setposition(1, LINES / 2, 0, LINES - LINES / 2, COLS / 2);
	window_setposition(2, LINES / 2, COLS / 2, LINES - LINES / 2, COLS - COLS / 2);

	/* stack pointer grows downwards */
	while(1) {
		int c;
		char *sk;
		bool isPreDigit;

		/* -1 to reserve a line to show the global status bar */
		window_displayall(0, 0, LINES - 1, COLS);
		if(cur_mode->flags & FMODE_SELECTION) {
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
			if(num || nKeys) {
				num = 0;
				nKeys = 0;
				continue;
			}
			break;
		}
		isPreDigit = !(cur_mode->flags & FMODE_TYPE) && isdigit(c) && (c != '0' || num);
		sk = keys + nKeys;
		if(c > 0xff) {
			c = SPECIAL_KEY(c);
			nKeys += utf8_convunicode(c, keys + nKeys);
		} else if(!isPreDigit) {
			keys[nKeys++] = c;
			while(c & 0x80) {
				c = getch();
				keys[nKeys++] = c;
			}
		}
		keys[nKeys] = 0;
		if((cur_mode->flags & FMODE_TYPE) && (isprint(c) || isspace(c))) {
		}
		/* either append the digit to the number or try to execute a bind
		 * render status bar
		 */
		attrset(COLOR(3, 0));
		move(LINES - 1, 0);
		if(isPreDigit) {
			num = SAFE_MUL(num, 10);
			num = SAFE_ADD(num, c - '0');
		} else {
		}
		if(num)
			printw("%zd", num);
		for(unsigned i = 0; i < nKeys; ) {
			const char *const utf8 = keys + i;
			const unsigned l = utf8_len(utf8, nKeys - i);
			const int uc = utf8_tounicode(utf8, nKeys - i);
			if(uc >= 0xf6b1 && uc <= 0xf6b1 + 0xff)
				printw("<%s>", keyname(uc - 0xf6b1 + 0xff) + 4);
			else if(uc <= 0x7f)
				printw("%s", keyname(uc));
			else
				addnstr(utf8, l);
			i += l;
		}
		ersline(COLS - getcurx(stdscr));
	}
	endwin();
	printf("^C exit\n");
	return 0;
}

#endif
