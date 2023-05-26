#include "cnergy.h"
#include <locale.h>

int print_modes(struct binding_mode *mode, int);

int
main(int argc, char **argv)
{
	setlocale(LC_ALL, "");

	// all below here is pretty much test code
	FILE *f;

	for(int i = 0; i < 1; i++) {
		f = fopen("draft.cng", "r");

		const int r = bind_parse(f);
		if(r) {
			printf("error parsing: %d\n", r);
			return -1;
		}
		fclose(f);
		print_modes(NULL, 0);
	}

	initscr();

	raw();
	noecho();
	keypad(stdscr, true);
	scrollok(stdscr, false);
	clearok(stdscr, false);
	set_tabsize(4);

	if(has_colors()) {
		static const U8 gruvbox_colors[16][3] = {
			{ 40, 40, 40 },   // 1: Black
			{ 204, 36, 29 },  // 2: Red
			{ 152, 151, 26 }, // 3: Green
			{ 215, 153, 33 }, // 4: Yellow
			{ 69, 133, 136 }, // 5: Blue
			{ 177, 98, 134 }, // 6: Magenta
			{ 104, 157, 106 }, // 7: Cyan
			{ 204, 204, 204 }, // 8: Light gray
			{ 55, 54, 54 },   // 9: Dark gray
			{ 251, 73, 52 },  // 10: Bright red
			{ 184, 187, 38 }, // 11: Bright green
			{ 250, 189, 47 }, // 12: Bright yellow
			{ 131, 165, 152 }, // 13: Bright blue
			{ 211, 134, 155 }, // 14: Bright magenta
			{ 108, 181, 131 }, // 15: Bright cyan
			{ 254, 254, 254 } // 16: White
		};
		start_color();
		for(U32 i = 0; i < ARRLEN(gruvbox_colors); i++)
			init_color(i,
					(U32) gruvbox_colors[i][0] * 1000 / 256,
					(U32) gruvbox_colors[i][1] * 1000 / 256,
					(U32) gruvbox_colors[i][2] * 1000 / 256);
		for(U32 i = 0; i < 16; i++)
		for(U32 j = 0; j < 16; j++)
			init_pair(1 + i + j * 16, i, j);
	}

	int keys[32];
	U16 nKeys = 0;
	I32 num = 0;

	// setup some windows for testing
	struct window *w;
	struct buffer *b;
	FILE *fp;
	// 0 right
	// 1 below 
	int path[] = {
		0, 0, 1, 0
	};
	const char *files[] = {
		"src/window.c",
		"include/cnergy.h",
		"src/bind.c",
	};
	fp = fopen("example_utf8.txt", "r");
	b = buffer_new(fp);
	fclose(fp);
	w = window_new(b);
	first_window = w;
	focus_window = w;
	for(U32 i = 0; i < ARRLEN(path); i++) {
		fp = fopen(files[i % ARRLEN(files)], "r");
		b = buffer_new(fp);
		fclose(fp);
		struct window *const new = window_new(b);
		if(path[i])
			window_attach(w, new, ATT_WINDOW_VERTICAL);
		else
			window_attach(w, new, ATT_WINDOW_HORIZONTAL);
		w = new;
	}

	while(1) {
		int c;

		if(!focus_window)
			break;
		// -1 to reserve a line to show key presses
		first_window->line = 0;
		first_window->col = 0;
		first_window->lines = LINES - 1;
		first_window->cols = COLS;
		window_layout(first_window);
		for(U32 i = n_windows; i > 0;)
			window_render(all_windows[--i]);
		if(!mode_has(FBIND_MODE_SELECTION)) {
			curs_set(1);	
			move(focus_y, focus_x);
		} else
			curs_set(0);
		c = getch();
		if(c == 0x03) // ^C
			break;
		switch(c) {
		case 0x7f: // delete
			c = KEY_BACKSPACE;
			break;
		case 0x1b: // escape
			if(num || nKeys) {
				num = 0;
				nKeys = 0;
				continue;
			}
			break;
		}
		if(mode_has(FBIND_MODE_TYPE) && (isprint(c) || isspace(c))) {
			char b[10];

			b[0] = c;
			// get length of the following utf8 character 
			const U32 len = (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : (c & 0xf8) == 0xf0 ? 4 : 1;
			for(U32 i = 1; i < len; i++)
				b[i] = getch();
			buffer_insert(focus_window->buffers[focus_window->iBuffer], b, len);
		}
		attrset(COLOR(3, 0));
		mvprintw(LINES - 1, 0, "%s ", mode_name());
		if(!nKeys) {
			if(isdigit(c) && (c != '0' || num)) {
				num = SAFE_MUL(num, 10);
				num = SAFE_ADD(num, c - '0');
				if(num) {
					printw("%d", num);
					printw("%*s", COLS - getcurx(stdscr), "");
				}
				continue;
			}
		}
		keys[nKeys] = c;
		if(num)
			printw("%d", num);
		for(U32 i = 0; i <= nKeys; i++)
			printw("%s", keyname(keys[i]));
		printw("%*s", COLS - getcurx(stdscr), "");
		if(nKeys + 1 < ARRLEN(keys))
			nKeys++;
		keys[nKeys] = 0;
		if(exec_bind(keys, num ? num : 1) != 1) {
			nKeys = 0;
			num = 0;
		}
	}
	endwin();
	printf("normal exit\n");
	return 0;
}
