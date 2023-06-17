#include "cnergy.h"

bool edit_event(windowid_t winid, struct event *ev);
bool bufferviewer_event(windowid_t winid, struct event *ev);
bool fileviewer_event(windowid_t winid, struct event *ev);

bool (*window_types[])(windowid_t winid, struct event *ev) = {
	[WINDOW_EDIT] = edit_event,
	[WINDOW_BUFFERVIEWER] = bufferviewer_event,
	[WINDOW_FILEVIEWER] = fileviewer_event,
};

// TODO: when this list is complete, add binary search or hashing (same goes for translate_key)
// Note that this is used by the parser (parser_key_call.c)
const struct event_info infoEvents[EVENT_MAX] = {
	[EVENT_COLORWINDOW] = { "COLOR", 0 },
	[EVENT_CLOSEWINDOW] = { "CLOSE", 0 },
	[EVENT_NEWWINDOW] = { "NEW", 0 },
	[EVENT_OPENWINDOW] = { "OPEN", 0 },
	[EVENT_HSPLIT] = { "HSPLIT", 0 },
	[EVENT_VSPLIT] = { "VSPLIT", 0 },
	[EVENT_QUIT] = { "QUIT", 0 },

	[EVENT_DELETESELECTION] = { "DELETE", 0 },
	[EVENT_COPY] = { "COPY", 0 },
	[EVENT_PASTE] = { "PASTE", 0 },
	[EVENT_UNDO] = { "UNDO", 0 },
	[EVENT_REDO] = { "REDO", 0 },
	[EVENT_WRITEFILE] = { "WRITE", 0 },
	[EVENT_READFILE] = { "READ", 0 },

	[EVENT_FIND] = { "FIND", 1 },

	[EVENT_CHOOSE] = { "CHOOSE", 0 },
	[EVENT_SORTALPHABETICAL] = { "SORT_ALPHABETICAL", 0 },
	[EVENT_SORTCHANGETIME] = { "SORT_CHANGETIME", 0 },
	[EVENT_SORTMODIFICATIONTIME] = { "SORT_MODIFICATIONTIME", 0 },
	[EVENT_TOGGLESORTTYPE] = { "TOGGLE_SORT_TYPE", 0 },
	[EVENT_TOGGLESORTREVERSE] = { "TOGGLE_SORT_REVERSE", 0 },
	[EVENT_TOGGLEHIDDEN] = { "TOGGLE_HIDDEN", 0 },
};

bool
event_recursiverender(windowid_t winid, struct event *ev)
{
	/* Do not need to render empty windows */
	if(window_getarea(winid) != 0)
		(*window_types[window_gettype(winid)])(winid, ev);
	if(window_getbelow(winid) != ID_NULL)
		event_recursiverender(window_getbelow(winid), ev);
	if(window_getright(winid) != ID_NULL)
		event_recursiverender(window_getright(winid), ev);
	return true;
}

bool
event_dispatch(struct event *ev)
{
	switch(ev->type) {
	case EVENT_RENDER:
		return event_recursiverender(focus_window, ev);
	default:
		return (*window_types[window_gettype(focus_window)])
			(focus_window, ev);
	}
}
