#include <ncurses.h>

#define COLOR_TABLE_WIDTH 8

int gruvbox_colors[16][3] = {
    {40, 40, 40},   // 0: Black
    {204, 36, 29},  // 1: Red
    {152, 151, 26}, // 2: Green
    {215, 153, 33}, // 3: Yellow
    {69, 133, 136}, // 4: Blue
    {177, 98, 134}, // 5: Magenta
    {104, 157, 106}, // 6: Cyan
    {204, 204, 204}, // 7: Light gray
    {55, 54, 54},   // 8: Dark gray
    {251, 73, 52},  // 9: Bright red
    {184, 187, 38}, // 10: Bright green
    {250, 189, 47}, // 11: Bright yellow
    {131, 165, 152}, // 12: Bright blue
    {211, 134, 155}, // 13: Bright magenta
    {108, 181, 131}, // 14: Bright cyan
    {254, 254, 254} // 15: White
};

int main() {
    initscr(); // Initialize the ncurses screen

    if (!has_colors()) {
        endwin();
        printf("Your terminal does not support colors.\n");
        return 1;
    }

    start_color(); // Enable color support
	
	for (int i = 0; i < 16; i++) {
        init_color(i, gruvbox_colors[i][0] * 1000 / 256, gruvbox_colors[i][1] * 1000 / 256, gruvbox_colors[i][2] * 1000 / 256);
    }

    int num_colors = COLORS; // Get the number of colors supported by the terminal

    int num_rows = (num_colors + COLOR_TABLE_WIDTH - 1) / COLOR_TABLE_WIDTH;
    int num_cols = (num_colors < COLOR_TABLE_WIDTH) ? num_colors : COLOR_TABLE_WIDTH;

    printw("Terminal Colors\n\n");

    for (int row = 0; row < num_rows; row++) {
        for (int col = 0; col < num_cols; col++) {
            int color_index = row * COLOR_TABLE_WIDTH + col;

            if (color_index < num_colors) {
                init_pair(color_index + 1, color_index, 236); // Initialize color pairs

                attron(COLOR_PAIR(color_index + 1)); // Turn on the color pair
                printw("%-5d", color_index + 1);
                attroff(COLOR_PAIR(color_index + 1)); // Turn off the color pair
            } else {
                printw("   "); // Empty space for incomplete rows
            }
        }

        printw("\n");
    }

    refresh(); // Update the screen

    getch(); // Wait for a key press

    endwin(); // Clean up the ncurses environment

    return 0;
}

