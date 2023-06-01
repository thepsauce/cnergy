#ifndef INCLUDED_BIND_H
#define INCLUDED_BIND_H

// bind.c
/**
 * Binds are always defined in .cng files and allow the user to customize the bindings to their heart's content.
 * This also means that there need to be an abstract layer in between the user and the editor which here is the bindining_call
 * which each window type defines for its own although there are also global binding calls
 * For further information, see parser.h
 */

typedef enum {
	BIND_CALL_NULL,
	// general bindings calls, these have a default behavior which cannot be overwritten but only extended
	BIND_CALL_STARTLOOP,
	BIND_CALL_ENDLOOP,
	BIND_CALL_REGISTER,
	BIND_CALL_ASSERT,
	BIND_CALL_SETMODE,
	BIND_CALL_VSPLIT,
	BIND_CALL_HSPLIT,
	BIND_CALL_COLORWINDOW,
	BIND_CALL_OPENWINDOW,
	BIND_CALL_CLOSEWINDOW,
	BIND_CALL_NEWWINDOW,
	BIND_CALL_MOVEWINDOW_RIGHT,
	BIND_CALL_MOVEWINDOW_BELOW,
	BIND_CALL_QUIT,
	// these have no default behavior and it solely depends on the window type what the behavior is
	BIND_CALL_MOVECURSOR,
	BIND_CALL_MOVEHORZ,
	BIND_CALL_MOVEVERT,
	BIND_CALL_INSERT,
	BIND_CALL_DELETE,
	BIND_CALL_DELETELINE,
	BIND_CALL_DELETESELECTION,
	BIND_CALL_COPY,
	BIND_CALL_PASTE,
	BIND_CALL_UNDO,
	BIND_CALL_REDO,
	BIND_CALL_WRITEFILE,
	BIND_CALL_READFILE,

	BIND_CALL_CHOOSE,
	BIND_CALL_TOGGLEHIDDEN,
	BIND_CALL_TOGGLESORTTYPE,
	BIND_CALL_TOGGLESORTREVERSE,
	BIND_CALL_SORTMODIFICATIONTIME,
	BIND_CALL_SORTALPHABETICAL,
	BIND_CALL_SORTCHANGETIME,
} bind_call_t;

typedef enum {
	FBIND_CALL_USENUMBER = 1 << 0,
	FBIND_CALL_AND = 1 << 1,
	FBIND_CALL_OR = 1 << 2,
	FBIND_CALL_XOR = 1 << 4,
} bind_flags_t;

struct binding_call {
	bind_call_t type;
	bind_flags_t flags;
	ssize_t param;
	union {
		void *ptr;
		char *str;
	};
};

struct binding {
	small_size_t nKeys;
	small_size_t nCalls;
	int *keys;
	struct binding_call *calls;
};

enum {
	FBIND_MODE_SUPPLEMENTARY = 1 << 0,
	FBIND_MODE_SELECTION = 1 << 1,
	FBIND_MODE_TYPE = 1 << 2,
};

struct binding_mode {
	char name[64];
	unsigned flags;
	small_size_t nBindings;
	struct binding *bindings;
	struct binding_mode *next;
};

extern struct binding_mode *all_modes[WINDOW_MAX];

int modes_add(struct binding_mode *modes[WINDOW_MAX]);
int bind_exec(const int *keys, ssize_t param);

#endif
