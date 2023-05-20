#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

int main(int argc, char** argv) {
    Display* display;
    Window window;
    char* text = "Hello, world!";
    Atom clipboard;
    int result;

    // Open the display and create a window
    display = XOpenDisplay(NULL);
    window = XCreateSimpleWindow(display, RootWindow(display, 0), 0, 0, 1, 1, 0, 0, 0);

    // Register the clipboard selection
    clipboard = XInternAtom(display, "CLIPBOARD", False);
    XSetSelectionOwner(display, clipboard, window, CurrentTime);

    // Set the clipboard text
    XStoreBytes(display, text, strlen(text));

    // Check if the clipboard was successfully set
    result = XGetSelectionOwner(display, clipboard);
    if (result != window) {
        printf("Failed to set clipboard text\n");
        return 1;
    }

    // Close the display and free resources
    XCloseDisplay(display);
    return 0;
}
