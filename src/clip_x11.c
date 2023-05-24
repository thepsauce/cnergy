#include "cnergy.h"

#if _PLATFORM == _X11

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <pthread.h>
#include <unistd.h>

static Display *display;
static Window window;
static Atom clipboard;
static Atom utf8_string;
static pthread_t x11_thread_id;
static pthread_mutex_t clipboard_mutex = PTHREAD_MUTEX_INITIALIZER;
static char *clipboard_data;
static pthread_cond_t clipboard_paste_cond = PTHREAD_COND_INITIALIZER;
static char *clipboard_paste_data;

void *
x11_thread(void *arg)
{
	(void) arg;

	XEvent event;
	XTextProperty prop;

	while(1) {
		if(XPending(display) <= 0) {
			usleep(1000);
			continue;
		}
		pthread_mutex_lock(&clipboard_mutex);
		XNextEvent(display, &event);
		if(event.type == SelectionRequest) {
			XEvent response;
			XSelectionRequestEvent request;

			request = event.xselectionrequest;
			if(request.property == None)
				request.property = request.target;
			if(request.target == XA_STRING || request.target == utf8_string) {
				XStringListToTextProperty(&clipboard_data, 1, &prop);
				XChangeProperty(request.display, request.requestor,
								 request.property, request.target, 8,
								 PropModeReplace, (unsigned char*) prop.value,
								 prop.nitems);
				XFree(prop.value);
			}
			response.xselection.type = SelectionNotify;
			response.xselection.display = request.display;
			response.xselection.requestor = request.requestor;
			response.xselection.selection = request.selection;
			response.xselection.target = request.target;
			response.xselection.time = request.time;
			response.xselection.property = request.property;
			XSendEvent(request.display, request.requestor, False, 0, &response);
		} else if(event.type == SelectionNotify) {
			if(event.xselection.property != None) {
				XGetTextProperty(display, window, &prop, utf8_string);
				if(prop.value && prop.encoding == utf8_string) {
					clipboard_paste_data = malloc(prop.nitems + 1);
					if(clipboard_paste_data)
						strcpy(clipboard_paste_data, (const char*) prop.value);
				}
				XFree(prop.value);
			}
			pthread_cond_signal(&clipboard_paste_cond);
		}
		pthread_mutex_unlock(&clipboard_mutex);
	}
}

void __attribute__((constructor))
init(void)
{
	int screen;

	display = XOpenDisplay(NULL);
	screen = DefaultScreen(display);
	window = XCreateSimpleWindow(display, RootWindow(display, screen), 0, 0, 1, 1, 0, 0, 0);
	clipboard = XInternAtom(display, "CLIPBOARD", False);
	utf8_string = XInternAtom(display, "UTF8_STRING", False);
	pthread_create(&x11_thread_id, NULL, x11_thread, NULL);
}

void __attribute__((destructor))
uninit(void)
{
	XCloseDisplay(display);
}

int
clipboard_copy(const char *text, U32 nText)
{
	pthread_mutex_lock(&clipboard_mutex);
	XSetSelectionOwner(display, clipboard, window, CurrentTime);
	const Window result = XGetSelectionOwner(display, clipboard);
	if(result != window)
		return -1;
	clipboard_data = realloc(clipboard_data, nText + 1);
	memcpy(clipboard_data, text, nText);
	clipboard_data[nText] = 0;
	pthread_mutex_unlock(&clipboard_mutex);
	return 0;
}

int
clipboard_paste(char **pText)
{
	pthread_mutex_lock(&clipboard_mutex);
	XConvertSelection(display, clipboard, utf8_string, None, window, CurrentTime);
	pthread_cond_wait(&clipboard_paste_cond, &clipboard_mutex);
	pthread_mutex_unlock(&clipboard_mutex);
	*pText = clipboard_paste_data;
	if(!clipboard_paste_data)
		return -1;
	return 0;
}

#endif
