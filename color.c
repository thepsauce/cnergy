#include "cnergy.h"

int next_pair = 1;

static inline void
color_renderchooser(struct window *win)
{
	static const int artofawidth = 7;
	static const char *artofa =
		"\n   #   \n"
		  "  # #  \n"
		  " #   # \n"
		  " # # # \n"
		  " #   # \n"
		  " #   # ";
	struct window_color *const color = &win->color;
	struct window_flags_color *const flags = &win->flags.color;
	int lines, cols;
	int y, x;
	int sx;

	attr_set(0, 0, NULL);
	mvaddstr(win->line, win->col, flags->choosebg ? "Choose background" : "Choose foreground");
	cols = COLORS > 64 ? 32 : COLORS > 32 ? 16 : COLORS > 16 ? 8 : 4;
	lines = COLORS / cols;
	for(int y = 0; y < lines; y++)
		for(int x = 0; x < cols; x++) {
			const int i = y * cols + x;
			init_pair(i, i, COLOR_BLACK);
			attr_set(0, i, NULL);
			if(i == color->selected)
				attr_on(A_REVERSE, NULL);
			move(y + win->line + 1, x + win->col);
			addch('A');
		}
	sx = win->col;
	for(int i = 0; i < 3; i++) {
		switch(i) {
		case 0:
			attr_set(0, color->fgsel, NULL);
			break;
		case 1:
			init_pair(COLORS, color->fgsel, color->bgsel);
			attr_set(0, COLORS, NULL);
			break;
		case 2:
			attr_set(0, color->bgsel, NULL);
			break;
		}
		y = win->line + 1 + lines;
		for(const char *a = artofa; *a; a++) {
			if(*a == '\n') {
				y++;
				x = sx;
				continue;
			}
			mvaddch(y, x, *a);
			x++;
		}
		sx += artofawidth + 1;
	}
}

static inline int
color_rendernormal(struct window *win)
{
	static const int attrs[] = {
		A_STANDOUT,
		A_UNDERLINE,
		A_REVERSE,
		A_BLINK,
		A_DIM,
		A_BOLD,
		0,
	};

	for(int i = 0; i < next_pair; i++) {
		if(i >= win->lines)
			break;
		const int y = win->line + i;
		move(y, win->col);
		attrset(COLOR_PAIR(i));
		for(int j = 0; j < ARRLEN(attrs); j++) {
			if(j)
				attroff(attrs[j - 1]);
			attron(attrs[j]);
			addch('A');
		}
	}
	return 0;
}

bool
color_call(windowid_t winid, call_t call)
{
	struct window *const win = all_windows + winid;
	struct window_color *const color = &win->color;
	struct window_flags_color *const flags = &win->flags.color;
	switch(call) {
	case CALL_RENDER:
		if(flags->choosefg || flags->choosebg)
			color_renderchooser(win);
		else
			color_rendernormal(win);
		break;
	case CALL_MOVECURSOR:
		color->selected += main_environment.A;
		color->selected = (unsigned) color->selected % COLORS;
		break;
	case CALL_CHOOSE:
		if(flags->choosefg)
			color->fgsel = color->selected;
		else if(flags->choosebg)
			color->bgsel = color->selected;
		else {
			flags->choosefg = true;
			break;
		}
		break;
	case CALL_SWAP:
		if(flags->choosefg || flags->choosebg) {
			flags->choosefg = !flags->choosefg;
			flags->choosebg = !flags->choosebg;
		}
		break;
	case CALL_MOVEVERT:
		color->selected += SAFE_MUL(main_environment.A, 32);
		color->selected = (unsigned) color->selected % COLORS;
		break;
	}
	return false;
}
