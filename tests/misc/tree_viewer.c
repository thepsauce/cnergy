#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ncurses.h>
#include <unistd.h>

// Function to recursively traverse and display the directory tree
void traverseDirectory(const char *path, int indent)
{
    DIR *dir;
    struct dirent *entry;

    // Open the directory
    dir = opendir(path);
    if (dir == NULL)
        return;

    // Read directory entries
    while ((entry = readdir(dir)) != NULL)
    {
        // Skip "." and ".." directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Print indentation
        for (int i = 0; i < indent; i++)
            printw("    ");

        // Print the entry name
        printw("%s\n", entry->d_name);

        // Check if the entry is a directory
        if (entry->d_type == DT_DIR)
        {
            // Recursively traverse the subdirectory
            char subPath[256];
            snprintf(subPath, sizeof(subPath), "%s/%s", path, entry->d_name);
            traverseDirectory(subPath, indent + 1);
        }
    }

    // Close the directory
    closedir(dir);
}

int main()
{
    // Initialize ncurses
    initscr();
    keypad(stdscr, TRUE);
    noecho();
    curs_set(0);

    // Get the current working directory
    char path[256];
    getcwd(path, sizeof(path));

    // Traverse and display the directory tree
    traverseDirectory(path, 0);

    // Wait for user input
    getch();

    // Clean up and exit
    endwin();
    return 0;
}
